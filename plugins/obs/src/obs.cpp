#include "obs.hpp"
#include <shellapi.h>
#include "shared/base64_util.hpp"
#include "shared/sha256_util.hpp"
#include "shared/shell_util.hpp"
#include "fmt/format.h"
#include <cassert>

#define ADD_HANDLER(evt) \
  event_handlers_.emplace(#evt, [this](auto&& d) { return evt(d); })

#define ADD_COMMAND(cmd) commands_[#cmd] = [this](auto&& json) { cmd(json); }

namespace network {
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

ObsWebClient::ObsWebClient(std::filesystem::path data_dir,
    std::string host,
    unsigned port,
    std::string password)
    : data_dir_(std::move(data_dir))
    , host_(std::move(host))
    , port_(port)
    , password_(std::move(password)) {
  assert(!host_.empty());
  assert(port_ > 0);

  client_.init_asio();

  client_.clear_access_channels(websocketpp::log::alevel::all);
  client_.set_access_channels(websocketpp::log::elevel::info);

  client_.set_message_handler(bind(&ObsWebClient::OnMessage, this, _1, _2));

  ADD_HANDLER(StreamStateChanged);
  ADD_HANDLER(ReplayBufferStateChanged);

  ADD_COMMAND(OpenConfigFile);
}

ObsWebClient::~ObsWebClient() {
  OutputDebugStringA(__FUNCTION__);
  client_.stop();
  runner_.join();
}

bool ObsWebClient::Start(message_handler_t on_message) {
  if (on_message)
    on_message_ = std::move(on_message);

  client_.set_open_handler(bind(&ObsWebClient::OnOpen, this, _1));
  client_.set_fail_handler(bind(&ObsWebClient::OnFail, this, _1));
  client_.set_message_handler(bind(&ObsWebClient::OnMessage, this, _1, _2));
  client_.set_close_handler(bind(&ObsWebClient::OnClose, this, _1));

  // Create a connection to the given URI and queue it for connection once
  // the event loop starts
  std::string const uri = "ws://" + host_ + ":" + std::to_string(port_);
  websocketpp::lib::error_code ec;
  client_t::connection_ptr con = client_.get_connection(uri, ec);
  client_.connect(con);

  // Start the ASIO io_service run loop
  runner_ = std::thread([&] {
    try {
      client_.run();
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  });
  return true;
}

bool ObsWebClient::Send(connection_hdl hdl, const char* data, size_t size) {
  try {
    OutputDebugStringA(("Sending " + std::string(data, size)).c_str());
    client_.send(hdl, data, size, websocketpp::frame::opcode::TEXT);
    return true;
  } catch (...) {
    std::cout << "Cannot send " << size << " bytes" << std::endl;
    return false;
  }
}

bool ObsWebClient::Request(std::string const& cmd,
    nlohmann::json const& params) {
  if (auto&& it = commands_.find(cmd); it != commands_.end()) {
    it->second(params);
    return true;
  }

  /*
  {
    "op": 6,
    "d": {
      "requestType": "SetCurrentProgramScene",
      "requestId": "f819dcf0-89cc-11eb-8f0e-382c4ac93b9c",
      "requestData": {
        "sceneName": "Scene 12"
      }
    }
  }
  */
  try {
    nlohmann::json j;
    j["op"] = 6;
    j["d"]["requestType"] = cmd;
    j["d"]["requestId"] = std::to_string(request_id_++);
    j["d"]["requestId"] = nlohmann::json::object();
    for (auto& [k, v] : params.items()) {
      j["d"]["requestData"][k] = v;
    }
    client_.send(server_, j.dump(), websocketpp::frame::opcode::TEXT);
    return true;
  } catch (...) {
    OutputDebugStringW(L"Error sending request to server!");
  }
  return false;
}

void ObsWebClient::Shutdown() {
  client_.stop_listening();
  client_.stop();
}

void ObsWebClient::OnOpen(connection_hdl hdl) {
  server_ = hdl;
  std::cout << "Server connection opened" << std::endl;
}

void ObsWebClient::OnFail(connection_hdl hdl) {
  std::cout << "Server connection failed" << std::endl;
}

void ObsWebClient::OnClose(connection_hdl hdl) {
  std::cout << "Server connection closed" << std::endl;
}

bool ObsWebClient::StreamStateChanged(nlohmann::json const& event_data) {
  state_.streaming = event_data["outputActive"];
  if (!state_.streaming)
    return true;

  StopReplayBuffer();
  return true;
}

bool ObsWebClient::ReplayBufferStateChanged(nlohmann::json const& event_data) {
  state_.replay_buffer = event_data["outputActive"];
  if (!state_.replay_buffer)
    return true;

  StopStream();
  return true;
}

void ObsWebClient::StopStream() {
  if (state_.streaming)
    Request("StopStream");
}

void ObsWebClient::StopReplayBuffer() {
  if (state_.replay_buffer)
    Request("StopReplayBuffer");
}

bool ObsWebClient::HandleEvent(nlohmann::json const& json) {
  std::string const event_type = json["d"]["eventType"];
  auto const event_data = json["d"]["eventData"];
  auto it = event_handlers_.find(event_type);
  if (it == event_handlers_.end())
    return true;

  return it->second(event_data);
}

bool ObsWebClient::HandleResponse(connection_hdl hdl,
    client_t::message_ptr msg) {
  nlohmann::json r;
  try {
    std::string const& m = msg->get_payload();
    OutputDebugStringA(m.c_str());

    auto j = nlohmann::json::parse(m);
    int op = j["op"];
    switch (op) {
      case 0:  // Hello
      {
        r["op"] = 1;
        r["d"]["rpcVersion"] = j["d"]["rpcVersion"];
        if (j["d"].contains("authentication"))
          r["d"]["authentication"] = GeneratePaswordHash(
              j["d"]["authentication"]);
        // General (1)
        // Config (2)
        // Outputs (64)
        r["d"]["eventSubscriptions"] = 67;

        std::string const& rm = r.dump();
        Send(hdl, rm.c_str(), rm.size());
        return true;
      }
      case 5:  // Event
        return HandleEvent(j);
    }
  } catch (...) {
    OutputDebugStringW(L"Error processing response");
  }
  return false;
}

void ObsWebClient::StartReplayBuffer() {
  static_cast<void>(Request("StartReplayBuffer"));
}

std::string ObsWebClient::GeneratePaswordHash(
    nlohmann::json const& payload) const {
  /*
  Concatenate the websocket password with the salt provided by the server
  (password + salt) Generate an SHA256 binary hash of the result and base64
  encode it, known as a base64 secret. Concatenate the base64 secret with the
  challenge sent by the server (base64_secret + challenge) Generate a binary
  SHA256 hash of that result and base64 encode it. You now have your
  authentication string.
  */
  std::string const challenge = payload["challenge"];
  std::string const salt = payload["salt"];
  std::string const h = util::Base64Encode(
      util::String2SHA256Sum(password_ + salt)) + challenge;
  return util::Base64Encode(util::String2SHA256Sum(h));
}

void ObsWebClient::OnMessage(connection_hdl hdl, client_t::message_ptr msg) {
  if (HandleResponse(hdl, msg))
    return;

  if (on_message_)
    on_message_(hdl, msg.get()->get_payload());
}

void ObsWebClient::OpenConfigFile(nlohmann::json const& params) {
  if (!params.contains("file") || !params["file"].is_string())
    return;

  std::error_code ec;
  auto file = data_dir_ / params["file"];
  if (!std::filesystem::exists(file, ec))
    return;

  auto url = L"file://" + file.wstring();
  static_cast<void>(util::OpenViaShell(url));
}
}  // namespace network
