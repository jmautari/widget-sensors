#include "shared/platform.hpp"
#include "shared/simple_db.hpp"
#include "shared/string_util.h"
#include "shared/widget_plugin.h"
#include "shared/logger.hpp"
#include "twitch.hpp"
#include "nlohmann/json.hpp"
#include "fmt/format.h"
#include "nvapi/nvapi.h"
#include "nvapi/NvApiDriverSettings.h"
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
constexpr wchar_t kConfigFile[]{ L"twitch.json" };
constexpr wchar_t kGamesDbFile[]{ L"games_db.json" };

constexpr char kDefaultIp[]{ "0.0.0.0" };
constexpr int kDefaultPort = 30000;

bool debug = false;
bool init = false;
struct {
  std::string client_id;
  std::string secret;
  std::string ip;
  int port{};
} twitch_config;
std::unique_ptr<network::TwitchClient> twitch;
std::filesystem::path data_dir;
std::filesystem::path db_file;
core::SimpleDb game_db;

template<typename T>
T GetConfigOrDefaultValue(nlohmann::json const& j,
    std::string k,
    T default_value = {}) {
  if (!j.contains(k))
    return default_value;

  return j[k].get<T>();
}

void StartTwitchBackend() {
  twitch->StartListen(twitch_config.client_id, twitch_config.secret,
      twitch_config.ip, twitch_config.port);

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

std::string FindGame(std::wstring executable) {
  // (1) Create the session handle to access driver settings
  NvDRSSessionHandle hSession = 0;
  auto status = NvAPI_DRS_CreateSession(&hSession);
  if (status != NVAPI_OK)
    return {};

  std::string game;
  bool res = false;
  do {
    status = NvAPI_DRS_LoadSettings(hSession);
    if (status != NVAPI_OK) {
      LOG(ERROR) << "Could not load settings. Err: " << status;
      break;
    }

    NvDRSProfileHandle hProfile;
    auto const app = std::make_unique<NVDRS_APPLICATION>();
    app->version = NVDRS_APPLICATION_VER;
    status = NvAPI_DRS_FindApplicationByName(hSession,
        reinterpret_cast<NvU16*>(executable.data()), &hProfile, app.get());
    if (status != NVAPI_OK) {
      if (status == NVAPI_EXECUTABLE_NOT_FOUND)
        LOG(ERROR) << "Profile not found for " << wstring2string(executable);
      else
        LOG(ERROR) << "Other NVAPI error. Code: " << status;

      break;
    }

    LOG(INFO) << "Profile found";

    auto profile = std::make_unique<NVDRS_PROFILE>();
    profile->version = NVDRS_PROFILE_VER;
    status = NvAPI_DRS_GetProfileInfo(hSession, hProfile, profile.get());
    if (status != NVAPI_OK) {
      LOG(ERROR) << "Error getting profile info";
      break;
    }

    LOG(INFO) << profile->profileName;
    LOG(INFO) << "==";

    game = wstring2string(reinterpret_cast<wchar_t*>(profile->profileName));
  } while (false);

  // (6) We clean up. This is analogous to doing a free()
  static_cast<void>(NvAPI_DRS_DestroySession(hSession));
  return game;
}
}  // namespace

// Begin exported functions
bool DECLDLL PLUGIN InitPlugin(const std::filesystem::path& d,
    bool debug_mode) {
  LOG(INFO) << __FUNCTION__;
  data_dir = d;
  db_file = data_dir / kGamesDbFile;

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

    twitch_config.client_id = std::move(client_id);
    twitch_config.secret = std::move(secret);
    twitch_config.ip = std::move(ip);
    twitch_config.port = port;

    twitch = std::make_unique<network::TwitchClient>(data_dir);

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
  return {};
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
  if (pname.empty())
    return;

  auto const filename = data_dir / L"history.txt";
  std::ofstream file(filename, std::ios::app);
  if (file.is_open())
    file << pname << std::endl;

  if (!game_db.Load(db_file, true))
    LOG(ERROR) << "Could not load games database";

  try {
    std::filesystem::path p = pname;
    std::string exe = p.filename().u8string();
    auto const& game_data = game_db.Find(
        [&](auto&& it) { return exe == it["exe"]; });
    std::string const game = game_data.is_null() ? FindGame(p.wstring())
                                                 : game_data["name"];
    if (!game.empty()) {
      auto i = twitch->GetGameInfo(game);
      if (!i.contains("data") || !i["data"].is_array()) {
        file << "No data for " << game << std::endl;
        return;
      }

      nlohmann::json item;
      std::string game_id;
      if (i["data"].size() > 0 && !i["data"][0].is_null()) {
        item = i["data"][0];
        game_id = item["id"];
      }

      if (game_id.empty()) {
        file << "ID is empty for " << game << std::endl;
        return;
      }

      std::string title;
      auto& data = game_db.Find([&](auto&& it) {
        std::string const id = it["id"];
        return id == game_id;
      });
      if (data.is_null()) {
        title = item["name"];
        file << "Adding game " << game << " ID: " << game_id << std::endl;
        // clang-format off
        // "box_art_url":"https://static-cdn.jtvnw.net/ttv-boxart/515025-{width}x{height}.jpg","id":"515025","igdb_id":"125174","name":"Overwatch 2
        // clang-format on
        item["exe"] = exe;
        item["title"] = item["name"];
        if (game_db.Add(std::move(item))) {
          if (!game_db.Save())
            file << "Could not save data" << std::endl;
          else
            file << "Item added successfully" << std::endl;
        } else {
          file << "Could not add item" << std::endl;
        }
      } else {
        title = data["title"];
        file << "Found " << game << " Title: " << title << " ID: " << game_id
             << std::endl;
      }

      LOG(INFO) << "Starting " << game << " id: " << game_id;
      twitch->SetBroadcastInfo(game_id, title);
    } else {
      file << "Profile for " << pname << " was not found!" << std::endl;
    }
  } catch (...) {
    file << "Error processing game" << std::endl;
  }
}
// End exported functions

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD, LPVOID) {
  hInstance = hInst;
  return TRUE;
}
