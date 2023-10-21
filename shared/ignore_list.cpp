/**
 * Widget Sensors
 * Copyright (C) 2021-2023 John Mautari - All rights reserved
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "shared/ignore_list.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

namespace shared {
bool IgnoreList::LoadList(std::filesystem::path filename) {
  filename_ = std::move(filename);

  try {
    std::ifstream file(filename_);
    if (!file.is_open())
      return false;

    auto data = nlohmann::json::parse(file);
    if (!data.contains("ignore_list") || !data["ignore_list"].is_array())
      return false;

    for (auto [a, i] : data["ignore_list"].items()) {
      std::string exe = i["exe"];
      data_.emplace(ToLower(exe));
    }

    return true;
  } catch (...) {
  }
  return false;
}

bool IgnoreList::IsIgnoredProcess(std::filesystem::path path) {
  std::shared_lock lock(mutex_);
  return data_.find(ToLower(path.filename().u8string())) != data_.end();
}

bool IgnoreList::AddProcess(std::filesystem::path const& path) {
  if (!IsIgnoredProcess(path)) {
    std::unique_lock lock(mutex_);
    data_.emplace(path.u8string());
    return true;
  }

  return false;
}

void IgnoreList::Save() {
  std::shared_lock lock(mutex_);
  nlohmann::json data;
  data["ignore_list"] = nlohmann::json::array();
  for (auto i : data_) {
    data.push_back({ "exe", std::move(i) });
  }
  lock.unlock();

  std::error_code ec;
  std::filesystem::path bak{ filename_ };
  bak.remove_filename();
  bak /= filename_.filename().wstring() + L".bak";
  if (std::filesystem::exists(filename_, ec)) {
    if (std::filesystem::exists(bak, ec)) {
      if (!std::filesystem::remove(bak, ec))
        return;
    }

    std::filesystem::rename(filename_, bak, ec);
    if (ec)
      return;
  }

  std::ofstream file(filename_, std::ios::trunc);
  file << data.dump(2);
}

std::string const& ToLower(std::string& str) {
  std::transform(str.begin(), str.end(), str.begin(),
      [](auto c) { return std::tolower(c); });
  return str;
}
}  // namespace shared