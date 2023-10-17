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