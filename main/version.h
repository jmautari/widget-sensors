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
