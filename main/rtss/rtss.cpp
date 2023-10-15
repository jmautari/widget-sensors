#include "rtss/rtss.hpp"

#define RTSS_VERSION(x, y) ((x << 16) + y)

namespace rtss {
RTSSSharedMemory::RTSSSharedMemory() {
  ready_ = Open();
}

RTSSSharedMemory::~RTSSSharedMemory() {
  ready_ = false;
  Close();
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

std::string RTSSSharedMemory::GetCurrentProcessName() {
  if (current_process_.first == 0 || current_process_.second == nullptr)
    return {};

  return current_process_.second->szName;
}

RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_APP_ENTRY
    RTSSSharedMemory::GetEntry() {
  auto const target_pid = GetCurrentProcessPid();
  if (target_pid == 0)
    return nullptr;

  Update();
  if (!Reset())
    return nullptr;

  auto const size = static_cast<size_t>(shared_mem_->dwOSDArrSize);
  for (size_t i = 0; i < size; i++) {
    auto entry = reinterpret_cast<
        RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_APP_ENTRY>(
        reinterpret_cast<LPBYTE>(shared_mem_) + shared_mem_->dwAppArrOffset +
        (i * shared_mem_->dwAppEntrySize));
    if (entry->dwProcessID == target_pid) {
      if (entry->dwProcessID != current_process_.first) {
        current_process_ = std::make_pair(entry->dwProcessID, entry);
      }

      return entry;
    }
  }
  current_process_ = std::make_pair(0, nullptr);
  return nullptr;
}

std::pair<double, double> RTSSSharedMemory::GetFramerate() {
  if (!ready_ || !IsValidSharedMem())
    return {};

  auto const entry = GetEntry();
  if (entry == nullptr)
    return {};

  auto const delta = double(entry->dwTime1 - entry->dwTime0);
  double framerate{};
  if (delta > 0.0)
    framerate = 1000.0 * entry->dwFrames / delta;

  return { std::round(framerate), std::ceil(framerate * 10.0) / 10.0 };
}

std::pair<double, double> RTSSSharedMemory::GetFrametime() {
  if (!ready_ || !IsValidSharedMem())
    return {};

  auto const entry = GetEntry();
  if (entry == nullptr)
    return {};

  auto const frametime = double(entry->dwFrameTime) / 1000.0;
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