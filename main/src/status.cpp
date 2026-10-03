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
// state and the watch list (green entries are running, red entries are
// not) in one view. The command never starts the daemon; when the daemon
// is not running, the keep-awake states are inactive by definition and the
// watch list is checked locally against the running processes.
// =============================================================================

#include "commands.h"

#include <windows.h>

#include <set>
#include <string>
#include <vector>

#include "config.h"
#include "console.h"
#include "ipc.h"
#include "process.h"
#include "protocol.h"

// Queries GET_STATUS from the daemon and parses the response via the
// shared protocol layer. Returns false when the daemon is not reachable.
static bool QueryDaemonStatus(protocol::DaemonStatus& info) {
    std::string response;
    if (!ipc::SendRequest(protocol::kReqGetStatus, response)) {
        return false;
    }
    protocol::ParseStatusResponse(response, info);
    return true;
}

int cli::CmdStatus() {
    console::Printf(console::kColorReset, "Daemon:     ");
    const bool running = ipc::IsDaemonRunning();
    if (running) {
        console::PrintLine(console::kColorGreen, "running");
    } else {
        console::PrintLine(console::kColorRed, "not running");
    }

    // Keep-awake and screen states are authoritative only while the daemon
    // runs; without it they are inactive by definition.
    protocol::DaemonStatus info;
    if (running && !QueryDaemonStatus(info)) {
        console::PrintLine(console::kColorRed, "Error: lost contact with the daemon.");
        return 1;
    }

    if (!info.manual && !info.app) {
        // When only the screen keep-awake is on, the system idle sleep is
        // blocked as a side effect (the display must stay on), so spell
        // that out instead of claiming default power behavior.
        if (info.screen) {
            console::Printf(console::kColorReset, "Keep-awake: %sINACTIVE%s (screen keep-awake is also blocking idle sleep).\n",
                            console::kColorYellow, console::kColorReset);
        } else {
            console::Printf(console::kColorReset, "Keep-awake: %sINACTIVE%s (the system uses its default power behavior).\n",
                            console::kColorYellow, console::kColorReset);
        }
    } else {
        console::Printf(console::kColorReset, "Keep-awake: ");
        console::Printf(console::kColorGreen, "ENABLED");
        // Explain why the daemon is currently blocking sleep.
        if (info.manual && info.app) {
            console::Printf(console::kColorReset, " (manually set and a watched application is running)");
        } else if (info.manual) {
            console::Printf(console::kColorReset, " (manually set)");
        } else {
            console::Printf(console::kColorReset, " (a watched application is running)");
        }
        console::Printf(console::kColorReset, ".\n");
    }

    if (info.screen) {
        console::Printf(console::kColorReset, "Screen:     ");
        console::Printf(console::kColorGreen, "ENABLED");
        console::Printf(console::kColorReset,
                        " (screen off, screensaver and idle lock are blocked).\n");
    } else {
        console::Printf(console::kColorReset, "Screen:     ");
        console::Printf(console::kColorYellow, "INACTIVE");
        console::Printf(console::kColorReset,
                        " (the screen may turn off on idle).\n");
    }

    // The watch list always comes from the configuration file, so it is
    // shown even when the daemon is not running (checked locally then).
    const std::vector<std::string> watchList = config::ReadWatchList();
    console::Printf(console::kColorReset, "Watch list (%s):\n", config::GetConfigPath().c_str());
    if (watchList.empty()) {
        console::PrintLine(console::kColorYellow, "  (empty - use 'awake add <imagename>' to add entries)");
        return 0;
    }
    const std::set<std::string> runningNames = process::FindRunningImages(watchList);
    for (size_t i = 0; i < watchList.size(); ++i) {
        if (runningNames.count(config::ToLowerAscii(watchList[i])) > 0) {
            console::Printf(console::kColorGreen, "  %s (running)\n", watchList[i].c_str());
        } else {
            console::Printf(console::kColorRed, "  %s (not running)\n", watchList[i].c_str());
        }
    }
    return 0;
}
