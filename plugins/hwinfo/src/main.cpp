#include "shared/platform.hpp"
#include "shared/logger.hpp"
#include "shared/widget_plugin.h"
#include "shared/string_util.h"
#include "hwinfo.hpp"
#include <string>
#include <thread>

namespace {
bool debug = false;
bool init = false;

windows::HwInfo hwinfo;
}  // namespace

// Begin exported functions
bool DECLDLL PLUGIN InitPlugin(const std::filesystem::path& data_dir,
    bool debug_mode) {
  LOG(INFO) << __FUNCTION__;

  init = hwinfo.Initialize();
  debug = debug_mode;
  return true;
}

std::wstring DECLDLL PLUGIN GetValues(const std::wstring& profile_name) {
  if (!init)
    return L"";

  return hwinfo.GetData();
}

void DECLDLL PLUGIN ShutdownPlugin() {
  LOG(INFO) << __FUNCTION__;
  if (init) {
    init = false;

    hwinfo.Shutdown();
  }
}
// End exported functions

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
  return TRUE;
}
