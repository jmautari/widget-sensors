#include "shared/platform.hpp"
#include "shared/logger.hpp"
#include "shared/widget_plugin.h"
#include "shared/string_util.h"
#include "shared/power_util.hpp"
#include "resources/win/resource.h"
#include "main/version.h"
#include "rtss/rtss.hpp"
#include "websocket/server.hpp"
#include <atlbase.h>
#include <shellapi.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include "steam/sdk/include/steam_api.h"
#include "steam/sdk/include/isteamapps.h"
#include "nlohmann/json.hpp"
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <string>
#include <tuple>
#include <thread>
#include <shared_mutex>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <vector>

#define WM_M_TRAY   WM_USER + 1
#define ID_TRAY_ICON 100
#define IDM_EXIT 1000
#define IDM_COPY 1001
#define IDM_SET_BALANCED_PF 1002
#define IDM_SET_ULTIMATE_PERFORMANCE_PF 1003
#define IDM_CUSTOM_COMMAND 1010

namespace {
typedef BOOL(WINAPI* GUIDFromString_t)(LPCTSTR psz, LPGUID pguid);

typedef HRESULT(WINAPI* LoadIconWithScaleDown_t)(HINSTANCE hinst,
    PCWSTR pszName,
    int cx,
    int cy,
    HICON* phico);

LoadIconWithScaleDown_t LoadIconWithScaleDown_fn = nullptr;

struct loadlibrary_deleter {
  void operator()(HMODULE h) {
    ::FreeLibrary(h);
  }
  using pointer = HMODULE;
};
using ScopedLoadLibrary = std::unique_ptr<HMODULE, loadlibrary_deleter>;
using plugin_t = std::tuple<ScopedLoadLibrary,
    InitPlugin_t,
    GetValues_t,
    ShutdownPlugin_t,
    ExecuteCommand_t,
    ProfileChanged_t>;
using plugin_list_t = std::unordered_map<std::string, plugin_t>;

constexpr wchar_t kDefaultDataDir[] = L"D:\\backgrounds";
constexpr wchar_t kConfigFile[] = L"widget_sensors.json";
constexpr wchar_t kInstanceMutex[] = L"widgetsensorinstance";
constexpr wchar_t kGamesDatabase[] = L"gamedb.json";
constexpr wchar_t kAppsDatabase[] = L"appdb.json";
constexpr wchar_t kPluginsDir[] = L"plugins";
constexpr wchar_t kPluginExtension[] = L".dll";

constexpr char kPluginEntrypoint[] = "InitPlugin";
constexpr char kPluginGetValues[] = "GetValues";
constexpr char kPluginShutdown[] = "ShutdownPlugin";
constexpr char kPluginExecuteCommand[] = "ExecuteCommand";
constexpr char kPluginProfileChanged[] = "ProfileChanged";

constexpr unsigned kWebsocketPort = 30001;
constexpr int32_t kIntervalMs = 500;

std::unordered_multimap<std::string, std::filesystem::path> game_install_map;
RECT current_window_size{};
std::shared_mutex window_mutex;
plugin_list_t plugin_list;
HANDLE instance_mutex = nullptr;
HWND hwnd;
HANDLE quit_event{};
CRITICAL_SECTION cs;
std::unique_ptr<char[]> json_data;
size_t current_size{ 2048 };
size_t last_size{};
std::unordered_map<size_t, nlohmann::json> custom_commands;
windows::PowerUtil power_util;

void AddMenu(HMENU hmenu, int& pos, nlohmann::json const& popup);

bool IsRunning(HANDLE* mutex_handle) {
  auto handle = CreateMutexW(nullptr, true, kInstanceMutex);
  if (handle == nullptr) {
    std::cerr << "Warning: error opening mutex. Err: " << GetLastError()
              << std::endl;
    return false;
  }

  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(handle);
    return true;
  }

  *mutex_handle = handle;
  return false;
}

HRESULT SaveIcon(HICON hIcon, const wchar_t* path) {
  // Create the IPicture intrface
  PICTDESC desc{};
  desc.picType = PICTYPE_ICON;
  desc.icon.hicon = hIcon;
  CComPtr<IPicture> pPicture;
  HRESULT hr = OleCreatePictureIndirect(
      &desc, IID_IPicture, FALSE, (void**)&pPicture);
  if (FAILED(hr))
    return hr;

  // Create a stream and save the image
  CComPtr<IStream> pStream;
  hr = CreateStreamOnHGlobal(0, TRUE, &pStream);
  if (FAILED(hr))
    return hr;

  LONG cbSize = 0;
  hr = pPicture->SaveAsFile(pStream, TRUE, &cbSize);

  // Write the stream content to the file
  if (!FAILED(hr)) {
    HGLOBAL hBuf = 0;
    hr = GetHGlobalFromStream(pStream, &hBuf);
    if (FAILED(hr))
      return hr;

    void* buffer = GlobalLock(hBuf);
    HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (!hFile)
      hr = HRESULT_FROM_WIN32(GetLastError());
    else {
      DWORD written = 0;
      WriteFile(hFile, buffer, cbSize, &written, 0);
      CloseHandle(hFile);
    }
    GlobalUnlock(buffer);
  }
  return hr;
}

BOOL CALLBACK EnumFunc(HMODULE hModule,
    LPCWSTR lpType,
    LPWSTR lpName,
    LONG_PTR lParam) {
  if (IS_INTRESOURCE(lpName)) {
    auto icon_id = reinterpret_cast<wchar_t**>(lParam);
    if (icon_id != nullptr)
      *icon_id = lpName;

    return FALSE;
  }

  return TRUE;
}

void ExtractIconFromExe(const std::wstring& path,
    const std::filesystem::path& output_dir) {
  if (LoadIconWithScaleDown_fn == nullptr)
    return;

  auto module = ScopedLoadLibrary(
      LoadLibraryExW(path.c_str(), nullptr,
          LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE),
      loadlibrary_deleter());
  if (module.get() == nullptr)
    return;

  wchar_t* id = nullptr;
  EnumResourceNamesW(
      module.get(), RT_GROUP_ICON, EnumFunc, reinterpret_cast<LONG_PTR>(&id));
  if (id == nullptr)
    return;

  HICON icon_handle;
  auto hr = LoadIconWithScaleDown_fn(module.get(), id, 256, 256, &icon_handle);
  if (FAILED(hr))
    return;

  auto const filename = std::filesystem::path(path).filename().wstring() +
                        L".ico";
  auto const dir_name = output_dir / L"icons";
  auto const output_file = dir_name / filename;
  std::error_code ec;
  std::filesystem::create_directories(dir_name, ec);
  SaveIcon(icon_handle, output_file.c_str());
  DestroyIcon(icon_handle);
}

std::vector<AppId_t> GetGameIds(bool& init_flag) {
  init_flag = false;
  if (!SteamAPI_Init()) {
    LOG(ERROR) << "Could not initialise Steam";
    return {};
  }

  init_flag = true;

  auto apps = SteamAppList();
  if (apps == nullptr) {
    LOG(ERROR) << "SteamApps not available";
    SteamAPI_Shutdown();
    return {};
  }

  std::vector<AppId_t> list;
  uint32 list_count = 100u;
  list.resize(list_count);
  uint32 app_count = apps->GetInstalledApps(list.data(), list_count);
  if (app_count == 0) {
    LOG(ERROR) << "No apps installed";
    SteamAPI_Shutdown();
    return {};
  }

  list.resize(app_count);
  LOG(INFO) << app_count << " apps are installed";

  SteamAPI_Shutdown();
  return list;
}

void GetSteamGameList(std::vector<AppId_t> const& list,
    const std::filesystem::path& data_dir) {
  if (!SteamAPI_Init()) {
    LOG(ERROR) << "Could not initialise Steam";
    return;
  }

  auto apps = SteamApps();
  if (apps == nullptr) {
    LOG(ERROR) << "SteamApps not available";
    SteamAPI_Shutdown();
    return;
  }

  const auto db_file = data_dir / kGamesDatabase;
  std::ofstream file(db_file);
  for (uint32 i : list) {
    char path[MAX_PATH]{};
    apps->GetAppInstallDir(i, path, MAX_PATH);
    game_install_map.emplace(std::to_string(i), path);
    file << i << "," << path << std::endl;
    LOG(INFO) << "AppId " << i << " install " << path;
  }

  SteamAPI_Shutdown();
}

bool LoadDatabase(const std::filesystem::path& data_dir,
    const std::filesystem::path& filename) {
  const auto db_file = data_dir / filename;
  std::error_code ec;
  if (!std::filesystem::exists(db_file, ec))
    return false;

  std::ifstream file(db_file);
  if (!file.good())
    return false;

  std::string line;
  while (std::getline(file, line)) {
    const auto p = line.find(',');
    if (p == std::string::npos)
      continue;

    line[p] = '\0';

    auto const app_id = line.c_str();
    auto const path = &line[p + 1];

    game_install_map.emplace(app_id, path);
  }

  if (!game_install_map.empty())
    LOG(INFO) << "Game database loaded successfully";

  return !game_install_map.empty();
}

bool LoadGameDatabase(const std::filesystem::path& data_dir) {
  return LoadDatabase(data_dir, kGamesDatabase);
}

bool LoadAppDatabase(const std::filesystem::path& data_dir) {
  return LoadDatabase(data_dir, kAppsDatabase);
}

bool InitialiseGameDatabase(const std::filesystem::path& data_dir) {
  LoadDatabase(data_dir, kAppsDatabase);

  bool init_flag{};
  auto list = GetGameIds(init_flag);
  if (list.empty()) {
    if (!init_flag) {
      if (!LoadGameDatabase(data_dir))
        LOG(WARN) << "Cannot load database. Will retry later";
    }

    return false;
  }

  GetSteamGameList(list, data_dir);

  if (!game_install_map.empty())
    LOG(INFO) << "Game database loaded successfully";

  return !game_install_map.empty();
}

struct handle_data {
  unsigned long process_id;
  HWND window_handle;
};

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam) {
  auto const is_main_window = [](HWND h) {
    return GetWindow(h, GW_OWNER) == nullptr && IsWindowVisible(h);
  };
  handle_data& data = *(handle_data*)lParam;
  unsigned long process_id = 0;
  GetWindowThreadProcessId(handle, &process_id);
  if (data.process_id != process_id || !is_main_window(handle))
    return TRUE;

  data.window_handle = handle;
  return FALSE;
}

auto GetWindowForPid(DWORD pid) {
  handle_data data;
  data.process_id = pid;
  data.window_handle = nullptr;
  EnumWindows(enum_windows_callback, (LPARAM)&data);
  return data.window_handle;
}

auto GetWindowSize(HWND wnd) {
  const auto itos = [](auto&& i) { return std::to_string(i); };
  std::unique_lock lock(window_mutex);
  GetClientRect(wnd, &current_window_size);
  LOG(INFO) << "left=" << current_window_size.left
            << " top=" << current_window_size.top
            << " right=" << current_window_size.right
            << " bottom=" << current_window_size.bottom;
}

auto GetAppWindowSize(const std::filesystem::path& path) {
  PROCESSENTRY32 pe32;
  auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == nullptr)
    return;

  auto const cleanup = [&] { CloseHandle(snapshot); };
  auto const exe = path.filename();

  pe32.dwSize = sizeof(pe32);
  if (!Process32First(snapshot, &pe32))
    return;

  do {
    if (exe == pe32.szExeFile) {
      auto const wnd = GetWindowForPid(pe32.th32ProcessID);
      if (wnd != nullptr) {
        auto thread = std::thread([window_handle = wnd] {
          std::this_thread::sleep_for(std::chrono::seconds(3));
          GetWindowSize(window_handle);
        });
        if (thread.joinable())
          thread.detach();
      } else {
        LOG(ERROR) << "Window for pid " << pe32.th32ProcessID
                   << " was not found";
      }

      return;
    }
  } while (Process32Next(snapshot, &pe32));
}

std::string MapExecutableToAppId(const std::filesystem::path& data_dir,
    const std::wstring& exec) {
  InitialiseGameDatabase(data_dir);

  for (auto& [app_id, dir] : game_install_map) {
    if (exec.find(dir.wstring()) == std::string::npos)
      continue;

    GetAppWindowSize(exec);
    return app_id;
  }

  return "0";
}

bool LoadPlugin(const std::filesystem::path& path,
    const std::filesystem::path& data_dir, bool debug_mode) {
  auto lib = ScopedLoadLibrary(
      LoadLibrary(path.c_str()), loadlibrary_deleter());
  if (lib == nullptr)
    return false;

  auto init = reinterpret_cast<InitPlugin_t>(
      GetProcAddress(lib.get(), kPluginEntrypoint));
  auto getvalues = reinterpret_cast<GetValues_t>(
      GetProcAddress(lib.get(), kPluginGetValues));
  auto shutdown = reinterpret_cast<ShutdownPlugin_t>(
      GetProcAddress(lib.get(), kPluginShutdown));
  auto execute_command = reinterpret_cast<ExecuteCommand_t>(
      GetProcAddress(lib.get(), kPluginExecuteCommand));
  auto profile_changed = reinterpret_cast<
      ProfileChanged_t>(GetProcAddress(lib.get(), kPluginProfileChanged));
  if (init == nullptr || getvalues == nullptr || shutdown == nullptr)
    return false;

  if (!init(data_dir, debug_mode))
    return false;

  auto file_no_ext = path.stem().u8string();
  std::transform(file_no_ext.begin(), file_no_ext.end(), file_no_ext.begin(),
      [](auto c) { return std::tolower(c); });
  plugin_t p{ std::move(lib), init, getvalues, shutdown, execute_command,
    profile_changed };
  LOG(INFO) << "Adding plugin " << file_no_ext;
  plugin_list.emplace(std::move(file_no_ext), std::move(p));
  return true;
}

auto LoadPlugins(
    const std::filesystem::path& data_dir, bool debug_mode = false) {
  const auto plugins_dir = data_dir / kPluginsDir;
  for (auto& e : std::filesystem::directory_iterator(plugins_dir)) {
    if (!e.is_regular_file() || e.path().extension() != kPluginExtension)
      continue;

    LOG(INFO) << "Trying to load plugin from " << e.path().u8string();
    if (LoadPlugin(e, data_dir, debug_mode))
      LOG(INFO) << "Plugin loaded successfully";
    else
      LOG(ERROR) << "Could not load plugin from " << e.path().u8string();
  }
}

void OnProfileChanged(std::string const& pname) {
  for (auto& [plugin_name, p] : plugin_list) {
    auto const profile_changed = std::get<5>(p);
    if (profile_changed != nullptr) {
      profile_changed(pname);
    }
  }
}

BOOL WINAPI Shutdown(DWORD) {
  for (auto& [plugin_name, p] : plugin_list) {
    auto const shutdown = std::get<3>(p);
    if (shutdown != nullptr) {
      LOG(INFO) << "Shutting down plugin " << plugin_name;
      shutdown();
    }
  }

  CloseHandle(instance_mutex);
  return TRUE;
}

auto StartMonitoring(const wchar_t* data_dir) {
  std::error_code ec;
  if (data_dir == nullptr || !std::filesystem::exists(data_dir, ec) ||
      !std::filesystem::is_directory(data_dir, ec))
    data_dir = kDefaultDataDir;

  const auto path = std::filesystem::path(data_dir);
  LoadPlugins(path);

  std::unique_ptr<network::WebsocketServer> server;
  int result = 0;

  do {
    if (!std::filesystem::exists(path, ec)) {
      if (!std::filesystem::create_directories(path, ec)) {
        std::wcerr << L"Could not create data directory at " << path
                   << L". Err code: " << GetLastError() << std::endl;
        result = 1;
        break;
      }
    }

    std::wstring current_profile;
    uint32 current_app{};
    std::string app_poster;
    rtss::RTSSSharedMemory rtss;
    size_t data_size{};
    auto send_buffer = std::make_unique<char[]>(current_size);

    const auto set_current_profile = [&](std::wstring pname) {
      OnProfileChanged(wstring2string(pname));
      current_profile = std::move(pname);
    };

    server = std::make_unique<network::WebsocketServer>(kWebsocketPort);
    if (!server->Start([&](auto&& hdl, auto&& msg) {
          EnterCriticalSection(&cs);
          memcpy(send_buffer.get(), json_data.get(), data_size);
          LeaveCriticalSection(&cs);

          server->Send(hdl, send_buffer.get(), data_size);
        })) {
      std::cerr << "Could not start websocket server on port " << kWebsocketPort
                << std::endl;
      result = 2;
      break;
    }

    std::wcout << L"Websocket server listening to port " << kWebsocketPort
               << std::endl;

    DWORD wait_result;
    const auto write_sensors_file = [&](auto&& data) {
      EnterCriticalSection(&cs);
      const auto s = wstring2string(data);
      data_size = s.size();
      if (data_size > current_size) {
        auto new_size = data_size * 2;
        LOG(INFO) << "Resizing buffer from " << current_size << " to "
                  << new_size;
        current_size = new_size;
        json_data.reset(new char[current_size]);
        send_buffer.reset(new char[current_size]);
      }

      memcpy(json_data.get(), s.c_str(), data_size);
      last_size = data_size;
      LeaveCriticalSection(&cs);
    };

    do {
      std::wostringstream o;
      o << LR"({"sensors":{)";

      auto [framerate, framerate_raw] = rtss.GetFramerate();
      auto [frametime, frametime_raw] = rtss.GetFrametime();
      auto pname = string2wstring(rtss.GetCurrentProcessName());
      auto const process_name = [&] {
        if (auto const p = pname.rfind(L'\\'); p != std::string::npos)
          return &pname.c_str()[p + 1];

        return pname.c_str();
      }();

      uint32 app_id{};
      if (!pname.empty()) {
        if (current_profile.empty() || pname != current_profile) {
          LOG(INFO) << "Got new profile " << wstring2string(pname);
          static_cast<void>(power_util.SetScheme(
              windows::PowerScheme::kPowerUltimatePerformance));
          set_current_profile(pname);
          auto const app_image = MapExecutableToAppId(path, pname);
          app_poster = app_image.find("http") == 0 ? app_image : "";
          try {
            app_id = static_cast<uint32>(std::stoi(app_image));
            if (app_id) {
              LOG(INFO) << "Found app id " << app_id;
            } else {
              LOG(INFO) << "app id=0 app_image=" << app_image;
            }
          } catch (...) {
          }
          current_app = app_id;
        }
      } else if (!current_profile.empty()) {
        LOG(INFO) << "Reseting profile";
        set_current_profile({});
        current_app = 0;
        app_poster.clear();

        std::unique_lock lock(window_mutex);
        current_window_size = {};

        //SetPowerScheme(PowerScheme::kPowerBalanced);
      }

      LONG width, height;
      {
        std::shared_lock lock(window_mutex);
        width = current_window_size.right;
        height = current_window_size.bottom;
      }

      o << L"\"rtss=>framerate\": {\"sensor\":\"framerate\",\"value\":"
        << framerate << L",\"valueRaw\":" << framerate_raw << L"},";
      o << L"\"rtss=>frametime\": {\"sensor\":\"frametime\",\"value\":"
        << frametime << L",\"valueRaw\":" << frametime_raw << L"},";
      o << L"\"rtss=>process\": {\"sensor\":\"process\",\"value\":\""
        << process_name << L"\"},";
      o << L"\"steam=>app\": {\"sensor\":\"app\",\"value\":" << current_app
        << L"},";
      o << L"\"game=>poster\": {\"sensor\":\"poster\",\"value\":\""
        << string2wstring(app_poster) << L"\"},";
      if (width && height) {
        o << L"\"game=>size\": {\"sensor\":\"size\",\"value\":\"" << width
          << "x" << height << L"\"}";
      } else {
        o << L"\"game=>size\": {\"sensor\":\"size\",\"value\":\"\"}";
      }

      for (auto& [plugin_name, p] : plugin_list) {
        auto const getvalues = std::get<2>(p);
        if (getvalues != nullptr) {
          const auto v = getvalues(current_profile);
          if (!v.empty())
            o << L"," << v;
        }
      }
      o << L"}}";

      write_sensors_file(o.str());

      auto before_check = std::chrono::system_clock::now();
      wait_result = WaitForSingleObject(quit_event, kIntervalMs >> 2);
      if (wait_result == WAIT_TIMEOUT) {
        continue;
      } else if (wait_result == WAIT_OBJECT_0) {
        break;
      }

      auto after_check = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - before_check);
      if (after_check.count() < kIntervalMs) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kIntervalMs) - after_check);
      }
    } while (wait_result != WAIT_OBJECT_0);
  } while (false);

  if (server)
    server->Shutdown();

  DeleteCriticalSection(&cs);

  Shutdown(0);

  LOG(INFO) << "Exiting...";
  return result;
}

void RemoveTrayIcon() {
  NOTIFYICONDATA nid = {};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd;
  nid.uID = ID_TRAY_ICON;
  Shell_NotifyIcon(NIM_DELETE, &nid);
}

void CopyCurrentData() {
  if (!OpenClipboard(hwnd)) {
    LOG(ERROR) << "Error opening clipboard";
    return;
  }

  EmptyClipboard();
  do {
    // Allocate a global memory object for the text.
    auto hglbCopy = GlobalAlloc(GMEM_MOVEABLE, last_size + 1);
    if (hglbCopy == NULL) {
      LOG(ERROR) << "GlobalAlloc fail";
      break;
    }

    // Lock the handle and copy the text to the buffer.
    auto copy = reinterpret_cast<char*>(GlobalLock(hglbCopy));
    if (copy != nullptr) {
      EnterCriticalSection(&cs);
      memcpy(copy, json_data.get(), last_size);
      LeaveCriticalSection(&cs);
      copy[last_size] = '\0';
      GlobalUnlock(hglbCopy);
    }

    // Place the handle on the clipboard.
    SetClipboardData(CF_TEXT, hglbCopy);
  } while (false);
  CloseClipboard();
}

void ExecuteCustomCommand(size_t index) {
  auto it = custom_commands.find(index);
  if (it == custom_commands.end())
    return;

  try {
    auto custom_command = it->second;
    std::string action = custom_command["action"];
    std::transform(action.begin(), action.end(), action.begin(),
        [](auto c) { return std::tolower(c); });
    auto it = plugin_list.find(action);
    if (it == plugin_list.end())
      return;

    auto execute_command = std::get<4>(it->second);
    if (execute_command == nullptr)
      return;

    std::string command = custom_command["command"];
    auto params = custom_command.contains("params") &&
                          custom_command["params"].is_object()
                      ? custom_command["params"]
                      : std::vector<std::string>();

    nlohmann::json const cmd { { "command", command }, { "params", params } };
    if (!execute_command(cmd.dump()))
      LOG(ERROR) << "Error executing command " << command;
  } catch (...) {
  }
}

HMENU CreateMenuOptions(int& pos, nlohmann::json const& popup) {
  HMENU hmenu = CreatePopupMenu();
  AddMenu(hmenu, pos, popup);
  return hmenu;
}

void AddMenu(HMENU hmenu, int& pos, nlohmann::json const& popup) {
  for (auto&& [a, i] : popup.items()) {
    std::string const& text = i["text"];
    if (i.contains("popup") && i["popup"].is_array()) {
      InsertMenu(hmenu, pos++, MF_BYPOSITION | MF_POPUP,
          reinterpret_cast<UINT_PTR>(CreateMenuOptions(pos, i["popup"])),
          string2wstring(text).c_str());
    } else {
      custom_commands[pos] = i;
      InsertMenu(hmenu, pos++, MF_BYCOMMAND | MF_STRING,
          IDM_CUSTOM_COMMAND + pos, string2wstring(text).c_str());
    }
  }
}

void GetMenuOptions(HMENU hmenu, int& pos) {
  int argc{};
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == nullptr)
    return;

  std::filesystem::path config_file{ argc > 1 ? argv[1] : kDefaultDataDir };
  config_file /= kConfigFile;

  std::error_code ec;
  if (!std::filesystem::exists(config_file, ec))
    return;

  auto cfg = [&]() -> nlohmann::json {
    try {
      std::ifstream f(config_file);
      if (!f.good())
        return {};

      return nlohmann::json::parse(f);
    } catch (...) {
      return {};
    }
  }();
  if (cfg.empty())
    return;

  if (!cfg.contains("popup") || !cfg["popup"].is_array())
    return;

  try {
    AddMenu(hmenu, pos, cfg["popup"]);
  } catch (...) {
  }
}

auto FindStreamDeck() {
  HWND w = (HWND)0x50CC8;
  return w;
}

void SendPowerBroadcastMessage(bool suspending){
  auto wnd = FindStreamDeck();
  if (wnd == nullptr)
    return;

  UINT e = suspending ? PBT_APMSUSPEND : PBT_APMRESUMEAUTOMATIC;
  PostMessage(wnd, WM_POWERBROADCAST, e, 0);
}

LRESULT CALLBACK WndProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
  PAINTSTRUCT ps;
  HDC hdc;
  switch (msg) {
    case WM_COMMAND:
      if (lParam == 0) {
        auto cmd = LOWORD(wParam);
        switch (cmd) {
          case IDM_EXIT: {
            RemoveTrayIcon();
            SetEvent(quit_event);
            PostQuitMessage(0);
            break;
          }
          case IDM_COPY: {
            CopyCurrentData();
            break;
          }
          case IDM_SET_BALANCED_PF: {
            static_cast<void>(
                power_util.SetScheme(windows::PowerScheme::kPowerBalanced));
            break;
          }
          case IDM_SET_ULTIMATE_PERFORMANCE_PF: {
            static_cast<void>(power_util.SetScheme(
                windows::PowerScheme::kPowerUltimatePerformance));
            break;
          }
          default: {
            if (cmd >= IDM_CUSTOM_COMMAND) {
              ExecuteCustomCommand(
                  static_cast<size_t>(cmd) - IDM_CUSTOM_COMMAND);
            }

            break;
          }
        }
      }

      break;
    case WM_PAINT:
      hdc = BeginPaint(window, &ps);
      EndPaint(window, &ps);
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    case WM_M_TRAY:
      // This is a message that originated with the
      // Notification Tray Icon. The lParam tells use exactly which event
      // it is.
      switch (LOWORD(lParam)) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case WM_CONTEXTMENU: {
          POINT pt;
          GetCursorPos(&pt);

          HMENU hmenu = CreatePopupMenu();
          int pos = 0;
          GetMenuOptions(hmenu, pos);
          InsertMenu(hmenu, pos++, MF_BYPOSITION | MF_STRING,
              IDM_SET_BALANCED_PF, L"Balanced Power Profile");
          InsertMenu(hmenu, pos++, MF_BYPOSITION | MF_STRING,
              IDM_SET_ULTIMATE_PERFORMANCE_PF, L"Ultimate Performance Profile");
          InsertMenu(hmenu, pos++, MF_BYPOSITION | MF_SEPARATOR, 0, L"-");
          InsertMenu(hmenu, pos++, MF_BYPOSITION | MF_STRING, IDM_COPY,
              L"Copy current data");
          InsertMenu(hmenu, pos++, MF_BYPOSITION | MF_STRING, IDM_EXIT,
              L"Stop monitoring");

          auto index = power_util.GetProfileIndex();
          if (index != -1) {
            int p[] = { IDM_SET_BALANCED_PF, IDM_SET_ULTIMATE_PERFORMANCE_PF };
            if (index < _countof(p))
              CheckMenuItem(hmenu, p[index], MF_BYCOMMAND | MF_CHECKED);
          }

          SetForegroundWindow(hwnd);

          TrackPopupMenu(hmenu,
              TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0,
              hwnd, NULL);

          PostMessage(hwnd, WM_NULL, 0, 0);
          break;
        }
      }

      return 0;
    default:
      return DefWindowProc(window, msg, wParam, lParam);
  }
  return 0;
}

bool CreateWindowResources(HINSTANCE hInstance) {
  constexpr wchar_t kClassName[]{ L"hwinfowebsocketserver" };
  WNDCLASSEX c{};
  c.cbSize = sizeof(WNDCLASSEX);
  c.lpfnWndProc = WndProc;
  c.lpszClassName = kClassName;
  c.hInstance = hInstance;
  c.hIcon = LoadIcon(NULL, IDI_SHIELD);
  c.hCursor = LoadCursor(NULL, IDC_ARROW);
  c.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  c.style = CS_HREDRAW | CS_VREDRAW;

  if (!RegisterClassExW(&c))
    return false;

  hwnd = CreateWindowW(kClassName, kClassName, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr,
      nullptr, hInstance, nullptr);
  if (!hwnd) {
    LOG(ERROR) << "CreateWindow err " + GetLastError();
    return false;
  }

  ShowWindow(hwnd, SW_HIDE);

  NOTIFYICONDATA nid{};
  nid.cbSize = sizeof(NOTIFYICONDATA);
  nid.hWnd = hwnd;
  nid.uID = ID_TRAY_ICON;
  nid.uVersion = NOTIFYICON_VERSION;
  nid.uCallbackMessage = WM_M_TRAY;
  nid.hIcon = static_cast<HICON>(LoadImage(GetModuleHandle(nullptr),
      MAKEINTRESOURCEW(IDI_BIG), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
  wcscpy_s(nid.szTip, APP_NAME_W);
  nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

  if (Shell_NotifyIcon(NIM_ADD, &nid))
    Shell_NotifyIcon(NIM_SETVERSION, &nid);

  return true;
}
}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow) {
  if (IsRunning(&instance_mutex)) {
    std::cerr << "Another instance is already running" << std::endl;
    return 1;
  }

  int argc;
  auto const argv = CommandLineToArgvW(GetCommandLineW(), &argc);

  if (!CreateWindowResources(hInstance))
    return 1;

  quit_event = CreateEvent(nullptr, true, false, nullptr);
  if (quit_event == nullptr)
    return 1;

  InitializeCriticalSection(&cs);

  json_data = std::make_unique<char[]>(current_size);

  std::promise<int> running_promise;
  auto future = running_promise.get_future();
  std::thread thread([&] {
    int res = StartMonitoring(argc > 1 ? argv[1] : nullptr);
    running_promise.set_value(res);
  });
  std::thread check_future([&] {
    future.wait();
    PostMessage(hwnd, WM_COMMAND, IDM_EXIT, 0);
  });

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  thread.join();
  check_future.join();

  CloseHandle(quit_event);
  DeleteCriticalSection(&cs);
  return future.get();
}
