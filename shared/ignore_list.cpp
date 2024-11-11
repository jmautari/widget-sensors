/**
 * Widget Sensors
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
#include "shared/ignore_list.hpp"
#include "shared/logger.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

namespace shared {
IgnoreList::IgnoreList() {
  quit_event_ = CreateEvent(nullptr, false, false, nullptr);
}

IgnoreList::~IgnoreList() {
  if (quit_event_) {
    SetEvent(quit_event_);
    WaitForSingleObject(watcher_thread_, INFINITE);
    CloseHandle(quit_event_);
  }

  if (file_watcher_ != INVALID_HANDLE_VALUE)
    FindCloseChangeNotification(file_watcher_);
}

bool IgnoreList::LoadList(std::filesystem::path filename) {
  std::unique_lock lock(mutex_);
  filename_ = std::move(filename);

  try {
    std::ifstream file(filename_);
    if (!file.is_open())
      return false;

    auto data = nlohmann::json::parse(file);
    if (!data.contains("ignore_list") || !data["ignore_list"].is_array())
      return false;

    data_.clear();
    for (auto [a, i] : data["ignore_list"].items()) {
      std::string exe = i["exe"];
      data_.emplace(ToLower(exe));
    }

    if (file_watcher_ == INVALID_HANDLE_VALUE) {
      auto p = filename_;
      file_watcher_ = CreateFileW(p.remove_filename().c_str(),
          FILE_LIST_DIRECTORY | GENERIC_READ,
          FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING,
          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
      watcher_thread_ = CreateThread(
          nullptr, 0, &IgnoreList::StartWatcherThread, this, 0, 0);
    }

    return true;
  } catch (...) {
  }
  return false;
}

bool IgnoreList::IsIgnoredProcess(std::filesystem::path path) {
  std::shared_lock lock(mutex_);
  return data_.find(ToLower(path.filename().u8string())) != data_.end();
}

bool IgnoreList::AddProcess(std::filesystem::path const& path) {
  if (!IsIgnoredProcess(path)) {
    std::unique_lock lock(mutex_);
    data_.emplace(path.u8string());
    return true;
  }

  return false;
}

void IgnoreList::Save() {
  std::shared_lock lock(mutex_);
  nlohmann::json data;
  data["ignore_list"] = nlohmann::json::array();
  for (auto i : data_) {
    data.push_back({ "exe", std::move(i) });
  }
  lock.unlock();

  std::error_code ec;
  std::filesystem::path bak{ filename_ };
  bak.remove_filename();
  bak /= filename_.filename().wstring() + L".bak";
  if (std::filesystem::exists(filename_, ec)) {
    if (std::filesystem::exists(bak, ec)) {
      if (!std::filesystem::remove(bak, ec))
        return;
    }

    std::filesystem::rename(filename_, bak, ec);
    if (ec)
      return;
  }

  std::ofstream file(filename_, std::ios::trunc);
  file << data.dump(2);
}

void IgnoreList::WatcherThread() {
  OVERLAPPED ovl{};
  ovl.hEvent = CreateEvent(nullptr, true, false, nullptr);
  if (ovl.hEvent == nullptr) {
    LOG(ERROR) << "Could not start directory watcher. Err: " << GetLastError();
    return;
  }
  HANDLE handles[]{ quit_event_, ovl.hEvent };
  DWORD res;
  std::vector<wchar_t> buffer;
  buffer.resize(4096);
  DWORD bytes_read;

  ReadDirectoryChangesW(file_watcher_, buffer.data(), buffer.size(), false,
      FILE_NOTIFY_CHANGE_LAST_WRITE, &bytes_read, &ovl, nullptr);

  while (true) {
    res = WaitForMultipleObjects(_countof(handles), handles, false, INFINITE);
    if (res != WAIT_OBJECT_0 + 1)
      break;

    // Process the changes
    auto const ignore_list_file = filename_.filename();
    auto info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
    DWORD bytes;
    GetOverlappedResult(file_watcher_, &ovl, &bytes, false);
    do {
      std::wstring file = info->FileName;
      if (file == ignore_list_file)
        static_cast<void>(LoadList(filename_));

      if (info->NextEntryOffset == 0)
        break;

      info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
          reinterpret_cast<BYTE*>(info) + info->NextEntryOffset);
    } while (info);

    ResetEvent(ovl.hEvent);
    ReadDirectoryChangesW(file_watcher_, buffer.data(), buffer.size(), false,
        FILE_NOTIFY_CHANGE_LAST_WRITE, &bytes_read, &ovl, nullptr);
  }

  CloseHandle(ovl.hEvent);
}

DWORD WINAPI IgnoreList::StartWatcherThread(LPVOID param) {
  auto self = reinterpret_cast<IgnoreList*>(param);
  self->WatcherThread();
  return 0;
}

std::string const& ToLower(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(),
      [](auto c) { return std::tolower(c); });
  return str;
}
}  // namespace shared