#include <Windows.h>
#include <iostream>
#include <unordered_map>
#include <string>
#include <tuple>
#include <thread>
#include <sstream>
#include <fstream>
#include <filesystem>

constexpr wchar_t kHWINFO64Key[] = L"SOFTWARE\\HWiNFO64\\VSB";
constexpr uint32_t kMaxKeys = 50;
constexpr wchar_t kLabelKey[] = L"Label";
constexpr wchar_t kValuekey[] = L"Value";
constexpr wchar_t kValueRawKey[] = L"ValueRaw";
constexpr int32_t kIntervalSecs = 1;
constexpr wchar_t kDefaultDataDir[] = L"D:\\backgrounds";
constexpr wchar_t kSensorsFilename[] = L"sensors.json";

using key_list_t = std::unordered_map<int32_t,
    std::tuple<std::wstring, std::wstring, std::wstring>>;

key_list_t ReadRegistry() {
  constexpr auto get_key = [](const wchar_t* k, uint32_t i) -> std::wstring {
    wchar_t keyname[32];
    _snwprintf_s(keyname, _countof(keyname), _TRUNCATE, L"%s%d", k, i);
    return keyname;
  };
  const auto get_value = [&](const std::wstring& k) -> std::wstring {
    constexpr DWORD flags = RRF_RT_REG_SZ;
    wchar_t data[256];
    DWORD size = sizeof(data);
    DWORD type;
    if (RegGetValueW(HKEY_CURRENT_USER, kHWINFO64Key, k.c_str(), flags, &type,
            data, &size) != ERROR_SUCCESS) {
      return {};
    }

    return std::wstring(data, size / 2 - 1);
  };

  key_list_t list;
  for (uint32_t i = 0; i < kMaxKeys; i++) {
    const auto label = get_value(get_key(kLabelKey, i));
    if (label.empty())
      break;

    list.insert({ i, std::make_tuple(label,
                         get_value(get_key(kValuekey, i)),
                         get_value(get_key(kValueRawKey, i))) });
  }
  return list;
}

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

int wmain(int argc, wchar_t* argv[]) {
  std::wstring data_dir;
  if (argc > 1)
    data_dir = argv[1];

  std::error_code ec;
  if (data_dir.empty() || !std::filesystem::exists(data_dir, ec) ||
      !std::filesystem::is_directory(data_dir, ec))
    data_dir = kDefaultDataDir;

  if (!std::filesystem::exists(data_dir, ec)) {
    if (!std::filesystem::create_directories(data_dir, ec)) {
      std::wcerr << "Could not create data directory at " << data_dir
                 << ". Err code: " << GetLastError() << std::endl;
      return 1;
    }
  }

  auto thread = std::thread([&] {
    const auto write_sensors_file = [&](auto&& data) {
      const auto path = std::filesystem::path(data_dir) / kSensorsFilename;
      std::ofstream f(path, std::ios::trunc);
      if (!f)
        return;

      f << wstring2string(data);
      f.close();
    };

    do {
      const auto list = ReadRegistry();
      std::wostringstream o;
      o << LR"({"sensors":{)";
      for (auto&& [k, v] : list) {
        auto&& [label, value, value_raw] = v;
        o << L"\"" << k << L"\": {\"sensor\": \"" << label << L"\",\"value\":\""
          << value << L"\",\"valueRaw\":\"" << value_raw << L"\"},";
      }
      o.seekp(-1, std::ios_base::end);
      o << "}}";
      write_sensors_file(o.str());
      std::this_thread::sleep_for(std::chrono::seconds(kIntervalSecs));
    } while (true);
  });
  thread.join();
  return 0;
}
