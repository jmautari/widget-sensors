/*
 T2GP Launcher
 Logging system.
 Macros. They simplify our lives since 1978. At least when done correctly.

 Copyright (c) 2020 Take-Two Interactive Software, Inc. All rights reserved.
*/
#pragma once

#include "logging/logging.h"

#define FILE_LOG_TO(dest, severity, source, fmt, ...)                    \
  {                                                                      \
    using namespace t2gp::launcher::logging;                             \
    Logging::GetInstance()->Log(dest, LoggingLevel::k##severity, source, \
        __LINE__, fmt, ##__VA_ARGS__);                                   \
  }

#if defined(T2GP_DOWNLOADER)
#define FILE_LOG(x, source, fmt, ...) \
  FILE_LOG_TO(T2GP_LOG_DOWNLOADER, x, source, fmt, ##__VA_ARGS__)
#elif defined(T2GP_SERVICE)
#define FILE_LOG(x, source, fmt, ...) \
  FILE_LOG_TO(T2GP_LOG_SERVICE, x, source, fmt, ##__VA_ARGS__)
#elif defined(DCL_BUILD)
#define FILE_LOG(x, source, fmt, ...) \
  FILE_LOG_TO(T2GP_LOG_DNA, x, source, fmt, ##__VA_ARGS__)
#else
#define FILE_LOG(x, source, fmt, ...) \
  FILE_LOG_TO(T2GP_LOG_LAUNCHER, x, source, fmt, ##__VA_ARGS__)
#endif

/// Logs independent of the build target. Remember to use DLOG macro whenever
/// logging DEBUG only information!!!!
#define LOG_TO(dest, x, source, fmt, ...) \
  FILE_LOG_TO(dest, x, source, fmt, ##__VA_ARGS__)

/// Logs independent of the build target. Remember to use DLOG macro whenever
/// logging DEBUG only information!!!!
#define LOG(x, fmt, ...) FILE_LOG(x, __FILE__, fmt, ##__VA_ARGS__)

// wingdi.h defines ERROR as 0 making the macro LOG(ERROR, ...) to become LOG_0
// Convert LOGGING_0 back to LOGGING_ERROR.
#define LOGGING_0 LOGGING_ERROR

#ifdef _DEBUG
/// Logs DEBUG build only information, RELEASE builds will discard this message!
#define DLOG_TO(dest, x, source, fmt, ...) \
  FILE_LOG(dest, x, source, fmt, ##__VA_ARGS__)
#define DLOG(x, fmt, ...) FILE_LOG(x, __FILE__, fmt, ##__VA_ARGS__)
#else
#define DLOG_TO(dest, x, fmt, ...) (void)0
#define DLOG(x, fmt, ...) (void)0
#endif

#if !defined(IS_RELEASE) && !defined(IS_CI_BUILD)
/// This macro can be used to log sensitive information that can be useful for
/// debugging but must be logged only on NON-PRODUCTION builds.
#define SLOG_TO(dest, x, source, fmt, ...) \
  FILE_LOG(dest, x, source, fmt, ##__VA_ARGS__)
#define SLOG(x, fmt, ...) \
  FILE_LOG(x, __FILE__, "[MAY CONTAIN SENSITIVE INFO] "##fmt, ##__VA_ARGS__)
#else
#define SLOG_TO(dest, x, fmt, ...) (void)0
#define SLOG(x, fmt, ...) (void)0
#endif

#if !defined(IS_RELEASE)
/// This macro can be used to log anything that might be useful to know in
/// non-production but should not be logged in production builds.
#define NLOG_TO(dest, x, source, fmt, ...) \
  FILE_LOG(dest, x, source, fmt, ##__VA_ARGS__)
#define NLOG(x, fmt, ...) \
  FILE_LOG(x, __FILE__, "[DEV] "##fmt, ##__VA_ARGS__)
#else
#define NLOG_TO(dest, x, fmt, ...) (void)0
#define NLOG(x, fmt, ...) (void)0
#endif
