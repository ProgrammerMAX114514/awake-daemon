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
// logging.cpp - implementation of the daemon's file logging.
// =============================================================================

#include "logging.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <vector>

#include "config.h"

namespace daemonlog {

namespace {

std::mutex g_mutex;              // guards everything below

bool g_enabled = false;          // log_enabled
bool g_debugEnabled = false;     // log_level == "debug"
unsigned int g_maxFiles = 10;    // log_max_files (rolling limit)
unsigned long long g_maxBytes =  // log_max_size_mb, as bytes
    5ULL * 1024ULL * 1024ULL;

std::string g_directory;         // ...\awake\logs\daemon
std::string g_currentFile;       // absolute path of the active log file
unsigned long long g_currentSize = 0;

// Reads one [settings] key with a default when it is missing.
std::string ReadStringSetting(const char* key, const std::string& fallback) {
    std::string value;
    if (!config::ReadSetting("settings", key, value) || value.empty()) {
        return fallback;
    }
    return value;
}

// Creates "<dir>\daemon-<timestamp>.log" and prunes older files so that at
// most maxFiles daemon-*.log files remain (newest first, timestamp names sort
// chronologically). Returns the new file path or an empty string on failure.
// Creates every missing directory level of the path (CreateDirectoryA only
// creates the final level, and the log path is three levels deep).
void CreateDirectoryChain(const std::string& directory) {
    std::string partial;
    for (size_t i = 0; i < directory.size(); ++i) {
        partial += directory[i];
        if ((directory[i] == '\\' || directory[i] == '/') && partial.size() > 3) {
            CreateDirectoryA(partial.c_str(), NULL); // fails harmlessly when it exists
        }
    }
    CreateDirectoryA(directory.c_str(), NULL);
}

std::string OpenNewLogFile() {
    if (g_directory.empty()) {
        return std::string();
    }
    CreateDirectoryChain(g_directory);

    SYSTEMTIME st;
    GetLocalTime(&st);
    char name[64] = { 0 };
    sprintf_s(name, "daemon-%04u%02u%02u-%02u%02u%02u.log",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    const std::string path = g_directory + "\\" + name;

    std::ofstream file(path.c_str(), std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        return std::string();
    }
    file.close();

    // Rolling limit: collect every daemon-*.log and delete the oldest ones
    // beyond the configured count.
    const std::string pattern = g_directory + "\\daemon-*.log";
    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA(pattern.c_str(), &found);
    if (handle != INVALID_HANDLE_VALUE) {
        std::vector<std::string> logs;
        do {
            if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                logs.push_back(g_directory + "\\" + found.cFileName);
            }
        } while (FindNextFileA(handle, &found));
        FindClose(handle);
        if (logs.size() > g_maxFiles) {
            std::sort(logs.begin(), logs.end()); // names sort chronologically
            for (size_t i = 0; i + g_maxFiles < logs.size(); ++i) {
                DeleteFileA(logs[i].c_str());
            }
        }
    }
    return path;
}

// Appends one finished line to the current file, rolling it over when it has
// outgrown the size limit. Caller holds g_mutex and logging is enabled.
void AppendLine(const std::string& line) {
    // Open in append mode so exports always see a complete file even while
    // the daemon is running.
    std::ofstream file(g_currentFile.c_str(), std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        return;
    }
    file << line;
    file.close();

    g_currentSize += line.size();
    if (g_currentSize >= g_maxBytes) {
        // Size limit reached: start a fresh file (which also applies the
        // rolling limit to the old ones).
        const std::string newFile = OpenNewLogFile();
        if (!newFile.empty()) {
            g_currentFile = newFile;
            g_currentSize = 0;
        }
    }
}

} // namespace

void ReloadFromConfig() {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Settings with their defaults; values are validated with clamps so a
    // hand-edited awake.ini cannot produce absurd behavior.
    const std::string enabledValue = config::ToLowerAscii(
        ReadStringSetting("log_enabled", "false"));
    g_enabled = (enabledValue == "true" || enabledValue == "1" || enabledValue == "yes");

    const std::string level = config::ToLowerAscii(
        ReadStringSetting("log_level", "info"));
    g_debugEnabled = (level == "debug");

    const int maxFiles = std::atoi(
        ReadStringSetting("log_max_files", "10").c_str());
    g_maxFiles = static_cast<unsigned int>(
        (std::min)((std::max)(maxFiles, 1), 100));

    const int maxMb = std::atoi(
        ReadStringSetting("log_max_size_mb", "5").c_str());
    g_maxBytes = static_cast<unsigned long long>(
        (std::min)((std::max)(maxMb, 1), 100)) * 1024ULL * 1024ULL;

    // The directory only depends on the user profile.
    char localAppData[MAX_PATH] = { 0 };
    if (!GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH) ||
        localAppData[0] == '\0') {
        GetTempPathA(MAX_PATH, localAppData); // degraded fallback
        g_directory = std::string(localAppData) + "awake\\logs\\daemon";
    } else {
        g_directory = std::string(localAppData) + "\\awake\\logs\\daemon";
    }

    if (g_enabled && g_currentFile.empty()) {
        // Logging was just switched on (or the daemon just started): create
        // the first file of this session.
        const std::string newFile = OpenNewLogFile();
        if (!newFile.empty()) {
            g_currentFile = newFile;
            g_currentSize = 0;
        } else {
            g_enabled = false; // cannot create the file: stay disabled
        }
    }
}

std::string LogDirectory() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_directory;
}

bool Enabled() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_enabled;
}

void Write(const char* level, const std::string& message) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_enabled || g_currentFile.empty()) {
        return;
    }
    if (std::string(level) == "debug" && !g_debugEnabled) {
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    char stamp[32] = { 0 };
    sprintf_s(stamp, "[%04u-%02u-%02u %02u:%02u:%02u] ",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::ostringstream lineBuilder;
    lineBuilder << stamp << '[' << level << "] " << message << '\n';
    AppendLine(lineBuilder.str());
}

void Info(const std::string& message) {
    Write("info", message);
}

void Debug(const std::string& message) {
    Write("debug", message);
}

} // namespace daemonlog
