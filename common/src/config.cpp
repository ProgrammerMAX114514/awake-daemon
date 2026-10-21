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
#include <cstddef>
#include <fstream>
#include <set>

namespace config {

namespace {

// The [license] section, including the explanatory comments that are
// written into a fresh configuration file and added to older files when the
// acceptance flag is stored for the first time. The value line follows the
// header and is appended separately (see BuildLicenseSection).
const char kLicenseSectionHead[] =
    "# ===========================================================\n"
    "# License acceptance\n"
    "# ===========================================================\n"
    "# awake is released under the Apache License 2.0. The official\n"
    "# license page, including the full text, is:\n"
    "#     https://www.apache.org/licenses/LICENSE-2.0\n"
    "#\n"
    "# Setting license_accepted to true means that you have read the\n"
    "# license and agree to its terms.\n"
    "# Any other value, a missing entry or a missing file makes awake\n"
    "# print the license and ask again before running a command.\n"
    "#\n"
    "[license]\n";

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
    "#   - Section headers such as [watch] and [license] select the\n"
    "#     section that the following lines belong to.\n"
    "#   - Inside [watch], every non-empty line is ONE image name,\n"
    "#     for example:\n"
    "#       notepad.exe\n"
    "#     Only the file name part is compared and the comparison\n"
    "#     is case-insensitive. Use 'awake add <imagename>' and\n"
    "#     'awake del <imagename>' to edit this list comfortably.\n"
    "#   - Sections other than [watch] hold 'key = value' settings;\n"
    "#     their lines are never treated as watch entries.\n"
    "#\n"
    "# ===========================================================\n"
    "# License acceptance\n"
    "# ===========================================================\n"
    "# awake is released under the Apache License 2.0. The official\n"
    "# license page, including the full text, is:\n"
    "#     https://www.apache.org/licenses/LICENSE-2.0\n"
    "#\n"
    "# Setting license_accepted to true means that you have read the\n"
    "# license and agree to its terms.\n"
    "# Any other value, a missing entry or a missing file makes awake\n"
    "# print the license and ask again before running a command.\n"
    "#\n"
    "[license]\n"
    "license_accepted = false\n"
    "\n"
    "[watch]\n";

// The value line written by SetLicenseAccepted().
std::string LicenseValueLine(bool accepted) {
    return std::string(kLicenseKey) + " = " + (accepted ? "true" : "false");
}

// Splits a multi-line template constant into individual lines, dropping the
// trailing empty entry that comes from the final newline.
std::vector<std::string> SplitTemplate(const char* text) {
    std::vector<std::string> lines;
    std::string current;
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(*p);
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

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

// Returns true for blank lines and comment lines.
bool IsCommentOrBlank(const std::string& trimmed) {
    if (trimmed.empty()) {
        return true;
    }
    return trimmed[0] == '#' || trimmed[0] == ';';
}

// Returns true when the trimmed line is a "[name]" section header.
bool IsSectionHeader(const std::string& trimmed) {
    return !trimmed.empty() && trimmed[0] == '[';
}

// Returns the lower-cased name of a section header, or an empty string when
// the line is not a well formed header.
std::string SectionName(const std::string& trimmed) {
    if (!IsSectionHeader(trimmed)) {
        return std::string();
    }
    const size_t close = trimmed.find(']');
    if (close == std::string::npos || close < 2) {
        return std::string();
    }
    return ToLowerAscii(Trim(trimmed.substr(1, close - 1)));
}

// Returns true when lines belong to the watch list: either inside the
// [watch] section or before the first section header (files written by
// versions that had no sections at all).
bool IsWatchSection(const std::string& section) {
    return section.empty() || section == "watch";
}

// Reads the key of a "key = value" line (lower-cased), or an empty string
// when the line carries no '=' sign.
std::string KeyOf(const std::string& trimmed) {
    const size_t equals = trimmed.find('=');
    if (equals == std::string::npos) {
        return std::string();
    }
    return ToLowerAscii(Trim(trimmed.substr(0, equals)));
}

// Reads the (trimmed, lower-cased) value of a "key = value" line.
std::string ValueOf(const std::string& trimmed) {
    const size_t equals = trimmed.find('=');
    if (equals == std::string::npos) {
        return std::string();
    }
    return ToLowerAscii(Trim(trimmed.substr(equals + 1)));
}

// Reads every line of the configuration file. Returns false when the file
// cannot be opened.
bool ReadAllLines(const std::string& path, std::vector<std::string>& lines) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
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

// Removes leading and trailing blanks from text. Public wrapper around the
// internal Trim() helper, also used by the client for user input lines.
std::string TrimText(const std::string& text) {
    return Trim(text);
}

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
    std::string section;
    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = Trim(line);
        if (IsCommentOrBlank(trimmed)) {
            continue;
        }
        if (IsSectionHeader(trimmed)) {
            section = SectionName(trimmed);
            continue;
        }
        if (!IsWatchSection(section)) {
            continue; // settings of another section (e.g. [license])
        }
        entries.push_back(trimmed);
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
    if (!ReadAllLines(path, lines)) {
        return false;
    }

    // The new entry must land inside the watch section: when the file ends
    // in another section (for example a hand edited [license] block), add a
    // [watch] header first so that the entry is not read as a setting.
    std::string section;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string trimmed = Trim(lines[i]);
        if (IsSectionHeader(trimmed)) {
            section = SectionName(trimmed);
        }
    }
    if (!IsWatchSection(section)) {
        lines.push_back("[watch]");
        section = "watch";
    }

    const std::string lowered = ToLowerAscii(imageName);
    section.clear();
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string trimmed = Trim(lines[i]);
        if (IsCommentOrBlank(trimmed)) {
            continue;
        }
        if (IsSectionHeader(trimmed)) {
            section = SectionName(trimmed);
            continue;
        }
        if (IsWatchSection(section) && ToLowerAscii(trimmed) == lowered) {
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
    if (!ReadAllLines(path, lines)) {
        return false;
    }

    const std::string lowered = ToLowerAscii(imageName);
    std::string section;
    std::vector<std::string> kept;
    bool removed = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string trimmed = Trim(lines[i]);
        if (IsSectionHeader(trimmed)) {
            section = SectionName(trimmed);
            kept.push_back(lines[i]);
            continue;
        }
        if (IsCommentOrBlank(trimmed)) {
            kept.push_back(lines[i]);
            continue;
        }
        if (!removed && IsWatchSection(section) && ToLowerAscii(trimmed) == lowered) {
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

const char* const kLicenseKey = "license_accepted";

bool IsLicenseAccepted() {
    std::vector<std::string> lines;
    if (!ReadAllLines(GetConfigPath(), lines)) {
        return false; // missing or unreadable file: treat as a first run
    }
    std::string section;
    bool found = false;
    bool accepted = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string trimmed = Trim(lines[i]);
        if (IsCommentOrBlank(trimmed)) {
            continue;
        }
        if (IsSectionHeader(trimmed)) {
            section = SectionName(trimmed);
            continue;
        }
        if (section != "license" || KeyOf(trimmed) != kLicenseKey) {
            continue;
        }
        found = true;
        accepted = (ValueOf(trimmed) == "true");
    }
    // A missing key, the value false and any other value all mean "not
    // accepted yet".
    return found && accepted;
}

bool SetLicenseAccepted(bool accepted) {
    if (!EnsureConfigExists()) {
        return false;
    }
    const std::string path = GetConfigPath();
    std::vector<std::string> lines;
    if (!ReadAllLines(path, lines)) {
        return false;
    }

    const std::string valueLine = LicenseValueLine(accepted);

    std::string section;
    size_t headerIndex = std::string::npos;  // line of the "[license]" header
    size_t valueIndex = std::string::npos;   // line of the "license_accepted" key
    size_t sectionEnd = std::string::npos;   // first line after the section
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string trimmed = Trim(lines[i]);
        if (IsSectionHeader(trimmed)) {
            if (section == "license" && sectionEnd == std::string::npos) {
                sectionEnd = i;
            }
            section = SectionName(trimmed);
            if (section == "license" && headerIndex == std::string::npos) {
                headerIndex = i;
            }
            continue;
        }
        if (IsCommentOrBlank(trimmed)) {
            continue;
        }
        if (section == "license" && KeyOf(trimmed) == kLicenseKey &&
            valueIndex == std::string::npos) {
            valueIndex = i;
        }
    }

    if (valueIndex != std::string::npos) {
        lines[valueIndex] = valueLine; // update in place, keep the comments
    } else if (headerIndex != std::string::npos) {
        const size_t insertAt = (sectionEnd != std::string::npos) ? sectionEnd : lines.size();
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertAt), valueLine);
    } else {
        // No section yet (a file written by an older version): insert the
        // whole block before the first section header so that the watch list
        // keeps growing at the end of the file.
        size_t insertAt = lines.size();
        for (size_t i = 0; i < lines.size(); ++i) {
            if (IsSectionHeader(Trim(lines[i]))) {
                insertAt = i;
                break;
            }
        }
        std::vector<std::string> block = SplitTemplate(kLicenseSectionHead);
        block.push_back(valueLine);
        block.push_back(std::string());
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertAt),
                     block.begin(), block.end());
    }
    return WriteAllLines(path, lines);
}

} // namespace config
