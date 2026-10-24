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
// status.cpp - "awake status", the single status view.
//
// Reports the daemon state, the keep-awake state, the screen keep-awake
// state, the autostart state, system power facts (F6) and the watch list
// (green entries are running, red entries are not) in one view. The command
// never starts the daemon; when the daemon is not running, the keep-awake
// states are inactive by definition and the watch list is checked locally
// against the running processes. With --json (F3) the same data is printed
// as a machine-readable JSON object.
// =============================================================================

#include "commands.h"

#include <windows.h>
#include <powrprof.h>

#pragma comment(lib, "powrprof.lib")

#include <set>
#include <string>
#include <vector>

#include "autostart.h"
#include "config.h"
#include "console.h"
#include "ipc.h"
#include "process.h"
#include "protocol.h"
#include "timeparse.h"

// -----------------------------------------------------------------------------
// F6 helpers: system power facts
// -----------------------------------------------------------------------------

namespace {

// Power plan GUIDs (Windows SDK names, spelled out so no extra
// INITGUID machinery is needed).
const GUID kSleepSubgroup =     { 0x238C9FA8, 0x0AAD, 0x41ED, { 0x83, 0xF4, 0x97, 0xBE, 0x24, 0x2C, 0x8F, 0x20 } };
const GUID kStandbyTimeout =    { 0x29F6C1DB, 0x86DA, 0x48C5, { 0x9F, 0xDB, 0xF2, 0xB6, 0x7B, 0x1F, 0x44, 0xDA } };
const GUID kHibernateTimeout =  { 0x9D7815A6, 0x7EE4, 0x497E, { 0x88, 0x88, 0x51, 0x5A, 0x05, 0xF0, 0x23, 0x64 } };

// Reads a power plan timeout in seconds (0 = never) from the active scheme.
// ac selects the AC (plugged in) or DC (battery) value. Returns false when
// the value is not available.
bool ReadPowerSeconds(bool ac, const GUID& setting, unsigned int& seconds) {
    DWORD value = 0;
    // The current SDK declares a 5-parameter form with a scheme personality
    // GUID; passing NULL there reads the value of the active personality.
    const DWORD result = ac
        ? PowerReadACValueIndex(NULL, &kSleepSubgroup, &setting, NULL, &value)
        : PowerReadDCValueIndex(NULL, &kSleepSubgroup, &setting, NULL, &value);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    seconds = value;
    return true;
}

// Screensaver configuration lives in the registry, not in the power plan.
// Returns false when the values are missing or unreadable.
bool ReadScreenSaver(bool& enabled, unsigned int& timeoutSeconds) {
    char buffer[32] = { 0 };
    DWORD size = sizeof(buffer);
    if (RegGetValueA(HKEY_CURRENT_USER, "Control Panel\\Desktop", "ScreenSaveActive",
                     RRF_RT_REG_SZ, NULL, buffer, &size) != ERROR_SUCCESS) {
        return false;
    }
    enabled = (std::string(buffer) == "1");

    size = sizeof(buffer);
    if (RegGetValueA(HKEY_CURRENT_USER, "Control Panel\\Desktop", "ScreenSaveTimeOut",
                     RRF_RT_REG_SZ, NULL, buffer, &size) != ERROR_SUCCESS) {
        return false;
    }
    timeoutSeconds = static_cast<unsigned int>(std::atoi(buffer));
    return true;
}

// Escapes a string for a JSON string literal (names are validated ASCII,
// but be defensive anyway).
std::string JsonEscape(const std::string& text) {
    std::string escaped;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '"' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

// Queries GET_STATUS from the daemon and parses the response via the
// shared protocol layer. Returns false when the daemon is not reachable.
bool QueryDaemonStatus(protocol::DaemonStatus& info) {
    std::string response;
    if (!ipc::SendRequest(protocol::kReqGetStatus, response)) {
        return false;
    }
    protocol::ParseStatusResponse(response, info);
    return true;
}

} // namespace

int cli::CmdStatus() {
    protocol::DaemonStatus info;
    const bool running = ipc::IsDaemonRunning();
    bool lostContact = false;

    if (running && !QueryDaemonStatus(info)) {
        lostContact = true;
    }

    // Gather everything both output modes need, then render.
    std::string registeredPath;
    const bool autostartEnabled = autostart::Query(registeredPath);
    const bool autostartStale = autostartEnabled && !registeredPath.empty() &&
                                GetFileAttributesA(registeredPath.c_str()) == INVALID_FILE_ATTRIBUTES;

    unsigned int sleepAcSeconds = 0;
    unsigned int sleepDcSeconds = 0;
    unsigned int hibernateAcSeconds = 0;
    const bool haveSleepAc = ReadPowerSeconds(true, kStandbyTimeout, sleepAcSeconds);
    const bool haveSleepDc = ReadPowerSeconds(false, kStandbyTimeout, sleepDcSeconds);
    const bool haveHibAc = ReadPowerSeconds(true, kHibernateTimeout, hibernateAcSeconds);
    bool screenSaverEnabled = false;
    bool haveScreenSaver = false;
    unsigned int screenSaverSeconds = 0;
    haveScreenSaver = ReadScreenSaver(screenSaverEnabled, screenSaverSeconds);

    const std::vector<std::string> watchList = config::ReadWatchList();
    const std::set<std::string> runningNames = process::FindRunningImages(watchList);

    // -----------------------------------------------------------------------------
    // JSON output (--json, F3)
    // -----------------------------------------------------------------------------
    if (cli::JsonOutput()) {
        std::printf("{\n");
        std::printf("  \"daemon\": \"%s\",\n", (running && !lostContact) ? "running" : "not running");
        if (running && lostContact) {
            std::printf("  \"error\": \"lost contact with the daemon\",\n");
        }
        std::printf("  \"elevated\": %s,\n", info.elevated ? "true" : "false");
        std::printf("  \"keepAwake\": {\n");
        std::printf("    \"active\": %s,\n", (info.manual || info.app) ? "true" : "false");
        std::printf("    \"manual\": %s,\n", info.manual ? "true" : "false");
        std::printf("    \"app\": %s,\n", info.app ? "true" : "false");
        std::printf("    \"expiresInSeconds\": %d\n", info.manualRemaining);
        std::printf("  },\n");
        std::printf("  \"screen\": {\n");
        std::printf("    \"enabled\": %s,\n", info.screen ? "true" : "false");
        std::printf("    \"expiresInSeconds\": %d\n", info.screenRemaining);
        std::printf("  },\n");
        std::printf("  \"resumeAgoSeconds\": %d,\n", info.resumeAgo);
        std::printf("  \"autostart\": \"%s\",\n",
                    autostartStale ? "stale" : (autostartEnabled ? "enabled" : "disabled"));
        std::printf("  \"powerPlan\": {\n");
        std::printf("    \"sleepAcSeconds\": %s,\n", haveSleepAc ? std::to_string(sleepAcSeconds).c_str() : "null");
        std::printf("    \"sleepDcSeconds\": %s,\n", haveSleepDc ? std::to_string(sleepDcSeconds).c_str() : "null");
        std::printf("    \"hibernateAcSeconds\": %s,\n", haveHibAc ? std::to_string(hibernateAcSeconds).c_str() : "null");
        std::printf("    \"screensaverEnabled\": %s,\n", haveScreenSaver ? (screenSaverEnabled ? "true" : "false") : "null");
        std::printf("    \"screensaverTimeoutSeconds\": %s\n", haveScreenSaver ? std::to_string(screenSaverSeconds).c_str() : "null");
        std::printf("  },\n");
        const std::string requests = info.active
            ? (info.screen ? "system+display" : "system")
            : "";
        std::printf("  \"requests\": \"%s\",\n", requests.c_str());
        std::printf("  \"watch\": [\n");
        for (size_t i = 0; i < watchList.size(); ++i) {
            const bool isRunning = runningNames.count(config::ToLowerAscii(watchList[i])) > 0;
            std::printf("    { \"name\": \"%s\", \"running\": %s }%s\n",
                        JsonEscape(watchList[i]).c_str(), isRunning ? "true" : "false",
                        (i + 1 < watchList.size()) ? "," : "");
        }
        std::printf("  ]\n");
        std::printf("}\n");
        return lostContact ? 1 : 0;
    }

    // -----------------------------------------------------------------------------
    // Text output
    // -----------------------------------------------------------------------------
    console::Printf(console::kColorReset, "Daemon:     ");
    if (!running) {
        console::PrintLine(console::kColorRed, "not running");
    } else {
        // The status response is needed before the line can be completed,
        // because an elevated daemon is flagged in the same line.
        console::Printf(console::kColorGreen, "running");
        if (lostContact) {
            console::Printf(console::kColorReset, "\n");
            console::PrintLine(console::kColorRed, "Error: lost contact with the daemon.");
            return 1;
        }
        if (info.elevated) {
            // Explain why a non-elevated shell may be unable to control it.
            console::Printf(console::kColorYellow, " (elevated)");
        }
        console::Printf(console::kColorReset, "\n");
    }

    // Keep-awake and screen states are authoritative only while the daemon
    // runs; without it they are inactive by definition. The version check is
    // therefore skipped as well: with no daemon there is no version to
    // compare, and warning about a "missing" version would be misleading.
    if (running) {
        cli::CheckDaemonVersion(info.version);
    }

    if (!info.manual && !info.app) {
        // When only the screen keep-awake is on, the system idle sleep is
        // blocked as a side effect (the display must stay on), so spell
        // that out instead of claiming default power behavior.
        console::Printf(console::kColorReset, "Keep-awake: ");
        console::Printf(console::kColorYellow, "INACTIVE");
        if (info.screen) {
            console::Printf(console::kColorReset,
                            " (screen keep-awake is also blocking idle sleep).\n");
        } else {
            console::Printf(console::kColorReset,
                            " (the system uses its default power behavior).\n");
        }
    } else {
        console::Printf(console::kColorReset, "Keep-awake: ");
        console::Printf(console::kColorGreen, "ENABLED");
        // Explain why the daemon is currently blocking sleep. A timed period
        // (F1) also shows how long it still runs.
        const std::string expires = (info.manualRemaining > 0)
            ? ", expires in " + timeparse::FormatDuration(static_cast<unsigned int>(info.manualRemaining))
            : std::string();
        if (info.manual && info.app) {
            console::Printf(console::kColorReset, " (manually set and a watched application is running%s)", expires.c_str());
        } else if (info.manual) {
            console::Printf(console::kColorReset, " (manually set%s)", expires.c_str());
        } else {
            console::Printf(console::kColorReset, " (a watched application is running)");
        }
        console::Printf(console::kColorReset, ".\n");
    }

    if (info.screen) {
        console::Printf(console::kColorReset, "Screen:     ");
        console::Printf(console::kColorGreen, "ENABLED");
        if (info.screenRemaining > 0) {
            console::Printf(console::kColorReset,
                            " (screen off, screensaver and idle lock are blocked; expires in %s).\n",
                            timeparse::FormatDuration(
                                static_cast<unsigned int>(info.screenRemaining)).c_str());
        } else {
            console::Printf(console::kColorReset,
                            " (screen off, screensaver and idle lock are blocked).\n");
        }
    } else {
        console::Printf(console::kColorReset, "Screen:     ");
        console::Printf(console::kColorYellow, "INACTIVE");
        console::Printf(console::kColorReset,
                        " (the screen may turn off on idle).\n");
    }

    // Power event awareness (F13): shown once the daemon has seen a resume;
    // daemons without F13 never report the field (it stays -1).
    if (info.resumeAgo >= 0) {
        console::Printf(console::kColorReset, "Last resume:");
        console::Printf(console::kColorGreen, "%s ago",
                        timeparse::FormatDuration(
                            static_cast<unsigned int>(info.resumeAgo)).c_str());
        console::Printf(console::kColorReset,
                        " (the state was re-applied after system sleep).\n");
    }

    // Daemon autostart state comes from the registry (per-user Run key).
    // A stale entry (the registered daemon exe no longer exists, e.g. the
    // folder was moved) is called out with a repair hint.
    console::Printf(console::kColorReset, "Autostart:  ");
    if (!autostartEnabled) {
        console::Printf(console::kColorReset, "DISABLED");
        console::Printf(console::kColorReset, " (use 'awake daemon enable' to enable).\n");
    } else if (autostartStale) {
        console::Printf(console::kColorRed, "STALE");
        console::Printf(console::kColorReset,
                        " (the registered daemon path no longer exists - run 'awake daemon enable' to fix).\n");
    } else {
        console::Printf(console::kColorGreen, "ENABLED");
        console::Printf(console::kColorReset, " (the daemon starts at logon).\n");
    }

    // System power facts (F6): what the active power plan would do on idle,
    // and which execution state requirements the daemon currently holds.
    if (haveSleepAc || haveSleepDc) {
        std::string plan = "sleep after " + timeparse::FormatDuration(sleepAcSeconds) + " (AC)";
        if (haveSleepDc) {
            plan += " / " + timeparse::FormatDuration(sleepDcSeconds) + " (DC)";
        }
        if (haveHibAc) {
            plan += "; hibernate after " + timeparse::FormatDuration(hibernateAcSeconds) + " (AC)";
        }
        console::Printf(console::kColorReset, "Power plan: %s\n", plan.c_str());
    }
    if (haveScreenSaver) {
        if (screenSaverEnabled && screenSaverSeconds > 0) {
            console::Printf(console::kColorReset, "Screensaver: after %s\n",
                            timeparse::FormatDuration(screenSaverSeconds).c_str());
        } else {
            console::Printf(console::kColorReset, "Screensaver: disabled\n");
        }
    }
    if (running && info.active) {
        console::Printf(console::kColorReset, "Requests:   ");
        console::Printf(console::kColorGreen,
                        info.screen ? "system + display" : "system");
        console::Printf(console::kColorReset, " held by the daemon.\n");
    }

    // The watch list always comes from the configuration file, so it is
    // shown even when the daemon is not running (checked locally then).
    console::Printf(console::kColorReset, "Watch list (%s):\n", config::GetConfigPath().c_str());
    if (watchList.empty()) {
        console::PrintLine(console::kColorYellow, "  (empty - use 'awake add <imagename>' to add entries)");
        return 0;
    }
    for (size_t i = 0; i < watchList.size(); ++i) {
        if (runningNames.count(config::ToLowerAscii(watchList[i])) > 0) {
            console::Printf(console::kColorGreen, "  %s (running)\n", watchList[i].c_str());
        } else {
            console::Printf(console::kColorRed, "  %s (not running)\n", watchList[i].c_str());
        }
    }
    return 0;
}
