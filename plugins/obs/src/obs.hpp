#pragma once
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace network {
typedef websocketpp::client<websocketpp::config::asio_client> client_t;
using websocketpp::connection_hdl;
using message_handler_t = std::function<void(connection_hdl hdl,
    const std::string&)>;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

using event_handler_t = std::function<bool(nlohmann::json const&)>;

struct OutputState {
  bool replay_buffer{};
  bool streaming{};
};

class ObsWebClient {
public:
  ObsWebClient() = delete;

  ObsWebClient(std::filesystem::path data_dir,
      std::string host,
      unsigned port,
      std::string password);
  ~ObsWebClient();

  bool Start(message_handler_t on_message = nullptr);
  bool Send(connection_hdl hdl, const char* data, size_t size);
  void Shutdown();

  bool HandleEvent(nlohmann::json const& json);
  bool HandleResponse(connection_hdl hdl, client_t::message_ptr msg);
  bool Request(std::string const& cmd,
      nlohmann::json const& params = nlohmann::json::object());

  void StartReplayBuffer();
  void StopReplayBuffer();

  void StopStream();

  OutputState const& GetOutputState() const {
    return state_;
  }

private:
  void OnOpen(connection_hdl hdl);
  void OnFail(connection_hdl hdl);
  void OnClose(connection_hdl hdl);
  void OnMessage(connection_hdl hdl, message_ptr msg);

  bool StreamStateChanged(nlohmann::json const& event_data);
  bool ReplayBufferStateChanged(nlohmann::json const& event_data);

  std::string GeneratePaswordHash(nlohmann::json const& payload) const;

  std::filesystem::path data_dir_;
  std::thread runner_;
  message_handler_t on_message_;
  std::string host_;
  unsigned port_{};
  std::string password_;
  client_t client_;
  int64_t request_id_{};
  connection_hdl server_;

  std::unordered_map<std::string, event_handler_t> event_handlers_;
  std::unordered_map<std::string, std::function<void(nlohmann::json const&)>>
      commands_;

  void OpenConfigFile(nlohmann::json const& params);

  OutputState state_;
};
}  // namespace network
