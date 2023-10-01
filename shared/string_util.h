#pragma once
#include <stringapiset.h>
#include <string>
#include <vector>

namespace {
std::string wstring2string(const std::wstring& wstr) {
  auto dest_size = WideCharToMultiByte(
      CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, 0, 0);
  if (dest_size > 0) {
    std::vector<char> buffer(dest_size);
    if (WideCharToMultiByte(
            CP_UTF8, 0, wstr.c_str(), -1, buffer.data(), dest_size, 0, 0)) {
      return std::string(buffer.data());
    }
  }

  return std::string();
}

std::wstring string2wstring(const std::string& str) {
  auto dest_size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
  if (dest_size > 0) {
    std::vector<wchar_t> buffer(dest_size);
    if (MultiByteToWideChar(
            CP_UTF8, 0, str.c_str(), -1, buffer.data(), dest_size)) {
      return std::wstring(buffer.data());
    }
  }

  return std::wstring();
}
}  // namespace