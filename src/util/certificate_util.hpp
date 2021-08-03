/*
 T2GP Launcher
 Certificate utilities.

 Copyright (c) 2020 Take-Two Interactive Software, Inc. All rights reserved.
*/
#pragma once

#include "shared/platform.h"
#include <filesystem>

namespace t2gp {
BOOL GetSignature(const std::filesystem::path& path, std::string& thumbprint);
}  // namespace t2gp