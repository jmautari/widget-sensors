#pragma once
#include <filesystem>
#include <string>

#define DECLDLL __declspec(dllexport)
#define PLUGIN __stdcall

typedef bool(PLUGIN* InitPlugin_t)(const std::filesystem::path& data_dir,
    bool debug_mode);
typedef std::wstring(PLUGIN* GetValues_t)(
    const std::filesystem::path& profile_name);
typedef bool(PLUGIN* ShutdownPlugin_t)();
