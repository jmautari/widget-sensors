#pragma once

#include <algorithm>
#include <sstream>
#include "picosha2.h"

#pragma optimize("", off)
namespace util {
inline auto SHA256Sum2HexString(const unsigned char* buffer, size_t size) {
  std::ostringstream s;
  for (size_t i = 0; i < size; i++) {
    s << std::setfill('0') << std::setw(2) << std::hex
      << (static_cast<int>(buffer[i]) & 255);
  }
  return s.str();
}

inline std::string String2SHA256Sum(const std::string& str) {
  std::vector<unsigned char> hash(picosha2::k_digest_size);
  try {
    picosha2::hash256(str.begin(), str.end(), hash.begin(), hash.end());
  } catch (...) {
  }
  return { hash.begin(), hash.end() };
}
}  // namespace util