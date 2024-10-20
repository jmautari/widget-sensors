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
#pragma once
#include "shared/platform.hpp"
#include <filesystem>
#include <shared_mutex>
#include <set>

namespace shared {
class IgnoreList {
public:
  IgnoreList();
  ~IgnoreList();

  [[nodiscard]] bool LoadList(std::filesystem::path filename);
  [[nodiscard]] bool IsIgnoredProcess(std::filesystem::path path);
  bool AddProcess(std::filesystem::path const& path);

  void Save();
  void WatcherThread();

private:
  std::filesystem::path filename_;
  std::set<std::string> data_;
  std::shared_mutex mutex_;
  HANDLE watcher_thread_ {};
  HANDLE file_watcher_{ INVALID_HANDLE_VALUE };
  HANDLE quit_event_{};

  static DWORD WINAPI StartWatcherThread(LPVOID param);
};

static std::string const& ToLower(std::string& str);
}