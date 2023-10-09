#pragma once
#include "shared/platform.hpp"
#include <string>

namespace windows {
class EmbeddedResource {
public:
  explicit EmbeddedResource(HINSTANCE instance);
  std::string GetResourceById(int resource_id) const;

private:
  HINSTANCE instance_{};
};
}  // namespace windows
