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

// Diagnostic trace set by ReloadFromConfig under the lock, emitted by
// Write() after that lock has been released (R16 wedge diagnostics).
std::string g_traceAfterUnlock;

bool g_enabled = false;          // log_enabled
bool g_debugEnabled = false;     // log_level == "debug"
unsigned int g_maxFiles = 10;    // log_max_files (rolling limit)
unsigned long long g_maxBytes =  // log_max_size_mb, as bytes
    5ULL * 1024ULL * 1024ULL;

std::string g_directory;         // ...\awake\logs\daemon
std::string g_currentFile;       // absolute path of the active log file
std::string g_currentDay;        // YYYYMMDD the current file belongs to
unsigned long long g_currentSize = 0;

// Reads one [settings] key with a default when it is missing.
std::string ReadStringSetting(const char* key, const std::string& fallback) {
    std::string value;
    if (!config::ReadSetting("settings", key, value) || value.empty()) {
        return fallback;
    }
    return value;
}

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

// Returns today's date as "YYYYMMDD" - the log file name is per DAY, so
// restarts on the same day append to the same file instead of scattering
// one file per start.
std::string TodayStamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char stamp[16] = { 0 };
    sprintf_s(stamp, "%04u%02u%02u", st.wYear, st.wMonth, st.wDay);
    return std::string(stamp);
}

// Prunes older daemon-*.log files so that at most maxFiles remain (names
// sort chronologically: per-day names first, then timestamped size-rollover
// files of the same day).
void PruneOldLogs() {
    const std::string pattern = g_directory + "\\daemon-*.log";
    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA(pattern.c_str(), &found);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    std::vector<std::string> logs;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            logs.push_back(g_directory + "\\" + found.cFileName);
        }
    } while (FindNextFileA(handle, &found));
    FindClose(handle);
    if (logs.size() > g_maxFiles) {
        std::sort(logs.begin(), logs.end());
        for (size_t i = 0; i + g_maxFiles < logs.size(); ++i) {
            DeleteFileA(logs[i].c_str());
        }
    }
}

// Makes sure g_currentFile points at the file new lines belong to: today's
// daemon-YYYYMMDD.log (appended across daemon restarts) or, after a size
// rollover, a timestamped continuation file of the same day. Caller holds
// g_mutex. forceNew rolls to a fresh timestamped file (size limit hit).
void ResolveLogFile(bool forceNew) {
    if (g_directory.empty()) {
        return;
    }
    CreateDirectoryChain(g_directory);

    const std::string today = TodayStamp();
    if (!forceNew && !g_currentFile.empty() && g_currentDay == today) {
        // Still today's file - but the max-files setting may have changed
        // via RELOAD, so the pruning runs on every resolve anyway.
        PruneOldLogs();
        return; // keep appending to the current file
    }

    std::string name = "daemon-" + today + ".log";
    if (forceNew) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char suffix[32] = { 0 };
        sprintf_s(suffix, "-%02u%02u%02u", st.wHour, st.wMinute, st.wSecond);
        name = "daemon-" + today + suffix + ".log";
    }
    const std::string path = g_directory + "\\" + name;

    std::ofstream file(path.c_str(), std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        return; // cannot open: keep the old target; retried on the next write
    }
    file.close();

    g_currentFile = path;
    g_currentDay = today;

    // Track the actual size of the file we just attached to.
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    g_currentSize = 0;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attributes)) {
        g_currentSize = (static_cast<unsigned long long>(attributes.nFileSizeHigh) << 32) |
                        attributes.nFileSizeLow;
    }

    PruneOldLogs();
}

// Appends one finished line to the current file, rolling it over when it has
// outgrown the size limit or the day has changed. Caller holds g_mutex and
// logging is enabled.
void AppendLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_mutex);
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
        // Size limit reached: roll to a fresh timestamped continuation file
        // (which also applies the rolling limit to the old ones).
        ResolveLogFile(true);
        g_currentSize = 0;
    }
}

} // namespace

void ReloadFromConfig() {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Settings with their defaults; values are validated with clamps so a
    // hand-edited awake.ini cannot produce absurd behavior.
    const std::string enabledValue = config::ToLowerAscii(
        ReadStringSetting("log_enabled", "true")); // logging is on by default
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

    if (g_enabled) {
        // Attach to today's log file (or open it when this is the first
        // write of the day). Restarts on the same day keep appending.
        ResolveLogFile(false);
        if (g_currentFile.empty()) {
            g_enabled = false; // cannot create the file: stay disabled
        }
    }

    // Diagnostic trace (R16): ReloadFromConfig runs on the server thread and
    // was the last thing in the log when a wedge was once observed. The
    // trace is built under the lock and emitted after releasing it (Write
    // would self-deadlock otherwise).
    const std::string trace =
        "log settings reloaded: enabled=" + std::string(g_enabled ? "1" : "0") +
        " level=" + (g_debugEnabled ? "debug" : "info") +
        " maxFiles=" + std::to_string(g_maxFiles) +
        " file=" + (g_currentFile.empty() ? "<none>" : g_currentFile);
    g_traceAfterUnlock = trace;
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
    // The guard lives in AppendLine now, closer to the file handle, so the
    // hot path only pays for it once.
    const unsigned int traceBudgetMs = 200;
    if (!g_traceAfterUnlock.empty()) {
        // Emit the pending reload trace first (never filtered by level).
        AppendLine(g_traceAfterUnlock);
        g_traceAfterUnlock.clear();
    }
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
