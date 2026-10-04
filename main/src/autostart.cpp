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
// autostart.cpp - implementation of the daemon autostart (registry Run key).
// =============================================================================

#include "autostart.h"

#include <windows.h>

#include "config.h"
#include "ipc.h"

namespace autostart {

namespace {

// Per-user autostart key; no admin rights are required to write it.
const char* const kRunKeyPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// Name of the registry value identifying this tool. A fixed, well-known
// name makes enable/disable/query idempotent.
const char* const kValueName = "awake";

// Builds the command line that the registry value holds: the daemon is
// started detached with the internal launch token, exactly like
// "awake daemon on" would start it.
std::string BuildAutostartCommandLine() {
    return "\"" + config::GetExeDirectory() + "\\" + ipc::kDaemonExeName +
           "\" --internal-daemon " + ipc::kDaemonLaunchToken;
}

} // namespace

bool IsEnabled() {
    std::string registeredPath;
    return Query(registeredPath);
}

bool Query(std::string& registeredPath) {
    registeredPath.clear();

    HKEY key = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false; // key missing or not accessible -> treat as disabled
    }
    DWORD type = 0;
    DWORD size = 0;
    LRESULT result = RegQueryValueExA(key, kValueName, NULL, &type, NULL, &size);
    if (result == ERROR_SUCCESS && type == REG_SZ && size > 0) {
        std::string data(size, '\0');
        result = RegQueryValueExA(key, kValueName, NULL, &type,
                                  reinterpret_cast<BYTE*>(&data[0]), &size);
        if (result == ERROR_SUCCESS) {
            // The stored command line looks like
            //   "<path>\awake.daemon.exe" --internal-daemon <token>
            // Extract the path between the first and the last quote so the
            // caller can check whether the registered executable still
            // exists (stale-entry detection).
            const size_t firstQuote = data.find('"');
            const size_t lastQuote = data.rfind('"');
            if (firstQuote != std::string::npos && lastQuote > firstQuote) {
                registeredPath = data.substr(firstQuote + 1, lastQuote - firstQuote - 1);
            }
        }
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ;
}

bool Enable(std::string& error) {
    // The autostart entry is pointless when the daemon is not installed
    // next to the client, so fail early with a clear message.
    const std::string daemonPath = config::GetExeDirectory() + "\\" + ipc::kDaemonExeName;
    if (GetFileAttributesA(daemonPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        error = "awake.daemon.exe was not found next to awake.exe (" + daemonPath + ")";
        return false;
    }

    HKEY key = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        error = "cannot open the registry run key (error " +
                std::to_string(GetLastError()) + ")";
        return false;
    }
    const std::string commandLine = BuildAutostartCommandLine();
    const LRESULT result = RegSetValueExA(key, kValueName, 0, REG_SZ,
                                          reinterpret_cast<const BYTE*>(commandLine.c_str()),
                                          static_cast<DWORD>(commandLine.size() + 1));
    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
        error = "cannot write the registry value (error " + std::to_string(result) + ")";
        return false;
    }
    return true;
}

bool Disable(std::string& error) {
    HKEY key = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        error = "cannot open the registry run key (error " +
                std::to_string(GetLastError()) + ")";
        return false;
    }
    const LRESULT result = RegDeleteValueA(key, kValueName);
    RegCloseKey(key);
    if (result == ERROR_FILE_NOT_FOUND) {
        return true; // already disabled - keep disable idempotent
    }
    if (result != ERROR_SUCCESS) {
        error = "cannot delete the registry value (error " + std::to_string(result) + ")";
        return false;
    }
    return true;
}

} // namespace autostart
