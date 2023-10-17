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
#pragma once

#include <chrono>
#include <unordered_map>
#include <string>

namespace core {
class ProcessTracker {
public:
  void Add(int pid, std::string process_name);

  bool Delete(int pid);

  [[nodiscard]] int GetPidByProcessName(std::string const& process_name) const;

  [[nodiscard]] auto GetSize() const noexcept {
    return list_.size();
  }

  template<typename T>
  T GetElapsedTime(int pid) {
    auto&& it = list_.find(pid);
    if (it == list_.end())
      return {};

    auto start_time = it->second.second;
    return std::chrono::duration_cast<T>(
        std::chrono::system_clock::now() - start_time);
  }

private:
  std::unordered_map<int,
                    std::pair<std::string,
                        std::chrono::system_clock::time_point>> list_;
};
}  // namespace core