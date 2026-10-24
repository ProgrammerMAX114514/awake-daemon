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
#include <cstdlib>
#include <io.h>
#include <string>

namespace console {

// Set once the reminder has been shown (or is not wanted any more).
static bool g_aiNoticeSuppressed = false;

// Runtime switch for colored output (--no-color / NO_COLOR, F3): when false
// the color codes are not emitted at all.
static bool g_colorsEnabled = true;

const char* const kColorReset = "\x1b[0m";
const char* const kColorRed   = "\x1b[31m";
const char* const kColorGreen = "\x1b[32m";
const char* const kColorYellow= "\x1b[33m";
const char* const kColorCyan  = "\x1b[36m";

void SetColorsEnabled(bool enabled) {
    g_colorsEnabled = enabled;
}

bool ColorsEnabled() {
    return g_colorsEnabled;
}

namespace {

// Reads an environment variable. _dupenv_s is used under MSVC because plain
// getenv() is flagged as deprecated (C4996) at /W4.
bool ReadEnv(const char* name, std::string& value) {
    value.clear();
#if defined(_MSC_VER)
    char* buffer = NULL;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == NULL) {
        return false;
    }
    value.assign(buffer);
    std::free(buffer);
    return true;
#else
    const char* raw = std::getenv(name);
    if (raw == NULL) {
        return false;
    }
    value.assign(raw);
    return true;
#endif
}

// Case-insensitive comparison against an ASCII literal, so that
// AWAKE_AI_NOTICE=ALWAYS works as well.
bool EqualsIgnoreCase(const std::string& value, const char* expected) {
    size_t i = 0;
    for (; i < value.size(); ++i) {
        char c = value[i];
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
        if (expected[i] == '\0' || c != expected[i]) {
            return false;
        }
    }
    return expected[i] == '\0';
}

} // namespace

void EnableColors() {
    // NO_COLOR (https://no-color.org): any non-empty value disables colors.
    std::string noColor;
    if (ReadEnv("NO_COLOR", noColor) && !noColor.empty()) {
        g_colorsEnabled = false;
    }
    if (!g_colorsEnabled) {
        return; // plain output: do not even switch the console into VT mode
    }
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
    if (g_colorsEnabled) {
        std::printf("%s", color);
    }
    std::vprintf(fmt, args);
    if (g_colorsEnabled) {
        std::printf("%s", kColorReset);
    }
    va_end(args);
}

void SuppressAiNotice() {
    g_aiNoticeSuppressed = true;
}

void PrintAiNotice() {
    if (g_aiNoticeSuppressed) {
        return;
    }
    // AWAKE_AI_NOTICE overrides the automatic decision; the default is to
    // show the line only when a human is actually looking at the terminal.
    std::string mode;
    const bool hasMode = ReadEnv("AWAKE_AI_NOTICE", mode);
    bool show = true;
    if (hasMode && EqualsIgnoreCase(mode, "always")) {
        show = true;
    } else if (hasMode && EqualsIgnoreCase(mode, "never")) {
        show = false;
    } else {
        show = (_isatty(_fileno(stderr)) != 0);
    }
    if (!show) {
        return;
    }

    // stderr, not stdout: a redirected stdout (pipes, redirects, the future
    // --json mode) must stay machine-readable.
    std::string noColor;
    const bool plain = ReadEnv("NO_COLOR", noColor);
    std::fprintf(stderr, "%sAI-generated software: review before use.%s\n",
                 plain ? "" : kColorYellow, plain ? "" : kColorReset);
}

} // namespace console
