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
#include "rtss/rtss.hpp"

#define RTSS_VERSION(x, y) ((x << 16) + y)

namespace rtss {
RTSSSharedMemory::RTSSSharedMemory() {
  InitializeCriticalSection(&cs_);
  current_process_ = std::make_pair(0, rtss_entry_t{});
  ready_ = Open();
}

RTSSSharedMemory::~RTSSSharedMemory() {
  ready_ = false;
  Close();
  DeleteCriticalSection(&cs_);
}

bool RTSSSharedMemory::Open() {
  file_handle_ = OpenFileMapping(
      FILE_MAP_ALL_ACCESS, false, kRtssSharedMemoryId);
  if (file_handle_ == nullptr)
    return false;

  shared_mem_ = reinterpret_cast<LPRTSS_SHARED_MEMORY>(
      MapViewOfFile(file_handle_, FILE_MAP_ALL_ACCESS, 0, 0, 0));
  if (shared_mem_ == nullptr)
    return false;

  if (!IsValidSharedMem())
    return false;

  return true;
}

void RTSSSharedMemory::Close() {
  if (shared_mem_ != nullptr) {
    UnmapViewOfFile(shared_mem_);
    shared_mem_ = nullptr;
  }

  if (file_handle_ != nullptr) {
    CloseHandle(file_handle_);
    file_handle_ = nullptr;
  }
}

bool RTSSSharedMemory::Reset() {
  Close();
  return Open();
}

void RTSSSharedMemory::Update() {
  if (shared_mem_ != nullptr)
    shared_mem_->dwOSDFrame++;
}

std::string RTSSSharedMemory::GetCurrentProcessName() const {
  EnterCriticalSection(&cs_);
  auto leave_critical_section = [this] { LeaveCriticalSection(&cs_); };
  if (current_process_.first == 0)
    return {};

  return current_process_.second.szName;
}

rtss_entry_t RTSSSharedMemory::GetEntry() {
  EnterCriticalSection(&cs_);
  auto leave_critical_section = [this] { LeaveCriticalSection(&cs_); };
  auto const target_pid = GetCurrentProcessPid();
  if (target_pid == 0)
    return {};

  Update();
  if (!Reset())
    return {};

  auto const size = static_cast<size_t>(shared_mem_->dwOSDArrSize);
  for (size_t i = 0; i < size; i++) {
    auto entry = reinterpret_cast<
        RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_APP_ENTRY>(
        reinterpret_cast<LPBYTE>(shared_mem_) + shared_mem_->dwAppArrOffset +
        (i * shared_mem_->dwAppEntrySize));
    if (entry->dwProcessID == target_pid) {
      if (entry->dwProcessID != current_process_.first) {
        current_process_ = std::make_pair(entry->dwProcessID, *entry);
      }

      return *entry;
    }
  }
  current_process_ = {};
  return {};
}

std::pair<double, double> RTSSSharedMemory::GetFramerate() {
  if (!ready_ || !IsValidSharedMem())
    return {};

  auto const entry = GetEntry();
  if (entry.dwProcessID == 0)
    return {};

  auto const delta = double(entry.dwTime1 - entry.dwTime0);
  double framerate{};
  if (delta > 0.0)
    framerate = 1000.0 * entry.dwFrames / delta;

  return { std::round(framerate), std::ceil(framerate * 10.0) / 10.0 };
}

std::pair<double, double> RTSSSharedMemory::GetFrametime() {
  if (!ready_ || !IsValidSharedMem())
    return {};

  auto const entry = GetEntry();
  if (entry.dwProcessID == 0)
    return {};

  auto const frametime = double(entry.dwFrameTime) / 1000.0;
  return { std::round(frametime), std::ceil(frametime * 10.0) / 10.0 };
}

bool RTSSSharedMemory::IsValidSharedMem() const {
  return shared_mem_ != nullptr && shared_mem_->dwSignature == 'RTSS' &&
         shared_mem_->dwVersion >= RTSS_VERSION(2, 0);
}

DWORD GetCurrentProcessPid() {
  auto const foreground_window = GetForegroundWindow();
  if (foreground_window == nullptr)
    return 0;

  DWORD target_pid;
  if (!GetWindowThreadProcessId(foreground_window, &target_pid))
    return 0;

  return target_pid;
}

}  // namespace rtss