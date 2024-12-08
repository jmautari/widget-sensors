/**
 * Widget Sensors
 * RTSS shared memory
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
#include "shared/platform.hpp"
#include "rtss/RTSSSharedMemory.h"
#include <atomic>
#include <chrono>
#include <future>

namespace rtss {
inline constexpr wchar_t kRtssSharedMemoryId[] = L"RTSSSharedMemoryV2";
using rtss_entry_t = RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_APP_ENTRY;

DWORD GetCurrentProcessPid();

class RTSSSharedMemory {
public:
  RTSSSharedMemory();
  ~RTSSSharedMemory();

  std::pair<double, double> GetFramerate();
  std::pair<double, double> GetFrametime();
  std::string GetCurrentProcessName() const;
  auto IsReady() const noexcept {
    return ready_.load();
  }

private:
  bool Open();
  void Update();
  void Close();
  bool Reset();

  bool IsValidSharedMem() const;
  rtss_entry_t GetEntry();

  std::atomic<bool> ready_;
  HANDLE file_handle_ = nullptr;
  LPRTSS_SHARED_MEMORY shared_mem_ = nullptr;
  mutable std::pair<DWORD, rtss_entry_t> current_process_;
  mutable CRITICAL_SECTION cs_;
};
}  // namespace rtss