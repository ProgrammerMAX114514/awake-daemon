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
// main.cpp - awake.exe, the client command line tool.
//
// This file only contains the entry point and the argv dispatch; the
// command implementations live in commands.cpp (and status.cpp for the
// status view). Commands:
//
//   awake version           print the version
//   awake help              print help
//   awake aliases           print every command alias (also: awake a)
//   awake 0                 disable keep-awake
//   awake 1                 enable keep-awake
//   awake status            show daemon/keep-awake/screen/watch list status
//   awake screen on         keep the screen on (block screen off/screensaver/idle lock)
//   awake screen off        stop keeping the screen on
//   awake daemon on         start the daemon
//   awake daemon off        stop the daemon
//   awake daemon enable     enable daemon autostart at logon (registry)
//   awake daemon disable    disable daemon autostart at logon
//   awake reload            re-read the configuration file immediately
//   awake reset             reset the configuration file (asks confirmation)
//   awake add <imagename>   add an image name to the watch list
//   awake del <imagename>   remove an image name from the watch list
//
// "awake 0" and "awake 1" start the daemon automatically when it is not
// running. "awake status" deliberately never does, because its purpose is
// to report the current state.
// =============================================================================

#include <windows.h>

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "commands.h"
#include "config.h"
#include "console.h"
#include "privilege.h"
#include "timeparse.h"
#include "version.h"

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

// Parses the optional "for <duration>" / "until <HH:MM>" tail of a command,
// starting at argv[index]. On success seconds holds the period (0 = no
// expiry) and the function returns true. A malformed tail is reported and
// turns into a non-zero exit code, so nothing is sent to the daemon.
static bool ParseExpiryArguments(int argc, char** argv, int index, unsigned int& seconds) {
    seconds = 0;
    if (index >= argc) {
        return true; // no tail at all: keep-awake without an expiry
    }

    const std::string keyword = config::ToLowerAscii(argv[index]);
    const int argumentsLeft = argc - index;

    if (keyword == "for") {
        if (argumentsLeft != 2) {
            console::PrintLine(console::kColorRed,
                               "Error: 'for' needs a period, e.g. 'for 30m', 'for 1h30m'.");
            return false;
        }
        if (!timeparse::ParseDuration(argv[index + 1], seconds)) {
            console::Printf(console::kColorRed,
                            "Error: cannot read '%s' as a period. Use a number plus s/m/h/d, e.g. 30m, 2h, 1h30m.\n",
                            argv[index + 1]);
            return false;
        }
        return true;
    }

    if (keyword == "until") {
        if (argumentsLeft != 2) {
            console::PrintLine(console::kColorRed,
                               "Error: 'until' needs a time, e.g. 'until 18:00'.");
            return false;
        }
        // Convert the wall clock time to "seconds from now" here: the daemon
        // only knows monotonic time, which keeps the expiry stable even if
        // the clock is changed later.
        const std::time_t now = std::time(NULL);
        std::tm local;
        if (localtime_s(&local, &now) != 0) {
            console::PrintLine(console::kColorRed, "Error: cannot read the local time.");
            return false;
        }
        const unsigned int nowToday = static_cast<unsigned int>(local.tm_hour) * 3600u +
                                      static_cast<unsigned int>(local.tm_min) * 60u +
                                      static_cast<unsigned int>(local.tm_sec);
        bool rolledOver = false;
        if (!timeparse::ParseUntil(argv[index + 1], nowToday, seconds, rolledOver)) {
            console::Printf(console::kColorRed,
                            "Error: cannot read '%s' as a time. Use the 24 hour form HH:MM, e.g. 18:00.\n",
                            argv[index + 1]);
            return false;
        }
        if (rolledOver) {
            // The user almost certainly wants a keep-awake that ends today, so
            // rolling into tomorrow is worth a warning rather than silence.
            console::Printf(console::kColorYellow,
                            "Warning: %s has already passed today; using tomorrow instead (%s from now).\n",
                            argv[index + 1], timeparse::FormatDuration(seconds).c_str());
        }
        return true;
    }

    console::Printf(console::kColorRed,
                    "Error: unexpected argument '%s' (expected 'for <period>' or 'until <HH:MM>').\n",
                    argv[index]);
    return false;
}

// Runs one command and returns its exit code. Kept separate from main() so
// that the AI notice can be printed exactly once, after any command output.
static int RunCommand(int originalArgc, char** originalArgv) {
    console::EnableColors();

    // F3: the global output options may appear anywhere on the command
    // line; strip them from the argument list before dispatching.
    std::vector<char*> filteredArgv;
    filteredArgv.push_back(originalArgv[0]);
    for (int i = 1; i < originalArgc; ++i) {
        const std::string arg = originalArgv[i];
        if (arg == "--no-color") {
            console::SetColorsEnabled(false);
            continue;
        }
        if (arg == "--json") {
            cli::SetJsonOutput(true);
            continue;
        }
        filteredArgv.push_back(originalArgv[i]);
    }
    char** argv = filteredArgv.data();
    const int argc = static_cast<int>(filteredArgv.size());

    // Warn when the client itself runs with administrator rights. A daemon
    // started from such a console cannot be stopped or contacted from a
    // non-elevated shell, which looks like "the process runs but does not
    // respond". The warning is informational: it never blocks the command.
    if (privilege::IsProcessElevated()) {
        // stderr: warnings must not pollute machine-readable stdout (--json).
        console::PrintErrLine(console::kColorYellow,
                              "Warning: awake is running elevated; a daemon started from here cannot be controlled by a non-elevated shell.");
    }

    // No arguments behaves like "help".
    if (argc < 2) {
        cli::PrintHelp();
        return 0;
    }

    const std::string command = argv[1];

    // First-run license gate. "help", "version" and "aliases" stay available
    // without accepting the license: they only print documentation and do not
    // touch the system, so a new user can read them before deciding. Every
    // other command requires the acceptance.
    const bool documentation = (command == "version" || command == "help" ||
                                command == "?" || command == "h" ||
                                command == "/?" || command == "-h" || command == "--help" ||
                                command == "aliases" || command == "a" || command == "alias");
    if (!documentation && !cli::EnsureLicenseAccepted()) {
        return 1;
    }

    if (command == "version") {
        console::Printf(console::kColorCyan, "awake %s\n", version::kVersionDisplay);
        return 0;
    }
    if (command == "help" || command == "?" || command == "h" ||
        command == "/?" || command == "-h" || command == "--help") {
        cli::PrintHelp();
        return 0;
    }
    if (command == "aliases" || command == "a" || command == "alias") {
        return cli::CmdAliases();
    }
    if (command == "0" || command == "off" || command == "disable" || command == "stop") {
        return cli::CmdSetKeepAwake(false);
    }
    if (command == "1" || command == "on" || command == "enable" || command == "start") {
        unsigned int seconds = 0;
        if (!ParseExpiryArguments(argc, argv, 2, seconds)) {
            return 1;
        }
        return cli::CmdSetKeepAwake(true, seconds);
    }
    if (command == "status" || command == "st") {
        return cli::CmdStatus();
    }
    if (command == "daemon" || command == "d") {
        if (argc < 3) {
            cli::PrintCommandSuggestions("daemon", "daemon");
            return 1;
        }
        const std::string sub = argv[2];
        if (sub == "on" || sub == "1" || sub == "start") {
            char allowElevated[8] = { 0 };
            const bool overrideActive =
                GetEnvironmentVariableA("AWAKE_ALLOW_ELEVATED", allowElevated,
                                         sizeof(allowElevated)) > 0 &&
                allowElevated[0] != '\0';
            if (privilege::IsProcessElevated() && !overrideActive) {
                console::PrintErrLine(console::kColorRed,
                                      "Error: the daemon cannot be started from an elevated shell (its pipe would be uncontrollable from a normal one). Start it from a non-elevated shell.");
                return 1;
            }
            return cli::CmdDaemonOn();
        }
        if (sub == "off" || sub == "0" || sub == "stop")  return cli::CmdDaemonOff();
        if (sub == "enable")  return cli::CmdDaemonEnable();
        if (sub == "disable") return cli::CmdDaemonDisable();
        cli::PrintCommandSuggestions("daemon " + sub, "daemon");
        return 1;
    }
    if (command == "screen" || command == "scr") {
        if (argc < 3) {
            cli::PrintCommandSuggestions("screen", "screen");
            return 1;
        }
        const std::string sub = argv[2];
        if (sub == "on" || sub == "1") {
            unsigned int seconds = 0;
            if (!ParseExpiryArguments(argc, argv, 3, seconds)) {
                return 1;
            }
            return cli::CmdSetScreenKeepAwake(true, seconds);
        }
        if (sub == "off" || sub == "0") return cli::CmdSetScreenKeepAwake(false);
        cli::PrintCommandSuggestions("screen " + sub, "screen");
        return 1;
    }
    if (command == "reload") {
        return cli::CmdReload();
    }
    if (command == "reset") {
        return cli::CmdReset();
    }
    if (command == "list" || command == "l") {
        return cli::CmdList();
    }
    if (command == "log" || command == "logs") {
        return cli::CmdLog(argc, argv);
    }
    if (command == "add" || command == "del") {
        if (argc < 3) {
            console::Printf(console::kColorRed, "Error: missing image name. Usage: awake %s <imagename>\n", argv[1]);
            return 1;
        }
        return cli::CmdEditWatchList(argv[1], argv[2]);
    }

    // Unknown command: guess what the user meant instead of only rejecting it.
    // All arguments are joined so that "awake scren of" can be matched against
    // "screen off" as a whole.
    std::string typedCommand;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) {
            typedCommand += ' ';
        }
        typedCommand += argv[i];
    }
    cli::PrintCommandSuggestions(typedCommand);
    return 1;
}

// Every command ends with the same one-line reminder. It goes to stderr and
// only shows on an interactive terminal, so redirecting or piping the output
// (and any future machine-readable mode) stays clean.
int main(int argc, char** argv) {
    const int exitCode = RunCommand(argc, argv);
    console::PrintAiNotice();
    return exitCode;
}
