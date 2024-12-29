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
#include "process_tracker.hpp"
#include <wbemcli.h>
#include <comdef.h>

namespace core {
void ProcessTracker::Add(int pid, std::string process_name) {
  list_[pid] = std::make_pair(
      std::move(process_name), std::chrono::system_clock::now());
}

bool ProcessTracker::Delete(int pid) {
  if (auto&& it = list_.find(pid); it != list_.end()) {
    list_.erase(it);
    return true;
  }

  return false;
}

int ProcessTracker::GetPidByProcessName(
    std::string const& process_name) const {
  int pid{};
  auto it = std::find_if(list_.begin(), list_.end(), [&](auto&& i) {
    if (i.second.first != process_name)
      return false;

    pid = i.first;
    return true;
  });
  if (it == list_.end())
    return {};

  return pid;
}
}  // namespace core