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
// config.cpp - implementation of the awake.ini configuration file handling.
// =============================================================================

#include "config.h"

#include <windows.h>
#include <cctype>
#include <fstream>
#include <set>

namespace config {

namespace {

// The comment header that is written into a freshly created configuration
// file. It documents the format and the effect of the watch list.
const char kDefaultConfig[] =
    "# ===========================================================\n"
    "# awake configuration file\n"
    "# ===========================================================\n"
    "#\n"
    "# This file is read by the awake daemon (awake.daemon.exe).\n"
    "# While any image name listed below is running, the daemon\n"
    "# prevents the system from entering idle sleep or hibernation.\n"
    "# Manual sleep (power button, Start menu, or sleep commands\n"
    "# from other applications) is NOT blocked.\n"
    "#\n"
    "# Syntax:\n"
    "#   - Lines starting with '#' or ';' are comments.\n"
    "#   - Empty lines are ignored.\n"
    "#   - Section headers such as [watch] are ignored.\n"
    "#   - Every other non-empty line is ONE image name, for\n"
    "#     example:\n"
    "#       notepad.exe\n"
    "#     Only the file name part is compared and the comparison\n"
    "#     is case-insensitive. Use 'awake add <imagename>' and\n"
    "#     'awake del <imagename>' to edit this list comfortably.\n"
    "#\n"
    "[watch]\n";

// Removes leading and trailing blanks (spaces, tabs and carriage returns
// that linger from CRLF line endings read line by line).
std::string Trim(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

// Returns true when a raw line from the configuration file carries a watch
// entry (i.e. it is not blank, not a comment and not a section header).
bool IsEntryLine(const std::string& line) {
    if (line.empty()) {
        return false;
    }
    if (line[0] == '#' || line[0] == ';') {
        return false; // comment line
    }
    if (line[0] == '[') {
        return false; // section header
    }
    return true;
}

// Writes the initial template content to the given path, truncating any
// existing file. Shared by EnsureConfigExists() and ResetConfig().
bool WriteInitialConfig(const std::string& path) {
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file << kDefaultConfig;
    return file.good();
}

// Writes all lines back to the configuration file, always terminating the
// last line with a newline so that later appends stay well-formed.
bool WriteAllLines(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        file << lines[i] << '\n';
    }
    return file.good();
}

} // namespace

std::string GetExeDirectory() {
    char path[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string fullPath(path);
    size_t pos = fullPath.find_last_of("\\/");
    if (pos == std::string::npos) {
        return "."; // no directory separator found, fall back to CWD
    }
    return fullPath.substr(0, pos);
}

std::string GetConfigPath() {
    return GetExeDirectory() + "\\awake.ini";
}

bool EnsureConfigExists() {
    const std::string path = GetConfigPath();
    const DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return true; // the file already exists
    }
    return WriteInitialConfig(path);
}

bool ResetConfig() {
    return WriteInitialConfig(GetConfigPath());
}

std::vector<std::string> ReadWatchList() {
    std::vector<std::string> entries;
    std::ifstream file(GetConfigPath().c_str(), std::ios::binary);
    if (!file.is_open()) {
        return entries; // no file, no entries
    }
    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = Trim(line);
        if (IsEntryLine(trimmed)) {
            entries.push_back(trimmed);
        }
    }
    return entries;
}

bool AddWatchEntry(const std::string& imageName) {
    if (!IsValidImageName(imageName)) {
        return false;
    }
    if (!EnsureConfigExists()) {
        return false;
    }
    const std::string path = GetConfigPath();

    // Read the whole file so that comments are preserved and duplicates
    // (case-insensitive) can be detected.
    std::vector<std::string> lines;
    {
        std::ifstream file(path.c_str(), std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
    }

    const std::string lowered = ToLowerAscii(imageName);
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string trimmed = Trim(lines[i]);
        if (IsEntryLine(trimmed) && ToLowerAscii(trimmed) == lowered) {
            return false; // already present
        }
    }
    lines.push_back(imageName);
    return WriteAllLines(path, lines);
}

bool RemoveWatchEntry(const std::string& imageName) {
    if (!IsValidImageName(imageName)) {
        return false;
    }
    const std::string path = GetConfigPath();

    std::vector<std::string> lines;
    {
        std::ifstream file(path.c_str(), std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
    }

    const std::string lowered = ToLowerAscii(imageName);
    std::vector<std::string> kept;
    bool removed = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string trimmed = Trim(lines[i]);
        if (!removed && IsEntryLine(trimmed) && ToLowerAscii(trimmed) == lowered) {
            removed = true; // drop this line, keep everything else
            continue;
        }
        kept.push_back(lines[i]);
    }
    if (!removed) {
        return false;
    }
    return WriteAllLines(path, kept);
}

bool IsValidImageName(const std::string& imageName) {
    if (imageName.empty() || imageName.size() > 260) {
        return false;
    }
    for (size_t i = 0; i < imageName.size(); ++i) {
        const char c = imageName[i];
        // Reject path separators, wildcards and any non printable or
        // non-ASCII character; only plain image names are accepted.
        if (c == '\\' || c == '/' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || c == ':') {
            return false;
        }
        if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E) {
            return false;
        }
    }
    return true;
}

std::string ToLowerAscii(const std::string& text) {
    std::string result = text;
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
    }
    return result;
}

} // namespace config
