/*
 T2GP Launcher
 Logging system.

 Copyright (c) 2020 Take-Two Interactive Software, Inc. All rights reserved.
*/
#pragma once

#include "shared/platform.h"
#include "logging/logging_macros.h"
#include "util/datetime_util.h"
#include <fmt/format.h>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <shared_mutex>
#include <chrono>
#include <algorithm>

// Macros used by LOG macro - see logging_macros.h
#define kTRACEL3 kTrace3
#define kTRACEL2 kTraceL2
#define kTRACEL1 kTraceL1
#define kDEBUG kDebug
#define kINFO kInfo
#define kWARNING kWarning
// Workaround for Windows ERROR macro which replaces kERROR with k0
#define k0 kError
#define kCRITICAL kCritical

namespace t2gp {
namespace launcher {
namespace logging {
/*!
 \brief The logging severity level from less important to most important.
 * kXXXXX ids are used by macros to avoid changing all LOG macro calls.
 */
enum class LoggingLevel {
  kTraceL3 = -4,
  kTraceL2 = -3,
  kTraceL1 = -2,
  /*! Messages with severity level equal to or lesser than `Debug` are discarded
   * when the Logging class verbose logs flag is disabled.
   * See \ref Logging::SetLogLevel */
  kDebug = -1,
  kInfo = 0,
  kWarning = 1,
  kError = 2,
  kCritical = 3
};

struct LogFile {
  std::filesystem::path filename;
  std::ofstream file;
  int32_t max_size_in_bytes{};
};

constexpr int32_t kMaxLogFiles = 5;

// The % before the name prevents auto-linking that breaks the docs on S3.
/// %Logging class.
class Logging {
public:
  /**
   * \brief Create a new Logging instance.
   * \param[in] log_id the log id that will be used for reference when using
   * multiple log files.
   * \param[in] filename the output file name.
   * \param[in] max_size_in_bytes the max file size in bytes, the log file will
   * be rotated when it reaches the maximum file size.
   **/
  ~Logging() {
    Shutdown();
  }

  void AddLog(const std::string& log_id,
      const std::filesystem::path& filename,
      int32_t max_size_in_bytes = T2GP_MAX_LOG_SIZE_MB) {
    std::unique_lock lock(mutex_);
    if (streams_.find(log_id) != streams_.end())
      return;

    std::ofstream file(filename, std::ios::app);
    if (!file.is_open())
      return;

    LogFile logfile;
    logfile.filename = filename;
    logfile.file = std::move(file);
    logfile.max_size_in_bytes = std::max<int32_t>(T2GP_MIN_LOG_SIZE_MB,
        std::min<std::int32_t>(max_size_in_bytes, T2GP_MAX_LOG_SIZE_MB));

    streams_.insert(std::make_pair(log_id, std::move(logfile)));
  }

  void Shutdown() {
    std::unique_lock lock(mutex_);
    std::for_each(streams_.begin(), streams_.end(), [](auto& i) {
      LogFile& logfile = i.second;
      logfile.file.close();
    });
    streams_.clear();
  }

  template<typename... Args>
  void Log(const std::string& log_id,
      LoggingLevel severity,
      const std::string& source,
      uint32_t line,
      const char* format_template,
      Args&&... args) {
    if (severity < level_)
      return;

    std::unique_lock lock(mutex_);
    auto it = streams_.find(log_id);
    if (it == streams_.end())
      return;

    const auto get_date_time = [&] {
      const auto now = std::chrono::system_clock::now();
      const auto now_time = std::chrono::system_clock::to_time_t(now);
      const auto ms = (now - std::chrono::system_clock::from_time_t(now_time))
                          .count();
      char buffer[32];
      struct tm timeinfo;
      time_t time_now;
      time(&time_now);
      gmtime_s(&timeinfo, &time_now);
      strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
      return std::string(buffer) + "." + std::to_string(ms).substr(0, 3);
    };
    const auto get_severity_string = [&] {
      switch (severity) {
        case LoggingLevel::kTraceL1:
          return "TRACEL1";
        case LoggingLevel::kTraceL2:
          return "TRACEL2";
        case LoggingLevel::kTraceL3:
          return "TRACEL3";
        case LoggingLevel::kDebug:
          return "DEBUG";
        case LoggingLevel::kWarning:
          return "WARNING";
        case LoggingLevel::kError:
          return "ERROR";
        case LoggingLevel::kCritical:
          return "CRITICAL";
        default:
          return "INFO";
      }
    };
    const auto get_severity = [&]() -> std::string {
      const auto s = get_severity_string();
      const auto size = strlen(s);
      if (size >= 8)
        return s;

      return s + std::string(8 - size, ' ');
    };
    constexpr auto get_pid = [] {
#if defined(OS_WIN)
      return GetCurrentProcessId();
#else
      return getpid();
#endif
    };
    const auto get_filename = [&] {
#if defined(OS_WIN)
      const char* sep = "\\";
#else
      const char* sep = "/";
#endif
      if (const auto p = source.rfind(sep); p != std::string::npos) {
        return source.substr(p + 1);
      }

      return source;
    };
    constexpr auto get_thread_id = [] {
#if defined(OS_WIN)
      return GetCurrentThreadId();
#else
      return std::this_thread::get_id();
#endif
    };
    auto& logfile = it->second;
    auto& file = logfile.file;
    if (!file.is_open())
      return;

    const auto log_size = (file
                           << get_date_time() << "\t[" << get_severity()
                           << "]\t[" << get_pid() << "]\t" << get_filename()
                           << ":" << line << "\t[" << get_thread_id() << "]\t"
                           << fmt::format(format_template, args...) << std::endl
                           << std::flush)
                              .tellp();
    if (log_size < logfile.max_size_in_bytes)
      return;

    RotateFile(logfile);
  }

  /**
   * \brief Set the current logging level. See \ref LoggingLevel
   * \param[in] new_level the new logging severity level. Any messages with
   * a severity level value lesser than the new_value will be discarded.
   **/
  void SetLogLevel(LoggingLevel new_level) {
    std::unique_lock lock(mutex_);
    level_ = new_level;
  }

  static Logging* GetInstance() {
    static Logging instance;
    return &instance;
  }

private:
  Logging() = default;

  void RotateFile(LogFile& logfile) {
    std::error_code ec;
    logfile.file.close();

    // Find next available file (1 - kMaxLogFiles)
    constexpr auto get_next_index = [](auto&& path,
                                        auto&& current_file) -> std::wstring {
      constexpr auto get_int = [](auto&& v) {
        try {
          return std::stoi(v);
        } catch (...) {
          return 0;
        }
      };

      int32_t i = 0;
      for (auto& e : std::filesystem::directory_iterator(path)) {
        if (!e.is_regular_file())
          continue;

        const auto name = e.path().filename();
        if (name.wstring().find(current_file) != 0 ||
            name.extension().wstring().length() != 2)  // Look for .1 .2 .3 etc.
          continue;

        const auto ext = name.extension().wstring().substr(1);  // remove dot
        i = std::max(i, get_int(ext));
      }
      return std::to_wstring((i % kMaxLogFiles) + 1);
    };
    // Rename file to next index.
    const auto index = get_next_index(
        logfile.filename.parent_path(), logfile.filename.filename());
    auto new_file = logfile.filename;
    new_file.replace_extension(L".log." + index);

    // Delete existing file before renaming.
    if (std::filesystem::exists(new_file))
      std::filesystem::remove(new_file, ec);

    std::filesystem::rename(logfile.filename, new_file, ec);

    // Truncate file and resume logging.
    logfile.file = std::ofstream(logfile.filename, std::ios::trunc);
  }

  std::unordered_map<std::string, LogFile> streams_;
  std::shared_mutex mutex_;
  LoggingLevel level_ = LoggingLevel::kInfo;
};
}  // namespace logging
}  // namespace launcher
}  // namespace t2gp