// =============================================================================
// main.cpp - awake.exe, the client command line tool.
//
// Parses the command line, talks to the daemon over the named pipe and
// prints the results with ANSI colors. Commands:
//
//   awake help              print help
//   awake 0                 disable keep-awake
//   awake 1                 enable keep-awake
//   awake status            show keep-awake status (never starts the daemon)
//   awake screen on         keep the screen on (block screen off/screensaver/idle lock)
//   awake screen off        stop keeping the screen on
//   awake screen status     show screen keep-awake status
//   awake daemon on         start the daemon
//   awake daemon status     show daemon status + watched applications
//   awake daemon off        stop the daemon
//   awake reload            re-read the configuration file immediately
//   awake reset             reset the configuration file (asks confirmation)
//   awake add <imagename>   add an image name to the watch list
//   awake del <imagename>   remove an image name from the watch list
//
// "awake 0" and "awake 1" start the daemon automatically when it is not
// running. "awake status" and "awake daemon status" deliberately never do,
// because their purpose is to report the current state.
// =============================================================================

#include <windows.h>

#include <cstdio>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "config.h"
#include "console.h"
#include "ipc.h"
#include "process.h"

// -----------------------------------------------------------------------------
// Help text
// -----------------------------------------------------------------------------

// Prints the colored usage help. This is shown for "awake help", for
// "awake" without arguments and as a hint after unknown commands.
static void PrintHelp() {
    console::Printf(console::kColorCyan, "awake - keep the system awake\n");
    console::PrintLine(console::kColorReset, "");
    console::PrintLine(console::kColorReset, "Usage:");
    console::Printf(console::kColorReset,   "  awake help              %sShow this help message.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake 0                 %sDisable keep-awake (restore default power behavior).\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake 1                 %sEnable keep-awake (block idle sleep and hibernation).\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake status            %sShow the current keep-awake status.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake screen on         %sKeep the screen on (blocks screen off, screensaver and idle lock).\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake screen off        %sStop keeping the screen on.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake screen status     %sShow the current screen keep-awake status.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake daemon on         %sStart the background daemon.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake daemon status     %sShow daemon status and watched applications.\n", console::kColorReset);
    console::Printf(console::kColorReset,   "  awake daemon off        %sStop the background daemon.\n", console::kColorReset);
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
}

// -----------------------------------------------------------------------------
// Daemon management helpers
// -----------------------------------------------------------------------------

// Makes sure the daemon is running, starting it if necessary. Only used by
// the commands that must talk to the daemon (0, 1).
static bool EnsureDaemonRunning() {
    if (ipc::IsDaemonRunning()) {
        return true;
    }
    console::PrintLine(console::kColorYellow, "Daemon is not running. Starting daemon...");
    if (!ipc::StartDaemon()) {
        console::Printf(console::kColorRed, "Error: failed to start '%s'.\n", ipc::kDaemonExeName);
        return false;
    }
    if (!ipc::WaitForDaemon(5000)) {
        console::PrintLine(console::kColorRed, "Error: the daemon was started but does not respond.");
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// Command implementations
// -----------------------------------------------------------------------------

// "awake 0" / "awake 1": forward the request to the daemon.
static int CmdSetKeepAwake(bool enable) {
    if (!EnsureDaemonRunning()) {
        return 1;
    }
    std::string response;
    if (!ipc::SendRequest(enable ? "KEEP_AWAKE 1" : "KEEP_AWAKE 0", response)) {
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

// Parsed daemon status (from the GET_STATUS response).
struct DaemonStatus {
    bool manual;  // keep-awake set with "awake 1"
    bool app;     // a watched application is currently running
    bool screen;  // screen keep-awake set with "awake screen on"
    bool active;  // anything currently prevents idle sleep
};

// Queries GET_STATUS from the daemon and parses the KEY=VALUE response
// lines. Returns false when the daemon is not reachable.
static bool QueryDaemonStatus(DaemonStatus& info) {
    info.manual = false;
    info.app = false;
    info.screen = false;
    info.active = false;

    std::string response;
    if (!ipc::SendRequest("GET_STATUS", response)) {
        return false;
    }
    size_t lineStart = 0;
    while (lineStart <= response.size()) {
        size_t lineEnd = response.find('\n', lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = response.size();
        }
        const std::string line = response.substr(lineStart, lineEnd - lineStart);
        if (line.compare(0, 7, "MANUAL=") == 0) {
            info.manual = (line.compare(7, 1, "1") == 0);
        } else if (line.compare(0, 4, "APP=") == 0) {
            info.app = (line.compare(4, 1, "1") == 0);
        } else if (line.compare(0, 7, "SCREEN=") == 0) {
            info.screen = (line.compare(7, 1, "1") == 0);
        } else if (line.compare(0, 7, "ACTIVE=") == 0) {
            info.active = (line.compare(7, 1, "1") == 0);
        }
        if (lineEnd == response.size()) {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return true;
}

// Prints the "screen keep-awake" status line for the current state.
static void PrintScreenStatusLine(bool screenEnabled) {
    if (screenEnabled) {
        console::Printf(console::kColorGreen, "Screen keep-awake is ENABLED");
        console::Printf(console::kColorReset,
                        " (screen off, screensaver and idle lock are blocked).\n");
    } else {
        console::Printf(console::kColorYellow, "Screen keep-awake is INACTIVE");
        console::Printf(console::kColorReset,
                        " (the screen may turn off on idle).\n");
    }
}

// "awake status": report the state without ever starting the daemon.
static int CmdStatus() {
    if (!ipc::IsDaemonRunning()) {
        console::PrintLine(console::kColorYellow, "Daemon is not running.");
        console::Printf(console::kColorReset, "Keep-awake is %sINACTIVE%s (the system uses its default power behavior).\n",
                        console::kColorYellow, console::kColorReset);
        PrintScreenStatusLine(false);
        return 0;
    }

    DaemonStatus info;
    if (!QueryDaemonStatus(info)) {
        console::PrintLine(console::kColorRed, "Error: lost contact with the daemon.");
        return 1;
    }

    if (!info.manual && !info.app) {
        // When only the screen keep-awake is on, the system idle sleep is
        // blocked as a side effect (the display must stay on), so spell
        // that out instead of claiming default power behavior.
        if (info.screen) {
            console::Printf(console::kColorReset, "Keep-awake is %sINACTIVE%s (screen keep-awake is also blocking idle sleep).\n",
                            console::kColorYellow, console::kColorReset);
        } else {
            console::Printf(console::kColorReset, "Keep-awake is %sINACTIVE%s (the system uses its default power behavior).\n",
                            console::kColorYellow, console::kColorReset);
        }
    } else {
        console::Printf(console::kColorGreen, "Keep-awake is ENABLED");
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
    PrintScreenStatusLine(info.screen);
    return 0;
}

// "awake screen on" / "awake screen off": forward the request to the daemon.
static int CmdSetScreenKeepAwake(bool enable) {
    if (!EnsureDaemonRunning()) {
        return 1;
    }
    std::string response;
    if (!ipc::SendRequest(enable ? "SCREEN 1" : "SCREEN 0", response)) {
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

// "awake screen status": report the screen keep-awake state without ever
// starting the daemon.
static int CmdScreenStatus() {
    if (!ipc::IsDaemonRunning()) {
        console::PrintLine(console::kColorYellow, "Daemon is not running.");
        PrintScreenStatusLine(false);
        return 0;
    }
    DaemonStatus info;
    if (!QueryDaemonStatus(info)) {
        console::PrintLine(console::kColorRed, "Error: lost contact with the daemon.");
        return 1;
    }
    PrintScreenStatusLine(info.screen);
    return 0;
}

// "awake daemon on": launch the daemon unless it is already running.
static int CmdDaemonOn() {
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
    console::PrintLine(console::kColorGreen, "The daemon is now running.");
    return 0;
}

// "awake daemon status": running state, keep-awake state and the watch list
// with green entries for running applications and red entries for the rest.
static int CmdDaemonStatus() {
    const bool running = ipc::IsDaemonRunning();
    console::Printf(console::kColorReset, "Daemon:     ");
    if (running) {
        console::PrintLine(console::kColorGreen, "running");
    } else {
        console::PrintLine(console::kColorRed, "not running");
    }

    // When the daemon runs, it knows the authoritative state.
    if (running) {
        DaemonStatus info;
        if (QueryDaemonStatus(info)) {
            if (info.manual || info.app) {
                console::Printf(console::kColorGreen, "Keep-awake: enabled%s\n", console::kColorReset);
            } else {
                console::Printf(console::kColorReset, "Keep-awake: disabled\n");
            }
            if (info.screen) {
                console::Printf(console::kColorGreen, "Screen:     enabled%s\n", console::kColorReset);
            } else {
                console::Printf(console::kColorReset, "Screen:     disabled\n");
            }
        } else {
            console::Printf(console::kColorRed, "Keep-awake: unknown (lost contact with the daemon)\n");
        }
    }

    // The watch list always comes from the configuration file, so it is
    // shown even when the daemon is not running. The running check is done
    // locally in that case.
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

// "awake daemon off": politely ask the daemon to shut down.
static int CmdDaemonOff() {
    if (!ipc::IsDaemonRunning()) {
        console::PrintLine(console::kColorYellow, "The daemon is not running.");
        return 0;
    }
    std::string response;
    if (!ipc::SendRequest("SHUTDOWN", response) || response.compare(0, 2, "OK") != 0) {
        console::PrintLine(console::kColorRed, "Error: the daemon did not acknowledge the shutdown request.");
        return 1;
    }
    console::PrintLine(console::kColorGreen, "The daemon has been stopped.");
    return 0;
}

// "awake reload": ask the daemon to re-read the configuration file now.
static int CmdReload() {
    if (!ipc::IsDaemonRunning()) {
        console::PrintLine(console::kColorYellow, "Daemon is not running.");
        console::PrintLine(console::kColorReset,  "There is nothing to reload; the configuration is read when the daemon starts.");
        return 0;
    }
    std::string response;
    if (!ipc::SendRequest("RELOAD", response) || response.compare(0, 2, "OK") != 0) {
        console::PrintLine(console::kColorRed, "Error: the daemon did not acknowledge the reload request.");
        return 1;
    }
    console::PrintLine(console::kColorGreen, "Configuration reloaded.");
    return 0;
}

// Removes leading and trailing blanks from a line of user input.
static std::string TrimInput(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t' ||
                           text[begin] == '\r' || text[begin] == '\n')) {
        ++begin;
    }
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' ||
                           text[end - 1] == '\r' || text[end - 1] == '\n')) {
        --end;
    }
    return text.substr(begin, end - begin);
}

// "awake reset": restore the configuration file to its initial state. The
// user must confirm the destructive operation with an explicit "y"/"yes";
// anything else (including an empty answer, EOF or Ctrl+C style aborts)
// leaves the file untouched.
static int CmdReset() {
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
    const std::string normalized = config::ToLowerAscii(TrimInput(answer));
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
        if (ipc::SendRequest("RELOAD", response) && response.compare(0, 2, "OK") == 0) {
            console::PrintLine(console::kColorReset, "The running daemon has reloaded the configuration.");
        }
    }
    return 0;
}

// "awake add <imagename>" / "awake del <imagename>": edit the watch list.
static int CmdEditWatchList(const char* command, const char* imageName) {
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
// Entry point
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {
    console::EnableColors();

    // No arguments behaves like "help".
    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    const std::string command = argv[1];

    if (command == "help" || command == "/?" || command == "-h" || command == "--help") {
        PrintHelp();
        return 0;
    }
    if (command == "0") {
        return CmdSetKeepAwake(false);
    }
    if (command == "1") {
        return CmdSetKeepAwake(true);
    }
    if (command == "status") {
        return CmdStatus();
    }
    if (command == "daemon") {
        if (argc < 3) {
            console::PrintLine(console::kColorRed, "Error: missing daemon subcommand (on, status or off).");
            return 1;
        }
        const std::string sub = argv[2];
        if (sub == "on")    return CmdDaemonOn();
        if (sub == "status") return CmdDaemonStatus();
        if (sub == "off")   return CmdDaemonOff();
        console::Printf(console::kColorRed, "Error: unknown daemon subcommand '%s' (expected on, status or off).\n", argv[2]);
        return 1;
    }
    if (command == "screen") {
        if (argc < 3) {
            console::PrintLine(console::kColorRed, "Error: missing screen subcommand (on, status or off).");
            return 1;
        }
        const std::string sub = argv[2];
        if (sub == "on")     return CmdSetScreenKeepAwake(true);
        if (sub == "off")    return CmdSetScreenKeepAwake(false);
        if (sub == "status") return CmdScreenStatus();
        console::Printf(console::kColorRed, "Error: unknown screen subcommand '%s' (expected on, status or off).\n", argv[2]);
        return 1;
    }
    if (command == "reload") {
        return CmdReload();
    }
    if (command == "reset") {
        return CmdReset();
    }
    if (command == "add" || command == "del") {
        if (argc < 3) {
            console::Printf(console::kColorRed, "Error: missing image name. Usage: awake %s <imagename>\n", argv[1]);
            return 1;
        }
        return CmdEditWatchList(argv[1], argv[2]);
    }

    console::Printf(console::kColorRed, "Error: unknown command '%s'.\n", argv[1]);
    console::PrintLine(console::kColorReset, "Run 'awake help' to see the available commands.");
    return 1;
}
