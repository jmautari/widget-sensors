#include "shared/platform.hpp"
#include "shared/widget_plugin.h"
#include "core/process_tracker.hpp"
#include "core/process_watcher.hpp"
#include "shared/string_util.h"
#include <string>
#include <thread>

#define DBGOUT(x) if (debug) OutputDebugStringW((x))

namespace {
bool debug = false;
bool init = false;
int64_t elapsed_time{};
std::string current_profile;

core::ProcessWatcher watcher;
core::ProcessTracker tracker;

std::mutex mutex;
}  // namespace

// Begin exported functions
bool DECLDLL PLUGIN InitPlugin(const std::filesystem::path& data_dir,
    bool debug_mode) {
  OutputDebugStringW(__FUNCTIONW__);

  watcher.Start([&](std::string event_type, std::string process_name, int pid) {
    std::unique_lock lock(mutex);
    if (event_type == "started") {
      OutputDebugStringA(
          (process_name + " has started. PID: " + std::to_string(pid)).c_str());
      tracker.Add(pid, std::move(process_name));
    } else {
      auto delta = tracker.GetElapsedTime<std::chrono::seconds>(pid);
      OutputDebugStringA((process_name + " has stopped. Run time " +
                          std::to_string(delta.count()) +
                          " seconds. PID: " + std::to_string(pid))
                             .c_str());
      tracker.Delete(pid);
    }
  });

  init = true;
  debug = debug_mode;
  return true;
}

std::wstring DECLDLL PLUGIN GetValues(const std::wstring& profile_name) {
  if (current_profile.empty()) {
    elapsed_time = {};
  } else {
    auto pid = tracker.GetPidByProcessName(current_profile);
    if (pid != 0) {
      elapsed_time = tracker.GetElapsedTime<std::chrono::seconds>(pid).count();
    }
  }

  return L"\"tracker\":{\"sensor\":\"elapsedTime\",\"value\":" +
         std::to_wstring(elapsed_time) + L"}";
}

void DECLDLL PLUGIN ShutdownPlugin() {
  OutputDebugStringW(__FUNCTIONW__);
  if (init) {
    init = false;

  }
}

void DECLDLL PLUGIN ProfileChanged(const std::string& pname) {
  if (pname.empty()) {
    current_profile.clear();
    return;
  }

  if (auto p = pname.find_last_of('\\'); p != std::string::npos)
    current_profile = pname.substr(p + 1);

  OutputDebugStringA(("Got new profile " + current_profile).c_str());
}
// End exported functions

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
  return TRUE;
}
