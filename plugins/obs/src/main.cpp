#include "shared/platform.hpp"
#include "shared/widget_plugin.h"
#include "obs.hpp"
#include "nlohmann/json.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <fstream>
#include <filesystem>
#include <map>
#include <future>

#define DBGOUT(x) if (debug) OutputDebugStringW((x))

namespace {
constexpr wchar_t kConfigFile[]{ L"obs.json" };

bool debug = false;
bool init = false;
std::unique_ptr<network::ObsWebClient> obs;
std::promise<void> cancel_stop_buffer;
std::shared_future<void> future;
}  // namespace

// Begin exported functions
bool DECLDLL PLUGIN InitPlugin(const std::filesystem::path& data_dir,
    bool debug_mode) {
  OutputDebugStringW(__FUNCTIONW__);
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
    if (host.empty() || port < 0 || port >= 30000 || pwd.empty())
      return false;

    OutputDebugStringA(("Starting ObsWebSocket client using IP: " + host +
                        " port: " + std::to_string(port))
                           .c_str());

    obs = std::make_unique<network::ObsWebClient>(host, port, pwd);
    obs->Start();
  } catch (...) {
    OutputDebugStringW(L"Error parsing config file");
    return false;
  }

  init = true;
  debug = debug_mode;
  return true;
}

std::wstring DECLDLL PLUGIN GetValues(const std::wstring& profile_name) {
  return L"";
}

void DECLDLL PLUGIN ShutdownPlugin() {
  OutputDebugStringW(__FUNCTIONW__);
  if (init) {
    init = false;

  }
}

bool DECLDLL PLUGIN ExecuteCommand(const std::string& command) {
  if (command.empty())
    return false;

  try {
    auto json = nlohmann::json::parse(command);
    std::string cmd = json["command"];
    return obs->Request(cmd, json["params"]);
  } catch (...) {
  }
  return false;
}

void DECLDLL PLUGIN ProfileChanged(const std::string& pname) {
  if (obs == nullptr)
    return;

  if (pname.empty()) {
    bool cancel{};
    if (!future.valid()) {
      obs->Request("StopReplayBuffer");
    } else {
      std::thread wait([&, fut = future] {
        auto res = fut.wait_for(std::chrono::seconds(15));
        if (res == std::future_status::timeout) {
          OutputDebugStringW(L"Stopping replay buffer");
          obs->Request("StopReplayBuffer");
        } else {
          OutputDebugStringW(L"Previous stop replay request cancelled");
        }
      });
      wait.detach();
    }

    return;
  }

  OutputDebugStringW(L"Starting replay buffer");
  obs->Request("StartReplayBuffer");
  cancel_stop_buffer = std::promise<void>();
  future = cancel_stop_buffer.get_future();
}
// End exported functions

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
  return TRUE;
}
