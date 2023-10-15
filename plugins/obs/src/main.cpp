#include "shared/platform.hpp"
#include "shared/string_util.h"
#include "shared/widget_plugin.h"
#include "shared/logger.hpp"
#include "obs.hpp"
#include "nlohmann/json.hpp"
#include "fmt/format.h"
#include <iostream>
#include <string>
#include <thread>
#include <fstream>
#include <filesystem>
#include <map>
#include <future>

namespace {
constexpr wchar_t kConfigFile[]{ L"obs.json" };

bool debug = false;
bool init = false;
struct {
  std::filesystem::path data_dir;
  std::string host;
  int port{};
  std::string password;
  bool stop_replay_on_streaming{};
} obs_config;
std::unique_ptr<network::ObsWebClient> obs;
std::promise<void> cancel_stop_buffer;
std::shared_future<void> future;

void StartObsWebSocketClient() {
  LOG(INFO) << "Starting ObsWebSocket client using IP: " << obs_config.host
            << " port: " << obs_config.port;
  obs = std::make_unique<network::ObsWebClient>(obs_config.data_dir,
      obs_config.host, obs_config.port, obs_config.password);
  obs->Start();
}
}  // namespace

// Begin exported functions
bool DECLDLL PLUGIN InitPlugin(const std::filesystem::path& data_dir,
    bool debug_mode) {
  LOG(INFO) << __FUNCTION__;
  auto config_file = data_dir / kConfigFile;
  std::error_code ec;
  if (!std::filesystem::exists(config_file, ec))
    return false;

  try {
    std::ifstream f(config_file);
    if (!f.good())
      return false;

    auto cfg = nlohmann::json::parse(f);
    std::string host = cfg["host"];
    int port = cfg["port"];
    std::string pwd = cfg["password"];
    bool stop_replay = cfg.contains("stopReplay") ? cfg["stopReplay"] : true;
    if (host.empty() || port < 0 || port >= 30000 || pwd.empty())
      return false;

    obs_config.data_dir = std::move(data_dir);
    obs_config.host = std::move(host);
    obs_config.port = port;
    obs_config.password = std::move(pwd);
    obs_config.stop_replay_on_streaming = stop_replay;

    StartObsWebSocketClient();
  } catch (...) {
    LOG(ERROR) << "Error parsing config file";
    return false;
  }

  init = true;
  debug = debug_mode;
  return true;
}

std::wstring DECLDLL PLUGIN GetValues(const std::wstring& profile_name) {
  auto const& state = obs->GetOutputState();
  return string2wstring(fmt::format(
      "\"obs=>streaming\":{{\"sensor\":\"streaming\",\"value\":{}}}",
      state.streaming ? "true" : "false"));
}

void DECLDLL PLUGIN ShutdownPlugin() {
  LOG(INFO) << __FUNCTION__;
  if (init) {
    init = false;

  }
}

bool DECLDLL PLUGIN ExecuteCommand(const std::string& command) {
  if (command.empty())
    return false;

  auto const json = nlohmann::json::parse(command);
  std::string cmd = json["command"];
  const auto execute_cmd = [&] {
    try {
      return obs->Request(cmd, json["params"]);
    } catch (...) {
      return false;
    }
  };
  for (int retry = 0; retry < 3; retry++) {
    if (execute_cmd())
      return true;

    StartObsWebSocketClient();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return false;
}

void DECLDLL PLUGIN ProfileChanged(const std::string& pname) {
#if !defined(START_REPLAY_UPON_GAME_LAUNCH)
  return;
#else
  if (obs == nullptr)
    return;

  if (pname.empty() && obs->GetOutputState().replay_buffer) {
    bool cancel{};
    if (!future.valid()) {
      obs->Request("StopReplayBuffer");
    } else {
      std::thread wait([&, fut = future] {
        auto res = fut.wait_for(std::chrono::seconds(15));
        if (res == std::future_status::timeout) {
          LOG(INFO) << "Stopping replay buffer";
          obs->StopReplayBuffer();
        } else {
          LOG(INFO) << "Previous stop replay request cancelled";
        }
      });
      wait.detach();
    }

    return;
  }

  if (obs->GetOutputState().streaming) {
    LOG(INFO) << "Streaming. Don't start replay buffer";
  } else {
    LOG(INFO) << "Starting replay buffer";
    obs->StartReplayBuffer();
  }

  cancel_stop_buffer = std::promise<void>();
  future = cancel_stop_buffer.get_future();
#endif
}
// End exported functions

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
  return TRUE;
}
