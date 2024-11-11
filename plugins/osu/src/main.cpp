/**
 * Widget Sensors
 * Osu! plug-in
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
#include "shared/platform.hpp"
#include "shared/simple_db.hpp"
#include "shared/string_util.h"
#include "shared/widget_plugin.h"
#include "shared/logger.hpp"
#include "osu.hpp"
#include "nlohmann/json.hpp"
#include "fmt/format.h"
#include <shellapi.h>
#include <iostream>
#include <string>
#include <thread>
#include <fstream>
#include <filesystem>
#include <map>
#include <future>

HINSTANCE hInstance;

namespace {
constexpr wchar_t kConfigFile[]{ L"osu.json" };

constexpr char kDefaultIp[]{ "0.0.0.0" };
constexpr int kDefaultPort = 30000;

bool debug = false;
bool init = false;
struct {
  std::string client_id;
  std::string secret;
  std::string ip;
  int port{};
} osu_config;
std::unique_ptr<network::OsuClient> twitch;
std::filesystem::path data_dir;
std::filesystem::path db_file;
core::SimpleDb game_db;
std::string current_game;

template<typename T>
T GetConfigOrDefaultValue(nlohmann::json const& j,
    std::string k,
    T default_value = {}) {
  if (!j.contains(k))
    return default_value;

  return j[k].get<T>();
}

void StartTwitchBackend() {
  twitch->StartListen(
      osu_config.client_id, osu_config.secret, osu_config.ip, osu_config.port);

  auto auth_url = string2wstring(twitch->GetAuthenticationUrl());
  if (auth_url.empty())
    return;

  std::wstring dir = data_dir.native();
  SHELLEXECUTEINFOW s{};
  s.cbSize = sizeof(s);
  s.lpDirectory = dir.c_str();
  s.lpFile = auth_url.c_str();
  s.nShow = SW_SHOW;
  if (!ShellExecuteExW(&s)) {
    LOG(ERROR) << "Cannot start authentication";
    return;
  }
}
}  // namespace

// Begin exported functions
bool DECLDLL PLUGIN InitPlugin(const std::filesystem::path& d,
    bool debug_mode) {
  LOG(INFO) << __FUNCTION__;
  data_dir = d;

  auto config_file = data_dir / kConfigFile;
  std::error_code ec;
  if (!std::filesystem::exists(config_file, ec))
    return false;

  try {
    std::ifstream f(config_file);
    if (!f.good())
      return false;

    auto const cfg = nlohmann::json::parse(f);
    auto client_id = GetConfigOrDefaultValue<std::string>(cfg, "client_id", {});
    auto secret = GetConfigOrDefaultValue<std::string>(cfg, "secret", {});
    auto ip = GetConfigOrDefaultValue<std::string>(cfg, "ip", kDefaultIp);
    auto const port = GetConfigOrDefaultValue<int>(cfg, "port", kDefaultPort);
    if (client_id.empty() || secret.empty() || ip.empty() || port < 0) {
      LOG(ERROR) << "Invalid config";
      return false;
    }

    osu_config.client_id = std::move(client_id);
    osu_config.secret = std::move(secret);
    osu_config.ip = std::move(ip);
    osu_config.port = port;

    twitch = std::make_unique<network::OsuClient>(data_dir);

    StartTwitchBackend();
  } catch (...) {
    LOG(ERROR) << "Error parsing config file";
    return false;
  }

  init = true;
  debug = debug_mode;
  return true;
}

std::wstring DECLDLL PLUGIN GetValues(const std::wstring& profile_name) {
  return string2wstring(fmt::format(
      "\"osu=>game_name\":{{\"sensor\":\"game\",\"value\":\"{}\"}}",
      current_game));
}

void DECLDLL PLUGIN ShutdownPlugin() {
  LOG(INFO) << __FUNCTION__;
  if (init) {
    init = false;

    if (twitch)
      twitch.reset();
  }
}

bool DECLDLL PLUGIN ExecuteCommand(const std::string& command) {
  if (command.empty())
    return false;

  auto const json = nlohmann::json::parse(command);
  std::string cmd = json["command"];
  const auto execute_cmd = [&] {
    try {
      return twitch->Request(cmd, json["params"]);
    } catch (...) {
      return false;
    }
  };
  for (int retry = 0; retry < 3; retry++) {
    if (execute_cmd())
      return true;

    StartTwitchBackend();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return false;
}

void DECLDLL PLUGIN ProfileChanged(const std::string& pname) {
}
// End exported functions

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD, LPVOID) {
  hInstance = hInst;
  return TRUE;
}
