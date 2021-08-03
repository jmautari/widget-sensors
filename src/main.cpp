#include "util/certificate_util.hpp"
#include <filesystem>
#include <iostream>

int wmain(int argc, wchar_t* argv[]) {
  std::string thumbprint;
  std::wstring exe = argv[0];
  exe = exe.substr(exe.rfind(L"\\") + 1);
  for (auto& e : std::filesystem::recursive_directory_iterator(".")) {
    if (!e.is_regular_file())
      continue;

    const auto ext = e.path().extension();
    if (ext != ".exe")
      continue;

    if (t2gp::GetSignature(e, thumbprint)) {
      std::cout << e.path().u8string() << " " << thumbprint << std::endl;
    } else {
      if (e.path().filename().native() != exe) {
        std::wcout << L"Could not get certificate thumbprint for "
                   << e.path().native() << std::endl;
      }
    }
  }
  return 0;
}
