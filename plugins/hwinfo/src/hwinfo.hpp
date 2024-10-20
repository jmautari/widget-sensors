#pragma once
#include "shared/platform.hpp"
#include <array>
#include <chrono>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

namespace windows {
using wstr_ptr_t = std::unique_ptr<wchar_t[]>;
using key_list_t = std::unordered_map<int32_t,
    std::tuple<wstr_ptr_t, wstr_ptr_t, wstr_ptr_t, wstr_ptr_t>>;

constexpr uint32_t kMaxKeys = 100;

class HwInfo {
public:
  HwInfo();
  ~HwInfo();

  bool Initialize();
  void Shutdown();

  [[nodiscard]] std::wstring GetData();

private:
  void Runner();
  void ReadRegistry(key_list_t& list);

  bool init_{};
  bool quit_{};
  bool first_{ true };
  std::thread runner_;
  std::shared_mutex mutex_;

  HANDLE quit_event_;
  HKEY key_{};
  std::array<std::array<std::unique_ptr<wchar_t[]>, 4>, kMaxKeys> keys_;

  std::wstring cached_data_;
};
}  // namespace windows