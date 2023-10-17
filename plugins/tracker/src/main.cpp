/**
 * Widget Sensors
 * Process Tracer plug-in
 * Copyright (C) 2021-2023 John Mautari - All rights reserved
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "shared/platform.hpp"
#include "shared/logger.hpp"
#include "shared/widget_plugin.h"
#include "core/process_tracker.hpp"
#include "core/process_watcher.hpp"
#include "shared/string_util.h"
#include <string>
#include <thread>

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
  LOG(INFO) << __FUNCTION__;

  watcher.Start([&](std::string event_type, std::string process_name, int pid) {
    std::unique_lock lock(mutex);
    if (event_type == "started") {
      LOG(INFO) << process_name << " has started. PID: " << pid;
      tracker.Add(pid, std::move(process_name));
    } else {
      auto delta = tracker.GetElapsedTime<std::chrono::seconds>(pid);
      LOG(INFO) << process_name + " has stopped. Run time " << delta.count()
                << " seconds. PID: " << pid;
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
  LOG(INFO) << __FUNCTION__;
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

  LOG(INFO) << "Got new profile " << current_profile;
}
// End exported functions

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
  return TRUE;
}
