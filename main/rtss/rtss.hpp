/*
 * RTSS shared memory
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
  std::string GetCurrentProcessName();
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
};
}  // namespace rtss