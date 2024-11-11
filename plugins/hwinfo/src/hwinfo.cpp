/**
 * Widget Sensors
 * HWINFO64 plug-in
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
#include "hwinfo.hpp"
#include "shared/logger.hpp"
#include <chrono>
#include <memory>
#include <sstream>
#include <unordered_map>

namespace windows {
constexpr wchar_t kHWINFO64Key[] = L"SOFTWARE\\HWiNFO64\\VSB";
constexpr wchar_t kLabelKey[] = L"Label";
constexpr wchar_t kSensorKey[] = L"Sensor";
constexpr wchar_t kValuekey[] = L"Value";
constexpr wchar_t kValueRawKey[] = L"ValueRaw";

constexpr size_t kDataSize = 512;

HwInfo::HwInfo() {
  quit_event_ = CreateEvent(nullptr, true, false, nullptr);
}

HwInfo::~HwInfo() {
  Shutdown();

  if (quit_event_)
    CloseHandle(quit_event_);
}

bool HwInfo::Initialize() {
  if (init_) {
    LOG(ERROR) << "Already initialized";
    return false;
  }

  for (int retry = 30; retry > 0; retry--) {
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kHWINFO64Key, 0,
            KEY_QUERY_VALUE | KEY_NOTIFY, &key_) != ERROR_SUCCESS) {
      LOG(ERROR) << "Cannot open registry key. Retrying...";
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } else {
      LOG(INFO) << "Registry opened successfully";
      break;
    }
  }

  if (key_ == nullptr) {
    LOG(ERROR) << "RegOpenKey failure";
    return false;
  }

  LOG(INFO) << "Registry key opened, spawning runner thread";

  runner_ = std::thread(&HwInfo::Runner, this);
  init_ = runner_.joinable();
  return init_;
}

void HwInfo::Shutdown() {
  if (init_) {
    init_ = false;

    SetEvent(quit_event_);
    if (runner_.joinable())
      runner_.join();
  }

  if (key_) {
    RegCloseKey(key_);
    key_ = {};
  }
}

std::wstring HwInfo::GetData() {
  std::shared_lock lock(mutex_);
  return cached_data_;
}

void HwInfo::Runner() {
  auto change_event = CreateEvent(nullptr, false, false, nullptr);
  if (change_event == nullptr) {
    LOG(ERROR) << "Cannot create event";
    return;
  }

  const auto registry_notify = [&] {
    auto reg_notify = RegNotifyChangeKeyValue(
        key_, true, REG_NOTIFY_CHANGE_LAST_SET, change_event, true);
    if (reg_notify == ERROR_SUCCESS)
      return true;

    LOG(ERROR) << "Error subscribing to registry changes. Err: " << reg_notify;
    return false;
  };

  HANDLE handles[] = { change_event, quit_event_ };
  key_list_t list;
  DWORD res;
  for (;;) {
    registry_notify();

    res = WaitForMultipleObjects(_countof(handles), handles, false, INFINITE);
    if (res != WAIT_OBJECT_0)
      break;

    ReadRegistry(list);

    std::wostringstream o;
    for (auto&& [k, v] : list) {
      auto&& [sensor, label, value, value_raw] = v;
      if (wcslen(sensor.get()) == 0)
        continue;

      o << L"\"" << sensor << L"=>" << label << L"\": {\"index\":" << k
        << L",\"sensor\": \"" << label << L"\",\"value\":\"" << value
        << L"\",\"valueRaw\":\"" << value_raw << L"\"},";
    }

    {
      std::unique_lock lock(mutex_);
      cached_data_ = std::move(o.str());
      cached_data_.pop_back();
    }
  }

  CloseHandle(change_event);
}

void HwInfo::ReadRegistry(key_list_t& list) {
  if (first_) {
    first_ = false;

    wchar_t keyname[128];
    const auto get_key = [&](const wchar_t* k, uint32_t i) {
      _snwprintf_s(keyname, _countof(keyname), _TRUNCATE, L"%s%d", k, i);
      auto size = wcslen(keyname);
      auto ptr = std::make_unique<wchar_t[]>(size + 1);  // + null terminator
      memcpy(reinterpret_cast<void*>(ptr.get()), keyname,
          (size + 1) * sizeof(wchar_t));
      return ptr;
    };

    for (uint32_t i = 0; i < kMaxKeys; i++) {
      keys_[i][0] = get_key(kSensorKey, i);
      keys_[i][1] = get_key(kLabelKey, i);
      keys_[i][2] = get_key(kValuekey, i);
      keys_[i][3] = get_key(kValueRawKey, i);

      std::get<0>(list[i]) = std::make_unique<wchar_t[]>(kDataSize);
      std::get<1>(list[i]) = std::make_unique<wchar_t[]>(kDataSize);
      std::get<2>(list[i]) = std::make_unique<wchar_t[]>(kDataSize);
      std::get<3>(list[i]) = std::make_unique<wchar_t[]>(kDataSize);
    }
  }

  const auto get_value = [&](const wchar_t* k, wchar_t* data) {
    auto size = static_cast<DWORD>(kDataSize);
    DWORD type;
    return RegQueryValueExW(key_, k, nullptr, &type,
               reinterpret_cast<BYTE*>(data), &size) == ERROR_SUCCESS;
  };

  for (uint32_t i = 0; i < kMaxKeys; i++) {
    if (!get_value(keys_[i][0].get(), std::get<0>(list[i]).get()))
      continue;

    if (!get_value(keys_[i][1].get(), std::get<1>(list[i]).get()))
      continue;

    get_value(keys_[i][2].get(), std::get<2>(list[i]).get());
    // get_value(keys[i][3].get(), std::get<3>(list[i]).get());
  }
}
}  // namespace windows