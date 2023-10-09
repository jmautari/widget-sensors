#include "shared/platform.hpp"
#include <shellapi.h>
#include "shell_util.hpp"

namespace util {
bool OpenViaShell(const std::wstring& url) {
  SHELLEXECUTEINFOW s{};
  s.cbSize = sizeof(s);
  s.lpFile = url.c_str();
  s.nShow = SW_SHOW;
  return ShellExecuteExW(&s);
}
}  // namespace util
