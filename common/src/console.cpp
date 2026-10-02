// Copyright 2026 ProgrammerMAX114514
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// AI-GENERATED SOFTWARE: this file was generated with AI assistance.
// Please review it carefully before use. The author is not liable for
// any consequences arising from the use of this software.

// =============================================================================
// console.cpp - implementation of the ANSI colored console output helpers.
// =============================================================================

#include "console.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>

namespace console {

const char* const kColorReset = "\x1b[0m";
const char* const kColorRed   = "\x1b[31m";
const char* const kColorGreen = "\x1b[32m";
const char* const kColorYellow= "\x1b[33m";
const char* const kColorCyan  = "\x1b[36m";

void EnableColors() {
    // Only the standard output handle needs the virtual terminal flag;
    // every colored write in this tool goes through stdout.
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
        return; // not attached to a console, nothing to enable
    }
    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode)) {
        return; // output is redirected to a file/pipe, keep raw escape codes off the console path
    }
    // Enable ANSI escape sequence interpretation. If the flag is already
    // set, or the flag is not supported, the call simply has no effect.
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(handle, mode);
}

void PrintLine(const char* color, const char* text) {
    Printf(color, "%s\n", text);
}

void Printf(const char* color, const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    std::printf("%s", color);
    std::vprintf(fmt, args);
    std::printf("%s", kColorReset);
    va_end(args);
}

} // namespace console
