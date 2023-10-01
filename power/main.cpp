#include "shared/platform.hpp"
#include "shared/power_util.hpp"
#include <shellapi.h>
#include <chrono>
#include <iostream>

auto FakeMoveMouse() {
  // https://stackoverflow.com/a/44139660
  // Get the current position to ensure we put it back at the end
  POINT pt;
  GetCursorPos(&pt);

  // Make a mouse movement
  // Go to upper left corner (0,0)
  INPUT input;
  input.type = INPUT_MOUSE;
  input.mi.mouseData = 0;
  input.mi.dx = 0;
  input.mi.dy = 0;
  input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
  SendInput(1, &input, sizeof(input));
  Sleep(1);  // Just in case this is needed

  // Go to lower right corner (65535,65535)
  input.mi.dx = 65535;
  input.mi.dy = 65535;
  input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
  SendInput(1, &input, sizeof(input));
  Sleep(1);  // Just in case this is needed

  // Restore to original
  SetCursorPos(pt.x, pt.y);

  // Now let the system know a user is present
  SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
}

int WINAPI wWinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow) {
  int argc;
  auto const argv = CommandLineToArgvW(GetCommandLineW(), &argc);

  if (argc < 2 || argv == nullptr) {
    MessageBeep(MB_ICONSTOP);
    return 1;
  }

  windows::PowerUtil putil;
  std::wstring profile = argv[1];
  if (profile == L"--balanced" || profile == L"-b") {
    if (!putil.SetScheme(windows::PowerScheme::kPowerBalanced))
      return 1;
  } else if (profile == L"--ultimate-performance" || profile == L"-u") {
    if (!putil.SetScheme(windows::PowerScheme::kPowerUltimatePerformance))
      return 1;

    // Also try to turn on the monitor if currently sleeping
    FakeMoveMouse();
  } else {
    return 1;
  }

  return 0;
}
