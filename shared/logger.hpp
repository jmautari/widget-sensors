/**
 * Widget Sensors
 * Logger
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
#include <ShlObj.h>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <mutex>

#ifdef ERROR
#undef ERROR
#endif

#define LOG(x) logger::StreamLog::get(x, __FILE__, __LINE__)
#define INFO "INFO"
#define WARN "WARNING"
#define ERROR "ERROR"

namespace logger {
class StreamLog {
public:
  StreamLog() {
    const auto root = GetRoamingDir() / LOG_DIR;
    std::error_code ec;
    std::filesystem::create_directory(root, ec);
    file_ = std::ofstream(root / LOG_FILE, std::ios::app);
  }

  template<typename T>
  StreamLog& operator<<(T&& t) {
    std::lock_guard lock(mutex_);
    file_ << std::forward<T>(t);
    return *this;
  }

  static StreamLog& get(std::string const& severity = "INFO",
      std::string const& filename = {},
      int line = 0,
      unsigned pid = GetCurrentProcessId(),
      unsigned tid = GetCurrentThreadId()) {
    static StreamLog log;
    std::time_t currentTime = std::time(nullptr);
    std::tm* localTime = std::localtime(&currentTime);

    std::filesystem::path f{ filename };
    log.file_ << std::endl
              << std::put_time(localTime, "%Y-%m-%d %H:%M:%S") << "\t["
              << severity << "]"
              << "\t[" << pid << "]\t" << f.filename().u8string() << ":" << line
              << "\t[" << tid << "]\t";
    return log;
  }

private:
  static std::filesystem::path GetRoamingDir() {
    PWSTR path;
    if (SUCCEEDED(
            SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path))) {
      std::filesystem::path p{ path };
      CoTaskMemFree(path);
      return p;
    }

    throw std::runtime_error("Cannot get roaming dir");
  }

  std::ofstream file_;
  std::mutex mutex_;
};
}  // namespace logger