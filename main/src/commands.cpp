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

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include "autostart.h"
#include "config.h"
#include "logexport.h"
#include "console.h"
#include "ipc.h"
#include "license.h"
#include "protocol.h"
#include "timeparse.h"
#include "version.h"

// -----------------------------------------------------------------------------
// Machine-readable output (F3)
// -----------------------------------------------------------------------------

namespace {

// Set by the global --json option; "list" and "status" emit JSON when set.
bool g_jsonOutput = false;

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

} // namespace

void cli::SetJsonOutput(bool enabled) {
    g_jsonOutput = enabled;
}

bool cli::JsonOutput() {
    return g_jsonOutput;
}

// -----------------------------------------------------------------------------
// Help text
// -----------------------------------------------------------------------------

// Prints the colored usage help. This is shown for "awake help", for
// Formats "now + seconds" as a local "HH:MM" clock time, used by the
// confirmation messages of a timed keep-awake.
static std::string FormatLocalClock(unsigned int seconds) {
    const std::time_t target = std::time(NULL) + static_cast<std::time_t>(seconds);
    std::tm local;
    if (localtime_s(&local, &target) != 0) {
        return "?";
    }
    char buffer[16] = { 0 };
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", local.tm_hour, local.tm_min);
    return std::string(buffer);
}

// One row of the alias table: the canonical command and every other spelling
// that dispatches to it. Commands without an alias are not listed here; they
// are already shown in the usage block.
struct AliasRow {
    const char* command;
    const char* aliases;
};

const AliasRow kAliasRows[] = {
    { "awake help",        "?, h, /?, -h, --help" },
    { "awake aliases",     "a, alias" },
    { "awake 0",           "off, disable, stop" },
    { "awake 1",           "on, enable, start" },
    { "awake status",      "st" },
    { "awake screen",      "scr" },
    { "awake screen on",   "screen 1" },
    { "awake screen off",  "screen 0" },
    { "awake daemon",      "d" },
    { "awake daemon on",   "daemon 1, daemon start" },
    { "awake daemon off",  "daemon 0, daemon stop" },
    { "awake list",        "l" },
    { "awake log",         "logs" },
};

const size_t kAliasRowCount = sizeof(kAliasRows) / sizeof(kAliasRows[0]);

// One row of the command table: the canonical spelling (without the "awake "
// prefix), a one-line description and whether the Usage block lists it.
// Parents such as "daemon" are used for suggestions only; their leaf commands
// are what the Usage block shows.
struct CommandEntry {
    const char* command;
    const char* description;
    bool inUsage;
};

const CommandEntry kCommandEntries[] = {
    { "help",            "Show this help message.", true },
    { "version",         "Show the version.", true },
    { "aliases",         "Show every command alias (same as 'awake a').", true },
    { "0",               "Disable keep-awake (restore default power behavior).", true },
    { "1",               "Enable keep-awake (block idle sleep and hibernation).", true },
    { "1 for <period>",  "Enable keep-awake for a limited time, e.g. 30m, 1h30m, 2h.", true },
    { "1 until <HH:MM>", "Enable keep-awake until that clock time (tomorrow if passed).", true },
    { "status",          "Show daemon, keep-awake, screen and watch list status.", true },
    { "screen",          "Turn the screen keep-awake on or off (on / off).", false },
    { "screen on",       "Keep the screen on (blocks screen off, screensaver and idle lock).", true },
    { "screen on for <period>", "Keep the screen on for a limited time, e.g. 2h.", true },
    { "screen off",      "Stop keeping the screen on.", true },
    { "daemon",          "Start, stop or configure the daemon (on / off / enable / disable).", false },
    { "daemon on",       "Start the background daemon.", true },
    { "daemon off",      "Stop the background daemon.", true },
    { "daemon enable",   "Start the daemon automatically at logon.", true },
    { "daemon disable",  "Do not start the daemon automatically at logon.", true },
    { "reload",          "Re-read the configuration file immediately.", true },
    { "reset",           "Reset the configuration file (asks for confirmation).", true },
    { "add <imagename>", "Add an image name to the watch list.", true },
    { "del <imagename>", "Remove an image name from the watch list.", true },
    { "list",            "List the watched applications (use --json for JSON).", true },
    { "log",             "Show or change daemon logging (on / off / level / max-files / max-size / export).", false },
    { "log on",          "Turn daemon logging on.", true },
    { "log off",         "Turn daemon logging off.", true },
    { "log level <level>", "Set the log level (info or debug).", true },
    { "log max-files <n>", "Set how many log files are kept (1-100).", true },
    { "log max-size <mb>", "Set the per-file size limit in MB (1-100).", true },
    { "log export [path]", "Export the log files as a .zip (default: awake-logs.zip in the current directory).", true },
};

const size_t kCommandEntryCount = sizeof(kCommandEntries) / sizeof(kCommandEntries[0]);

// Commands whose command column is longer than this are not expected; the
// column width below keeps every row on a single line.
const int kCommandColumnWidth = 18;

// Returns the spelling without the argument placeholder ("add <imagename>"
// becomes "add"), so that suggestions compare command words only.
std::string CommandWords(const char* command) {
    std::string words(command);
    const size_t placeholder = words.find(" <");
    if (placeholder != std::string::npos) {
        words.erase(placeholder);
    }
    return words;
}

// Classic Levenshtein distance; the inputs are short command lines.
size_t EditDistance(const std::string& left, const std::string& right) {
    const size_t leftSize = left.size();
    const size_t rightSize = right.size();
    std::vector<size_t> previous(rightSize + 1);
    std::vector<size_t> current(rightSize + 1);
    for (size_t j = 0; j <= rightSize; ++j) {
        previous[j] = j;
    }
    for (size_t i = 1; i <= leftSize; ++i) {
        current[0] = i;
        for (size_t j = 1; j <= rightSize; ++j) {
            const size_t cost = (left[i - 1] == right[j - 1]) ? 0 : 1;
            size_t best = previous[j] + 1;                 // deletion
            if (current[j - 1] + 1 < best) {               // insertion
                best = current[j - 1] + 1;
            }
            if (previous[j - 1] + cost < best) {           // substitution
                best = previous[j - 1] + cost;
            }
            current[j] = best;
        }
        previous.swap(current);
    }
    return previous[rightSize];
}

// How many typos are tolerated for a command of the given length. Short
// commands are easy to confuse with unrelated words, so they allow fewer.
size_t AllowedDistance(size_t length) {
    if (length <= 3) {
        return 1;
    }
    if (length <= 6) {
        return 2;
    }
    return 3;
}

void PrintCommandRow(const char* command, const char* description) {
    char column[64] = { 0 };
    std::snprintf(column, sizeof(column), "awake %s", command);
    console::Printf(console::kColorReset, "  %-*s %s\n", kCommandColumnWidth + 6, column, description);
}

// "awake" without arguments and as a hint after unknown commands.
void cli::PrintHelp() {
    console::Printf(console::kColorCyan, "awake %s - keep the system awake\n", version::kVersionDisplay);
    console::PrintLine(console::kColorReset, "");
    console::PrintLine(console::kColorReset, "Usage:");
    for (size_t i = 0; i < kCommandEntryCount; ++i) {
        if (kCommandEntries[i].inUsage) {
            PrintCommandRow(kCommandEntries[i].command, kCommandEntries[i].description);
        }
    }
    console::PrintLine(console::kColorReset, "");
    console::PrintLine(console::kColorReset, "Command aliases:");
    cli::PrintAliasTable();
    console::PrintLine(console::kColorReset, "  Commands that are not listed accept no alias.");
    console::PrintLine(console::kColorReset, "");
    console::PrintLine(console::kColorReset, "Notes:");
    console::PrintLine(console::kColorReset, "  - Keep-awake only blocks idle sleep/hibernation. Manual sleep via the");
    console::PrintLine(console::kColorReset, "    power button, the Start menu or other applications still works.");
    console::PrintLine(console::kColorReset, "  - While a watched application (see awake.ini) is running, the daemon");
    console::PrintLine(console::kColorReset, "    blocks idle sleep/hibernation as well.");
    console::PrintLine(console::kColorReset, "  - 'awake 0' and 'awake 1' start the daemon automatically when needed;");
    console::PrintLine(console::kColorReset, "    'awake status' never does.");
    console::PrintLine(console::kColorReset, "  - The first run shows the Apache License 2.0 and asks for acceptance;");
    console::PrintLine(console::kColorReset, "    the answer is stored in the [license] section of awake.ini.");
}

// Prints the alias table with the canonical command in the first column and
// every accepted spelling in the second one. The column width is fixed, so
// each command stays on exactly one output line.
void cli::PrintAliasTable() {
    for (size_t i = 0; i < kAliasRowCount; ++i) {
        console::Printf(console::kColorReset, "  %-24s %s\n",
                        kAliasRows[i].command, kAliasRows[i].aliases);
    }
}

// "awake aliases": documentation command, no side effects.
int cli::CmdAliases() {
    console::Printf(console::kColorCyan, "awake %s - command aliases\n", version::kVersionDisplay);
    console::PrintLine(console::kColorReset, "");
    cli::PrintAliasTable();
    console::PrintLine(console::kColorReset, "");
    console::PrintLine(console::kColorReset, "Commands that are not listed accept no alias.");
    console::PrintLine(console::kColorReset, "Run 'awake help' for the full usage.");
    return 0;
}

// "Did you mean" output for an input that did not dispatch. Nothing is
// executed: the block only names the closest commands and explains them.
// scope is empty at the top level, or the parent command ("daemon"/"screen")
// whose subcommands are being completed.
void cli::PrintCommandSuggestions(const std::string& input, const char* scope) {
    const std::string typed = config::TrimText(config::ToLowerAscii(input));
    const std::string scopeText = (scope != NULL) ? std::string(scope) : std::string();

    // Missing subcommand: list everything that belongs to the parent.
    if (!scopeText.empty() && typed == scopeText) {
        console::Printf(console::kColorRed, "Error: 'awake %s' needs a subcommand.\n", scopeText.c_str());
        console::PrintLine(console::kColorReset, "Available subcommands:");
        for (size_t i = 0; i < kCommandEntryCount; ++i) {
            const std::string command(kCommandEntries[i].command);
            if (command.size() > scopeText.size() + 1 &&
                command.compare(0, scopeText.size() + 1, scopeText + " ") == 0) {
                PrintCommandRow(kCommandEntries[i].command, kCommandEntries[i].description);
            }
        }
        console::PrintLine(console::kColorReset, "");
        console::PrintLine(console::kColorReset, "Run 'awake help' to see the complete command list.");
        return;
    }

    // Typo: rank the command table by edit distance and keep the closest few.
    struct Match {
        size_t distance;
        size_t index;
    };
    std::vector<Match> matches;
    for (size_t i = 0; i < kCommandEntryCount; ++i) {
        const std::string words = CommandWords(kCommandEntries[i].command);
        if (!scopeText.empty()) {
            // Only the subcommands of the given parent are candidates; the
            // parent itself is not suggested back to the user.
            if (words == scopeText ||
                words.compare(0, scopeText.size() + 1, scopeText + " ") != 0) {
                continue;
            }
        }
        const size_t distance = EditDistance(typed, words);
        const size_t smallest = (words.size() < typed.size()) ? words.size() : typed.size();
        if (distance <= AllowedDistance(smallest)) {
            Match match = { distance, i };
            matches.push_back(match);
        }
    }
    std::sort(matches.begin(), matches.end(), [](const Match& left, const Match& right) {
        if (left.distance != right.distance) {
            return left.distance < right.distance;
        }
        return left.index < right.index;
    });
    if (matches.size() > 3) {
        matches.resize(3);
    }

    console::Printf(console::kColorRed, "Error: unknown command 'awake %s'.\n", typed.c_str());
    if (matches.empty()) {
        console::PrintLine(console::kColorReset, "Run 'awake help' to see the available commands.");
        return;
    }
    console::PrintLine(console::kColorReset, "Did you mean:");
    for (size_t i = 0; i < matches.size(); ++i) {
        const CommandEntry& entry = kCommandEntries[matches[i].index];
        PrintCommandRow(entry.command, entry.description);
    }
    console::PrintLine(console::kColorReset, "");
    console::PrintLine(console::kColorReset, "Run 'awake help' to see the complete command list.");
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
        console::PrintErrLine(console::kColorYellow,
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
        const std::string message = std::string("Error: version mismatch - client ") +
                                    version::kVersion + ", daemon " + daemonVersion +
                                    ". Please update both executables from the same release.";
        console::PrintErrLine(console::kColorRed, message.c_str());
        return false;
    }
    const std::string warning = std::string("Warning: version mismatch - client ") +
                                version::kVersion + ", daemon " + daemonVersion +
                                ". Some features may not work correctly.";
    console::PrintErrLine(console::kColorYellow, warning.c_str());
    return true;
}

// -----------------------------------------------------------------------------
// Daemon management helpers
// -----------------------------------------------------------------------------

// Explains a failed CreateProcess for the daemon in terms the user can act
// on, instead of a generic "failed to start".
static void PrintStartDaemonError(unsigned long errorCode) {
    console::Printf(console::kColorRed, "Error: failed to start '%s' (Win32 error %lu).\n",
                    ipc::kDaemonExeName, errorCode);
    switch (errorCode) {
        case 2: // ERROR_FILE_NOT_FOUND
            console::PrintLine(console::kColorReset,
                               "  The daemon executable was not found next to the client; both files must be in the same folder.");
            break;
        case 5: // ERROR_ACCESS_DENIED
            console::PrintLine(console::kColorReset,
                               "  Access denied. A daemon that was started as administrator cannot be controlled from a normal shell; check 'awake status'.");
            break;
        case 193: // ERROR_BAD_EXE_FORMAT
        case 216: // ERROR_EXE_MACHINE_TYPE_MISMATCH
            console::PrintLine(console::kColorReset,
                               "  The daemon executable is not a valid binary for this system.");
            break;
        default:
            console::PrintLine(console::kColorReset,
                               "  The process could not be created; check the folder permissions.");
            break;
    }
}

// Makes sure the daemon is running, starting it if necessary, and verifies
// the version handshake. Only used by the commands that must talk to the
// daemon (0, 1, screen on/off).
static bool EnsureDaemonRunning() {
    std::string daemonVersion;
    if (!ipc::PingDaemon(daemonVersion)) {
        console::PrintLine(console::kColorYellow, "Daemon is not running. Starting daemon...");
        unsigned long startError = 0;
        if (!ipc::StartDaemon(&startError)) {
            PrintStartDaemonError(startError);
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

// Returns true when standard input is an interactive console. A redirected
// or piped stdin is not, and must never block on the license question.
static bool HasInteractiveStdin() {
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == NULL || input == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD mode = 0;
    return GetConsoleMode(input, &mode) != 0;
}

// First-run license gate: prints the embedded license and asks for consent
// unless the configuration file already records it. Non-interactive input
// is treated as "not accepted" so that scripts never hang.
bool cli::EnsureLicenseAccepted() {
    // Make sure the template exists first: the user is pointed at that file
    // in the prompts below, and the template documents the flag.
    if (!config::EnsureConfigExists()) {
        console::PrintLine(console::kColorRed, "Error: cannot create the configuration file (awake.ini).");
        return false;
    }
    if (config::IsLicenseAccepted()) {
        return true;
    }

    console::PrintLine(console::kColorCyan, "awake first run - license agreement");
    console::PrintLine(console::kColorReset, "");
    console::Printf(console::kColorReset, "%s\n", license::kText);
    // The AI statement belongs to the license block (F21): a user who reads
    // the terms also reads what kind of software it is. The generic reminder
    // at the end of a command is suppressed for this run, so it is not
    // printed twice.
    console::PrintLine(console::kColorReset, "");
    console::PrintLine(console::kColorYellow,
                       "AI-GENERATED SOFTWARE: this program was generated with AI assistance.");
    console::PrintLine(console::kColorYellow,
                       "Review the source code carefully before relying on it; the author is not");
    console::PrintLine(console::kColorYellow,
                       "responsible or liable for any consequences arising from its use.");
    console::SuppressAiNotice();
    console::PrintLine(console::kColorReset, "");
    console::Printf(console::kColorReset, "Official license page: %s\n", license::kUrl);
    console::PrintLine(console::kColorReset, "");
    console::Printf(console::kColorReset,
                    "Setting license_accepted = true in\n  %s\nmeans that you have read the license and agree to its terms.\n",
                    config::GetConfigPath().c_str());

    if (!HasInteractiveStdin()) {
        console::PrintLine(console::kColorRed,
                           "Error: standard input is not an interactive console, so the license cannot be accepted here.");
        console::PrintLine(console::kColorReset,
                           "Run awake from a console, or set 'license_accepted = true' in the [license] section of awake.ini.");
        return false;
    }

    console::Printf(console::kColorYellow, "Do you accept the %s? [y/N]: ", license::kName);
    std::fflush(stdout);

    std::string answer;
    if (!std::getline(std::cin, answer)) {
        answer.clear(); // stdin closed: treat as "no"
    }
    const std::string normalized = config::ToLowerAscii(config::TrimText(answer));
    if (normalized != "y" && normalized != "yes") {
        console::PrintLine(console::kColorYellow, "License not accepted. The command was not executed.");
        return false;
    }
    if (!config::SetLicenseAccepted(true)) {
        console::PrintLine(console::kColorRed, "Error: cannot write the configuration file (awake.ini).");
        return false;
    }
    console::PrintLine(console::kColorGreen, "License accepted; the choice is stored in awake.ini.");
    return true;
}

// "awake 0" / "awake 1": forward the request to the daemon.
int cli::CmdSetKeepAwake(bool enable, unsigned int seconds) {
    if (!EnsureDaemonRunning()) {
        return 1;
    }
    std::string response;
    if (!ipc::SendRequest(protocol::MakeKeepAwakeRequest(enable, seconds), response)) {
        console::PrintLine(console::kColorRed, "Error: lost contact with the daemon.");
        return 1;
    }
    if (response.compare(0, 2, "OK") == 0) {
        if (enable && seconds > 0) {
            console::Printf(console::kColorGreen, "Keep-awake ENABLED for %s",
                            timeparse::FormatDuration(seconds).c_str());
            console::Printf(console::kColorReset, " (until %s). Idle sleep/hibernation is now blocked.\n",
                            FormatLocalClock(seconds).c_str());
        } else if (enable) {
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
int cli::CmdSetScreenKeepAwake(bool enable, unsigned int seconds) {
    if (!EnsureDaemonRunning()) {
        return 1;
    }
    std::string response;
    if (!ipc::SendRequest(protocol::MakeScreenRequest(enable, seconds), response)) {
        console::PrintLine(console::kColorRed, "Error: lost contact with the daemon.");
        return 1;
    }
    if (response.compare(0, 2, "OK") == 0) {
        if (enable && seconds > 0) {
            console::Printf(console::kColorGreen, "Screen keep-awake ENABLED for %s",
                            timeparse::FormatDuration(seconds).c_str());
            console::Printf(console::kColorReset, " (until %s). Screen off, screensaver and idle lock are now blocked.\n",
                            FormatLocalClock(seconds).c_str());
        } else if (enable) {
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
    unsigned long startError = 0;
    if (!ipc::StartDaemon(&startError)) {
        PrintStartDaemonError(startError);
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

// -----------------------------------------------------------------------------
// "awake list" (F3)
// -----------------------------------------------------------------------------

// Prints the watch list, one image name per line, or a JSON array with
// --json. Deliberately plain: this is the script-facing command.
int cli::CmdList() {
    const std::vector<std::string> watchList = config::ReadWatchList();
    if (JsonOutput()) {
        std::printf("[\n");
        for (size_t i = 0; i < watchList.size(); ++i) {
            std::printf("  \"%s\"%s\n", JsonEscape(watchList[i]).c_str(),
                        (i + 1 < watchList.size()) ? "," : "");
        }
        std::printf("]\n");
        return 0;
    }
    if (watchList.empty()) {
        console::PrintLine(console::kColorYellow, "(the watch list is empty)");
        return 0;
    }
    for (size_t i = 0; i < watchList.size(); ++i) {
        console::Printf(console::kColorReset, "%s\n", watchList[i].c_str());
    }
    return 0;
}

// -----------------------------------------------------------------------------
// "awake log" (F7)
// -----------------------------------------------------------------------------

namespace {

// Tells a running daemon to re-read its settings (the log commands write
// them into awake.ini first). Silent when the daemon is not running: the
// settings then apply at the next daemon start.
void NotifyDaemonReload() {
    if (!ipc::IsDaemonRunning()) {
        return;
    }
    std::string response;
    if (ipc::SendRequest(protocol::kReqReload, response) &&
        response.compare(0, 2, "OK") == 0) {
        console::PrintLine(console::kColorReset,
                           "The running daemon has reloaded its settings.");
    }
}

// Reads a log setting with a fallback for the display command.
std::string ReadLogSetting(const char* key, const char* fallback) {
    std::string value;
    if (!config::ReadSetting("settings", key, value) || value.empty()) {
        return fallback;
    }
    return value;
}

} // namespace

int cli::CmdLog(int argc, char** argv) {
    // "awake log": show the current settings and the log directory.
    if (argc < 3) {
        console::Printf(console::kColorReset, "Logging:    %s\n",
                        ReadLogSetting("log_enabled", "true").c_str());
        console::Printf(console::kColorReset, "Level:      %s\n",
                        ReadLogSetting("log_level", "info").c_str());
        console::Printf(console::kColorReset, "Max files:  %s\n",
                        ReadLogSetting("log_max_files", "10").c_str());
        console::Printf(console::kColorReset, "Max size:   %s MB\n",
                        ReadLogSetting("log_max_size_mb", "5").c_str());
        char cwd[MAX_PATH] = { 0 };
        GetCurrentDirectoryA(MAX_PATH, cwd);
        console::Printf(console::kColorReset, "Directory:  %s\n",
                        logexport::LogDirectory().c_str());
        console::Printf(console::kColorReset, "Export to:  %s\n",
                        (std::string(cwd) + "\\awake-logs.zip").c_str());
        console::PrintLine(console::kColorReset,
                           "Use 'awake log on' to enable logging or 'awake log export' to pack the files.");
        return 0;
    }

    const std::string sub = config::ToLowerAscii(argv[2]);

    if (sub == "on" || sub == "off") {
        const std::string value = (sub == "on") ? "true" : "false";
        if (!config::WriteSetting("settings", "log_enabled", value)) {
            console::PrintLine(console::kColorRed, "Error: cannot write the log settings.");
            return 1;
        }
        NotifyDaemonReload();
        if (sub == "on") {
            console::Printf(console::kColorGreen, "Daemon logging ENABLED. Files are written to %s\n",
                            logexport::LogDirectory().c_str());
        } else {
            console::PrintLine(console::kColorGreen, "Daemon logging DISABLED.");
        }
        return 0;
    }

    if (sub == "level") {
        if (argc < 4) {
            console::PrintLine(console::kColorRed, "Error: 'log level' needs a level (info or debug).");
            return 1;
        }
        const std::string level = config::ToLowerAscii(argv[3]);
        if (level != "info" && level != "debug") {
            console::Printf(console::kColorRed, "Error: unknown log level '%s' (expected info or debug).\n", argv[3]);
            return 1;
        }
        if (!config::WriteSetting("settings", "log_level", level)) {
            console::PrintLine(console::kColorRed, "Error: cannot write the log settings.");
            return 1;
        }
        NotifyDaemonReload();
        console::Printf(console::kColorGreen, "Log level set to %s.\n", level.c_str());
        return 0;
    }

    if (sub == "max-files" || sub == "max-size") {
        if (argc < 4) {
            console::Printf(console::kColorRed, "Error: 'log %s' needs a number (1-100).\n", sub.c_str());
            return 1;
        }
        const int value = std::atoi(argv[3]);
        if (value < 1 || value > 100) {
            console::Printf(console::kColorRed, "Error: '%s' is out of range (1-100).\n", argv[3]);
            return 1;
        }
        const std::string key = (sub == "max-files") ? "log_max_files" : "log_max_size_mb";
        if (!config::WriteSetting("settings", key, std::to_string(value))) {
            console::PrintLine(console::kColorRed, "Error: cannot write the log settings.");
            return 1;
        }
        NotifyDaemonReload();
        console::Printf(console::kColorGreen, "Log %s set to %d.\n",
                        (sub == "max-files") ? "file limit" : "size limit (MB)", value);
        return 0;
    }

    if (sub == "export") {
        // Default target: awake-logs-<timestamp>.zip in the current directory.
        std::string target;
        if (argc >= 4) {
            target = argv[3];
        } else {
            // Default target: the current working directory (absolute path).
            char cwd[MAX_PATH] = { 0 };
            GetCurrentDirectoryA(MAX_PATH, cwd);
            const std::time_t now = std::time(NULL);
            std::tm local;
            char name[64] = { 0 };
            if (localtime_s(&local, &now) == 0) {
                std::snprintf(name, sizeof(name), "awake-logs-%04d%02d%02d-%02d%02d%02d.zip",
                              local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                              local.tm_hour, local.tm_min, local.tm_sec);
            } else {
                std::snprintf(name, sizeof(name), "awake-logs.zip");
            }
            target = std::string(cwd) + "\\" + name;
        }
        int fileCount = 0;
        std::string error;
        if (!logexport::ZipLogDirectory(target, fileCount, error)) {
            console::Printf(console::kColorRed, "Error: %s.\n", error.c_str());
            return 1;
        }
        console::Printf(console::kColorGreen, "Exported %d log file(s) to %s\n",
                        fileCount, target.c_str());
        return 0;
    }

    cli::PrintCommandSuggestions("log " + sub, "log");
    return 1;
}
