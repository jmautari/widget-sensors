#include "resource_util.hpp"

namespace windows {
EmbeddedResource::EmbeddedResource(HINSTANCE instance) : instance_(instance) {
}

std::string EmbeddedResource::GetResourceById(int resource_id) const {
  if (!instance_)
    return "empty";

  auto resource = FindResource(
      instance_, MAKEINTRESOURCEW(resource_id), RT_RCDATA);

  if (resource == nullptr)
    return "empty";

  // Load the resource
  std::string content;
  auto resource_data = LoadResource(instance_, resource);
  if (resource_data) {
    // Get a pointer to the resource data
    auto const data = LockResource(resource_data);
    if (data) {
      // Get the size of the resource data
      size_t size = SizeofResource(instance_, resource);
      content = std::string(static_cast<const char*>(data), size);
    }

    FreeResource(resource_data);
  }

  return content;
}
}