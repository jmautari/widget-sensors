#pragma once
#define X_STR(a) C_STR(a)
#define C_STR(a) #a

#define ANSI_TO_WIDE2(x) L##x
#define ANSI_TO_WIDE(x) ANSI_TO_WIDE2(x)

#define APP_NAME "Widget Sensors"
#define APP_NAME_W ANSI_TO_WIDE(APP_NAME)

#define WIDGETS_VER_MAJOR 3
#define WIDGETS_VER_MINOR 1
#define WIDGETS_VER_PATCH 0
#define WIDGETS_VER_BUILD 0

#define COMMIT_HASH abcdef1

// clang-format off
#define WIDGETS_VER_STRING \
  X_STR(WIDGETS_VER_MAJOR.WIDGETS_VER_MINOR.WIDGETS_VER_PATCH.WIDGETS_VER_BUILD-COMMIT_HASH)
// clang-format on
