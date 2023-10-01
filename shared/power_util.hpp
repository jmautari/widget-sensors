#pragma once
#include "shared/platform.hpp"
#include <array>
#include <tuple>
#include <string>

#pragma comment(lib, "PowrProf.lib")

namespace windows {
std::wstring GuidToWstr(const GUID& guid);

using power_profile_t = std::array<std::tuple<GUID, std::wstring, std::wstring>,
    2>;

enum class PowerScheme { kPowerBalanced, kPowerUltimatePerformance };

class PowerUtil {
public:
  PowerUtil();

  [[nodiscard]] bool SetScheme(PowerScheme k) const;
  [[nodiscard]] int GetProfileIndex() const;

private:
  [[nodiscard]] bool EnumerateProfiles(power_profile_t& profiles);

  power_profile_t profiles_;
  bool init_ {};
};
}  // namespace windows