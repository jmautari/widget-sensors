/*
 T2GP Launcher
 Date & Time utilities.

 Copyright (c) 2020 Take-Two Interactive Software, Inc. All rights reserved.
*/
#pragma once

#include "shared/platform.h"
#include <time.h>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace t2gp {
namespace launcher {
namespace util {
static const char* sWeekDays[]{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri",
  "Sat" };
static const char* sMonths[]{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
  "Ago", "Sep", "Oct", "Nov", "Dez" };

/**
 * \brief Wrapper function for thread-safe versions of std::localtime, aka
 * localtime_s (Windows) and localtime_r (Mac OS X/Posix). It has the same
 * parameters and return value of the original localtime function but takes
 * a pointer to where the resulting tm structure will be stored instead of
 * using the static storage used in the default implementations which is
 * obviously not thread-safe.
 * \param[in] t a pointer to a std::time_t structure containing the time to be
 * converted to local time zone.
 * \param[in,out] tm a pointer to a tm structure that will receive the local
 * time.
 * \return the pointer to the tm structure passed in parameter tm.
 **/
inline struct std::tm* localtime(const std::time_t* t, struct tm* tm) {
#if defined(OS_WIN)
  // Microsoft VC doesn't support localtime_r and Microsoft CRT version of
  // localtime_s has reversed parameter order and returns an error code instead
  // of a pointer to the input tm struct.
  if (::localtime_s(tm, t) == 0) {
    return tm;
  }

  // If case of error return the original tm pointer with its contents zeroed.
  memset(tm, 0, sizeof(tm));
  return tm;
#else
  // Should work on Mac OS X and Linux/Posix systems.
  return ::localtime_r(t, tm);
#endif
}

/**
 * \brief Wrapper function for thread-safe versions of std::gmtime, aka
 * gmtime_s (Windows) and gmtime_r (Mac OS X/Posix). It has the same
 * parameters and return value of the original localtime function but takes
 * a pointer to where the resulting tm structure will be stored instead of
 * using the static storage used in the default implementations which is
 * obviously not thread-safe.
 * \param[in] t a pointer to a std::time_t structure containing the time to be
 * converted to GMT.
 * \param[in,out] tm a pointer to a tm structure that will receive the GMT.
 * \return the pointer to the tm structure passed in parameter tm.
 **/
inline std::tm* gmtime(const std::time_t* t, tm* tm) {
#if defined(OS_WIN)
  // Microsoft VC doesn't support gmtime_r and Microsoft CRT version of
  // localtime_s has reversed parameter order and returns an error code instead
  // of a pointer to the input tm struct.
  if (::gmtime_s(tm, t) == 0) {
    return tm;
  }

  // If case of error return the original tm pointer with its contents zeroed.
  memset(tm, 0, sizeof(tm));
  return tm;
#else
  // Should work on Mac OS X and Linux/Posix systems.
  return ::gmtime_r(t, tm);
#endif
}

/**
 * \brief Format an RFC-7232 compliant date/time string. See
 * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Last-Modified
 * \param[in] t the std::time_t value that will be converted to string.
 * \return the date & time string in GMT time zone.
 **/
inline std::string GetGmtDateTimeString(std::time_t t) {
  std::tm tm;
  auto last_modified_gmt = util::gmtime(&t, &tm);
  std::ostringstream ss;
  ss << sWeekDays[last_modified_gmt->tm_wday] << ", " << std::setw(2)
     << std::setfill('0') << last_modified_gmt->tm_mday << " "
     << sMonths[last_modified_gmt->tm_mon] << " "
     << (1900 + last_modified_gmt->tm_year) << " " << std::setw(2)
     << std::setfill('0') << last_modified_gmt->tm_hour << ":" << std::setw(2)
     << std::setfill('0') << last_modified_gmt->tm_min << ":" << std::setw(2)
     << std::setfill('0') << last_modified_gmt->tm_sec << " GMT";
  return ss.str();
}
}  // namespace util
}  // namespace launcher
}  // namespace t2gp
