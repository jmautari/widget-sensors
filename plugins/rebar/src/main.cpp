#include "shared/platform.hpp"
#include "shared/widget_plugin.h"
#include "nvapi/nvapi.h"
#include "nvapi/NvApiDriverSettings.h"
#include <iostream>
#include <string>
#include <thread>
#include <fstream>
#include <filesystem>
#include <map>

#define DBGOUT(x) if (debug) OutputDebugStringW((x))

namespace {
namespace NvDrv {
constexpr NvU32 kRebarFeature = 0X000F00BA;
constexpr NvU32 kRebarOptions = 0X000F00BB;
constexpr NvU32 kRebarSizeLimit = 0X000F00FF;
}  // namespace NvDrv

bool debug = false;
bool init = false;

struct {
  std::wstring profile;
  bool value{};

  void Reset() {
    profile.clear();
    value = false;
  }
} rebar_status;

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

bool CheckReBar(NvDRSSessionHandle hSession, NvDRSProfileHandle hProfile) {
  const auto get_setting = [&](auto&& id) -> NVDRS_SETTING {
    NVDRS_SETTING setting{};
    setting.version = NVDRS_SETTING_VER;
    if (NvAPI_DRS_GetSetting(hSession, hProfile, id, &setting) != NVAPI_OK)
      return {};

#if _DEBUG
    OutputDebugStringW(
        (std::to_wstring(id) + L" = " + (wchar_t*)setting.settingName + L" : " +
            std::to_wstring(setting.u32CurrentValue))
            .c_str());
#endif
    return setting;
  };
  if (get_setting(NvDrv::kRebarFeature).u32CurrentValue == 0 ||
      get_setting(NvDrv::kRebarOptions).u32CurrentValue == 0)
    return false;

  return true;
#if 0
  return [&] {
    static_assert(sizeof(uint64_t) == 8, "Invalid uint64 size");
    const auto s = get_setting(NvDrv::kRebarSizeLimit);
    if (s.binaryCurrentValue.valueLength != 8) {
      OutputDebugStringA(("Invalid value size of " +
                          std::to_string(s.binaryCurrentValue.valueLength))
                             .c_str());
      return 0ULL;
    }

    uint64_t v;
    memcpy(&v, s.binaryCurrentValue.valueData, sizeof(v));
    OutputDebugStringA(("valueData=" + std::to_string(v)).c_str());
    return v;
  }() > 0;
#endif
}

bool FindGameProfile(std::wstring executable) {
  if (!init)
    return false;
  else if (rebar_status.profile == executable) {
    DBGOUT((L"Returning cahced result for " + executable + L" res: " +
            (rebar_status.value ? L"true" : L"false"))
               .c_str());
    return rebar_status.value;
  }

  rebar_status.profile = executable;
  rebar_status.value = false;

  // (1) Create the session handle to access driver settings
  NvDRSSessionHandle hSession = 0;
  auto status = NvAPI_DRS_CreateSession(&hSession);
  if (status != NVAPI_OK)
    return false;

  bool res = false;
  do {
    status = NvAPI_DRS_LoadSettings(hSession);
    if (status != NVAPI_OK) {
      DBGOUT(
          (L"Could not load settings. Err: " + std::to_wstring(status))
              .c_str());
      break;
    }

    NvDRSProfileHandle hProfile;
    auto const app = std::make_unique<NVDRS_APPLICATION>();
    app->version = NVDRS_APPLICATION_VER;
    status = NvAPI_DRS_FindApplicationByName(hSession,
        reinterpret_cast<NvU16*>(executable.data()), &hProfile, app.get());
    if (status != NVAPI_OK) {
      if (status == NVAPI_EXECUTABLE_NOT_FOUND)
        DBGOUT((L"Profile not found for " + executable).c_str());

      break;
    }

    DBGOUT((L"Loaded driver profile for " + executable).c_str());
    res = CheckReBar(hSession, hProfile);
    rebar_status.value = res;
  } while (false);

  // (6) We clean up. This is analogous to doing a free()
  static_cast<void>(NvAPI_DRS_DestroySession(hSession));
  return res;
}
}  // namespace

// Begin exported functions
bool DECLDLL PLUGIN InitPlugin(const std::filesystem::path& data_dir,
    bool debug_mode) {
  OutputDebugStringW(__FUNCTIONW__);
  // (0) Initialize NVAPI. This must be done first of all
  if (NvAPI_Initialize() != NVAPI_OK)
    return false;

  init = true;
  debug = debug_mode;
  return true;
}

std::wstring DECLDLL PLUGIN GetValues(const std::wstring& profile_name) {
  if (!init)
    return L"";

  std::wstring res;
  if (profile_name.empty()) {
    if (!rebar_status.profile.empty())
      rebar_status.Reset();

    res = L"false";
  } else {
    res = FindGameProfile(profile_name) ? L"\"true\"" : L"\"false\"";
  }

  return L"\"rebar\":{\"sensor\":\"enabled\",\"value\":" + res + L"}";
}

void DECLDLL PLUGIN ShutdownPlugin() {
  OutputDebugStringW(__FUNCTIONW__);
  if (init) {
    init = false;
    NvAPI_Unload();
  }
}
// End exported functions

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
  return TRUE;
}
