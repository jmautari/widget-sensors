#include "power_util.hpp"
#include "shared/logger.hpp"
#include "shared/string_util.h"
#include <powrprof.h>
#include <powersetting.h>

namespace windows {
PowerUtil::PowerUtil() {
  if (!EnumerateProfiles()) {
    LOG(ERROR) << "Failure while enumerating power profiles";
    return;
  }

  init_ = true;
}

bool PowerUtil::SetScheme(PowerScheme k) const {
  if (!init_)
    return false;

  if (std::get<0>(profiles_[0]).Data1 == 0 ||
      std::get<0>(profiles_[1]).Data1 == 0) {
    LOG(ERROR) << "Invalid power profile";
    return false;
  }

  GUID guid;
  std::wstring scheme;
  std::wstring name;
  switch (k) {
    case PowerScheme::kPowerBalanced:
      guid = std::get<0>(profiles_[0]);
      scheme = std::get<1>(profiles_[0]);
      name = std::get<2>(profiles_[0]);
      break;
    case PowerScheme::kPowerUltimatePerformance:
      guid = std::get<0>(profiles_[1]);
      scheme = std::get<1>(profiles_[1]);
      name = std::get<2>(profiles_[1]);
      break;
    default:
      return false;
  }

  if (PowerSetActiveScheme(nullptr, &guid) == ERROR_SUCCESS) {
    LOG(INFO) << "Set power scheme " << wstring2string(name);
    return true;
  } else {
    LOG(ERROR) << "Error setting power scheme to " << wstring2string(name);
  }

  return false;
}

bool PowerUtil::EnumerateProfiles() {
  GUID guid;
  ULONG size = sizeof(guid);
  int found = 0;

  for (ULONG x = 0; x < 16; x++) {
    if (PowerEnumerate(NULL, NULL, NULL, ACCESS_SCHEME, x,
            reinterpret_cast<UCHAR*>(&guid), &size) != ERROR_SUCCESS)
      break;

    WCHAR nameBuffer[256];
    DWORD bufferSize = sizeof(nameBuffer) / sizeof(nameBuffer[0]);

    if (PowerReadFriendlyName(NULL, &guid, NULL, NULL,
            reinterpret_cast<PUCHAR>(nameBuffer), &bufferSize) != ERROR_SUCCESS)
      continue;

    auto const name = std::wstring(nameBuffer);
    if (name == L"Balanced") {
      profiles_[0] = std::make_tuple(guid, GuidToWstr(guid), name);
      found++;
    } else if (name == L"Ultimate Performance") {
      profiles_[1] = std::make_tuple(guid, GuidToWstr(guid), name);
      found++;
    }
  }

  return found == 2;
}

int PowerUtil::GetProfileIndex() const {
  if (!init_)
    return -1;

  GUID* guid;
  int index = -1;
  if (PowerGetActiveScheme(nullptr, &guid) == ERROR_SUCCESS) {
    const auto scheme = GuidToWstr(*guid);
    index = scheme == std::get<1>(profiles_[0])   ? 0
            : scheme == std::get<1>(profiles_[1]) ? 1
                                                 : -1;
    LocalFree(guid);
  }

  return index;
}

std::wstring GuidToWstr(const GUID& guid) {
  wchar_t guid_cstr[39];
  _snwprintf_s(guid_cstr, _countof(guid_cstr), _TRUNCATE,
      L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", guid.Data1,
      guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2],
      guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6],
      guid.Data4[7]);
  return guid_cstr;
}
}  // namespace windows