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
// commands.cpp - implementation of the client commands except "status"
// (which lives in status.cpp).
// =============================================================================

#include "commands.h"

#include <windows.h>

#include <cstdio>
#include <iostream>
#include <string>

#include "autostart.h"
#include "config.h"
#include "console.h"
#include "ipc.h"
#include "protocol.h"
#include "version.h"

// -----------------------------------------------------------------------------
// Help text
// -----------------------------------------------------------------------------

// Prints the colored usage help. This is shown for "awake help", for
// "awake" without arguments and as a hint after unknown commands.
void cli::PrintHelp() {
    console::Printf(console::kColorCyan, "awake %s - keep the system awake\n", version::kVersion);
    console::PrintLine(console::kColorReset, "");
    console::PrintLine(console::kColorReset, "Usage:");
    console::Printf(console::kColorReset,   "  awake help              %sShow this help message.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake 0                 %sDisable keep-awake (restore default power behavior).\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake 1                 %sEnable keep-awake (block idle sleep and hibernation).\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake status            %sShow daemon, keep-awake, screen and watch list status.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake screen on         %sKeep the screen on (blocks screen off, screensaver and idle lock).\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake screen off        %sStop keeping the screen on.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake daemon on         %sStart the background daemon.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake daemon off        %sStop the background daemon.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake daemon enable     %sStart the daemon automatically at logon.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake daemon disable    %sDo not start the daemon automatically at logon.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake reload            %sRe-read the configuration file immediately.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake reset             %sReset the configuration file (asks for confirmation).\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake add <imagename>   %sAdd an image name to the watch list.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake del <imagename>   %sRemove an image name from the watch list.\n", console::kColorReset);
    console::PrintLine(console::kColorReset, "");
    console::PrintLine(console::kColorReset, "Notes:");
    console::PrintLine(console::kColorReset, "  - Keep-awake only blocks idle sleep/hibernation. Manual sleep via the");
    console::PrintLine(console::kColorReset, "    power button, the Start menu or other applications still works.");
    console::PrintLine(console::kColorReset, "  - While a watched application (see awake.ini) is running, the daemon");
    console::PrintLine(console::kColorReset, "    blocks idle sleep/hibernation as well.");
    console::PrintLine(console::kColorReset, "  - 'awake 0' and 'awake 1' start the daemon automatically when needed;");
    console::PrintLine(console::kColorReset, "    'awake status' never does.");
    console::PrintLine(console::kColorReset, "  - Command aliases: '?', 'h' = help; 'off', 'disable', 'stop' = 0;");
    console::PrintLine(console::kColorReset, "    'on', 'enable', 'start' = 1; 'st' = status; 'scr' = screen;");
    console::PrintLine(console::kColorReset, "    'screen 1/0' = 'screen on/off'; 'd' = daemon; 'daemon 1/start' =");
    console::PrintLine(console::kColorReset, "    'daemon on'; 'daemon 0/stop' = 'daemon off'.");
}

// -----------------------------------------------------------------------------
// Version compatibility
// -----------------------------------------------------------------------------

// Extracts the major version number from a "X.Y.Z-suffix" string.
// Returns false when the string cannot be parsed.
static bool ParseMajorVersion(const std::string& version, int& majorOut) {
    const size_t dot = version.find('.');
    if (dot == std::string::npos) {
        return false;
    }
    try {
        majorOut = std::stoi(version.substr(0, dot));
    } catch (...) {
        return false;
    }
    return true;
}

bool cli::CheckDaemonVersion(const std::string& daemonVersion) {
    if (daemonVersion.empty()) {
        // Very old daemons did not report a version; accept but warn.
        console::PrintLine(console::kColorYellow,
                           "Warning: the daemon did not report its version; it may be outdated.");
        return true;
    }
    if (daemonVersion == version::kVersion) {
        return true; // exact match - the normal case
    }

    int clientMajor = 0;
    int daemonMajor = 0;
    const bool parsed = ParseMajorVersion(version::kVersion, clientMajor) &&
                        ParseMajorVersion(daemonVersion, daemonMajor);
    if (parsed && clientMajor != daemonMajor) {
        console::Printf(console::kColorRed,
                        "Error: version mismatch - client %s, daemon %s. "
                        "Please update both executables from the same release.\n",
                        version::kVersion, daemonVersion.c_str());
        return false;
    }
    console::Printf(console::kColorYellow,
                    "Warning: version mismatch - client %s, daemon %s. "
                    "Some features may not work correctly.\n",
                    version::kVersion, daemonVersion.c_str());
    return true;
}

// -----------------------------------------------------------------------------
// Daemon management helpers
// -----------------------------------------------------------------------------

// Makes sure the daemon is running, starting it if necessary, and verifies
// the version handshake. Only used by the commands that must talk to the
// daemon (0, 1, screen on/off).
static bool EnsureDaemonRunning() {
    std::string daemonVersion;
    if (!ipc::PingDaemon(daemonVersion)) {
        console::PrintLine(console::kColorYellow, "Daemon is not running. Starting daemon...");
        if (!ipc::StartDaemon()) {
            console::Printf(console::kColorRed, "Error: failed to start '%s'.\n", ipc::kDaemonExeName);
            return false;
        }
        if (!ipc::WaitForDaemon(5000)) {
            console::PrintLine(console::kColorRed, "Error: the daemon was started but does not respond.");
            return false;
        }
        if (!ipc::PingDaemon(daemonVersion)) {
            console::PrintLine(console::kColorRed, "Error: the daemon stopped responding right after starting.");
            return false;
        }
    }
    return cli::CheckDaemonVersion(daemonVersion);
}

// -----------------------------------------------------------------------------
// Command implementations
// -----------------------------------------------------------------------------

// "awake 0" / "awake 1": forward the request to the daemon.
int cli::CmdSetKeepAwake(bool enable) {
    if (!EnsureDaemonRunning()) {
        return 1;
    }
    std::string response;
    if (!ipc::SendRequest(protocol::MakeKeepAwakeRequest(enable), response)) {
        console::PrintLine(console::kColorRed, "Error: lost contact with the daemon.");
        return 1;
    }
    if (response.compare(0, 2, "OK") == 0) {
        if (enable) {
            console::PrintLine(console::kColorGreen, "Keep-awake ENABLED. Idle sleep/hibernation is now blocked.");
        } else {
            console::PrintLine(console::kColorGreen, "Keep-awake DISABLED. The system uses its default power behavior.");
        }
        return 0;
    }
    console::Printf(console::kColorRed, "Error: the daemon rejected the request: %s", response.c_str());
    return 1;
}

// "awake screen on" / "awake screen off": forward the request to the daemon.
int cli::CmdSetScreenKeepAwake(bool enable) {
    if (!EnsureDaemonRunning()) {
        return 1;
    }
    std::string response;
    if (!ipc::SendRequest(protocol::MakeScreenRequest(enable), response)) {
        console::PrintLine(console::kColorRed, "Error: lost contact with the daemon.");
        return 1;
    }
    if (response.compare(0, 2, "OK") == 0) {
        if (enable) {
            console::PrintLine(console::kColorGreen, "Screen keep-awake ENABLED. Screen off, screensaver and idle lock are now blocked.");
        } else {
            console::PrintLine(console::kColorGreen, "Screen keep-awake DISABLED. The screen behaves according to the power settings.");
        }
        return 0;
    }
    console::Printf(console::kColorRed, "Error: the daemon rejected the request: %s", response.c_str());
    return 1;
}

// "awake daemon on": launch the daemon unless it is already running.
int cli::CmdDaemonOn() {
    if (ipc::IsDaemonRunning()) {
        console::PrintLine(console::kColorYellow, "The daemon is already running.");
        return 0;
    }
    if (!config::EnsureConfigExists()) {
        console::PrintLine(console::kColorRed, "Error: cannot create the configuration file (awake.ini).");
        return 1;
    }
    if (!ipc::StartDaemon()) {
        console::Printf(console::kColorRed, "Error: failed to start '%s'.\n", ipc::kDaemonExeName);
        return 1;
    }
    if (!ipc::WaitForDaemon(5000)) {
        console::PrintLine(console::kColorRed, "Error: the daemon was started but does not respond.");
        return 1;
    }
    // Verify the version handshake; the daemon stays running either way.
    std::string daemonVersion;
    if (ipc::PingDaemon(daemonVersion)) {
        cli::CheckDaemonVersion(daemonVersion);
    }
    console::PrintLine(console::kColorGreen, "The daemon is now running.");
    return 0;
}

// "awake daemon off": politely ask the daemon to shut down.
int cli::CmdDaemonOff() {
    if (!ipc::IsDaemonRunning()) {
        console::PrintLine(console::kColorYellow, "The daemon is not running.");
        return 0;
    }
    std::string response;
    if (!ipc::SendRequest(protocol::kReqShutdown, response) || response.compare(0, 2, "OK") != 0) {
        console::PrintLine(console::kColorRed, "Error: the daemon did not acknowledge the shutdown request.");
        return 1;
    }
    console::PrintLine(console::kColorGreen, "The daemon has been stopped.");
    return 0;
}

// "awake daemon enable": register the daemon for autostart at logon.
// Re-running this command also REPAIRS a stale entry whose registered
// daemon path no longer exists (e.g. the folder was moved) - only a
// healthy, existing registration short-circuits to "already enabled".
int cli::CmdDaemonEnable() {
    std::string registeredPath;
    if (autostart::Query(registeredPath) && !registeredPath.empty() &&
        GetFileAttributesA(registeredPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        console::PrintLine(console::kColorYellow, "Daemon autostart is already enabled.");
        return 0;
    }
    std::string error;
    if (!autostart::Enable(error)) {
        console::Printf(console::kColorRed, "Error: %s.\n", error.c_str());
        return 1;
    }
    console::PrintLine(console::kColorGreen, "Daemon autostart ENABLED. The daemon will start at logon.");
    return 0;
}

// "awake daemon disable": remove the daemon autostart registration.
int cli::CmdDaemonDisable() {
    if (!autostart::IsEnabled()) {
        console::PrintLine(console::kColorYellow, "Daemon autostart is already disabled.");
        return 0;
    }
    std::string error;
    if (!autostart::Disable(error)) {
        console::Printf(console::kColorRed, "Error: %s.\n", error.c_str());
        return 1;
    }
    console::PrintLine(console::kColorGreen, "Daemon autostart DISABLED. The daemon will not start at logon.");
    return 0;
}

// "awake reload": ask the daemon to re-read the configuration file now.
int cli::CmdReload() {
    if (!ipc::IsDaemonRunning()) {
        console::PrintLine(console::kColorYellow, "Daemon is not running.");
        console::PrintLine(console::kColorReset,  "There is nothing to reload; the configuration is read when the daemon starts.");
        return 0;
    }
    std::string response;
    if (!ipc::SendRequest(protocol::kReqReload, response) || response.compare(0, 2, "OK") != 0) {
        console::PrintLine(console::kColorRed, "Error: the daemon did not acknowledge the reload request.");
        return 1;
    }
    console::PrintLine(console::kColorGreen, "Configuration reloaded.");
    return 0;
}

// "awake reset": restore the configuration file to its initial state. The
// user must confirm the destructive operation with an explicit "y"/"yes";
// anything else (including an empty answer, EOF or Ctrl+C style aborts)
// leaves the file untouched.
int cli::CmdReset() {
    if (!config::EnsureConfigExists()) {
        console::PrintLine(console::kColorRed, "Error: cannot create the configuration file (awake.ini).");
        return 1;
    }

    // Warning with the exact file path that is about to be wiped.
    console::Printf(console::kColorRed,
                    "WARNING: the configuration file\n  %s\nwill be RESET to its initial state.\n",
                    config::GetConfigPath().c_str());
    console::PrintLine(console::kColorRed,
                       "All existing watch list entries and any manual edits will be PERMANENTLY LOST.");
    console::Printf(console::kColorYellow, "Are you sure you want to continue? [y/N]: ");

    std::string answer;
    if (!std::getline(std::cin, answer)) {
        answer.clear(); // stdin closed: treat as "no"
    }
    const std::string normalized = config::ToLowerAscii(config::TrimText(answer));
    if (normalized != "y" && normalized != "yes") {
        console::PrintLine(console::kColorYellow, "Aborted. The configuration file was not changed.");
        return 0;
    }

    if (!config::ResetConfig()) {
        console::PrintLine(console::kColorRed, "Error: cannot write the configuration file (awake.ini).");
        return 1;
    }
    console::PrintLine(console::kColorGreen, "The configuration file has been reset to its initial state.");

    // Let a running daemon pick up the change immediately instead of
    // waiting for the next 30 second poll.
    if (ipc::IsDaemonRunning()) {
        std::string response;
        if (ipc::SendRequest(protocol::kReqReload, response) && response.compare(0, 2, "OK") == 0) {
            console::PrintLine(console::kColorReset, "The running daemon has reloaded the configuration.");
        }
    }
    return 0;
}

// "awake add <imagename>" / "awake del <imagename>": edit the watch list.
int cli::CmdEditWatchList(const char* command, const char* imageName) {
    const std::string name(imageName);
    if (!config::IsValidImageName(name)) {
        console::Printf(console::kColorRed,
                        "Error: '%s' is not a valid image name (plain file name without path or wildcards).\n",
                        imageName);
        return 1;
    }
    if (!config::EnsureConfigExists()) {
        console::PrintLine(console::kColorRed, "Error: cannot create the configuration file (awake.ini).");
        return 1;
    }

    if (std::string(command) == "add") {
        if (config::AddWatchEntry(name)) {
            console::Printf(console::kColorGreen, "Added '%s' to the watch list.\n", imageName);
            return 0;
        }
        console::Printf(console::kColorYellow, "'%s' is already in the watch list.\n", imageName);
        return 0;
    }

    if (config::RemoveWatchEntry(name)) {
        console::Printf(console::kColorGreen, "Removed '%s' from the watch list.\n", imageName);
        return 0;
    }
    console::Printf(console::kColorYellow, "'%s' was not found in the watch list.\n", imageName);
    return 0;
}
