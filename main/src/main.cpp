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
#include <string>

#include "commands.h"
#include "console.h"
#include "version.h"

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {
    console::EnableColors();

    // No arguments behaves like "help".
    if (argc < 2) {
        cli::PrintHelp();
        return 0;
    }

    const std::string command = argv[1];

    if (command == "version") {
        console::Printf(console::kColorCyan, "awake %s\n", version::kVersion);
        return 0;
    }
    if (command == "help" || command == "?" || command == "h" ||
        command == "/?" || command == "-h" || command == "--help") {
        cli::PrintHelp();
        return 0;
    }
    if (command == "0" || command == "off" || command == "disable" || command == "stop") {
        return cli::CmdSetKeepAwake(false);
    }
    if (command == "1" || command == "on" || command == "enable" || command == "start") {
        return cli::CmdSetKeepAwake(true);
    }
    if (command == "status" || command == "st") {
        return cli::CmdStatus();
    }
    if (command == "daemon" || command == "d") {
        if (argc < 3) {
            console::PrintLine(console::kColorRed, "Error: missing daemon subcommand (on, off, enable or disable).");
            return 1;
        }
        const std::string sub = argv[2];
        if (sub == "on" || sub == "1" || sub == "start")  return cli::CmdDaemonOn();
        if (sub == "off" || sub == "0" || sub == "stop")  return cli::CmdDaemonOff();
        if (sub == "enable")  return cli::CmdDaemonEnable();
        if (sub == "disable") return cli::CmdDaemonDisable();
        console::Printf(console::kColorRed, "Error: unknown daemon subcommand '%s' (expected on/1/start, off/0/stop, enable or disable).\n", argv[2]);
        return 1;
    }
    if (command == "screen" || command == "scr") {
        if (argc < 3) {
            console::PrintLine(console::kColorRed, "Error: missing screen subcommand (on or off).");
            return 1;
        }
        const std::string sub = argv[2];
        if (sub == "on" || sub == "1")  return cli::CmdSetScreenKeepAwake(true);
        if (sub == "off" || sub == "0") return cli::CmdSetScreenKeepAwake(false);
        console::Printf(console::kColorRed, "Error: unknown screen subcommand '%s' (expected on/1 or off/0).\n", argv[2]);
        return 1;
    }
    if (command == "reload") {
        return cli::CmdReload();
    }
    if (command == "reset") {
        return cli::CmdReset();
    }
    if (command == "add" || command == "del") {
        if (argc < 3) {
            console::Printf(console::kColorRed, "Error: missing image name. Usage: awake %s <imagename>\n", argv[1]);
            return 1;
        }
        return cli::CmdEditWatchList(argv[1], argv[2]);
    }

    console::Printf(console::kColorRed, "Error: unknown command '%s'.\n", argv[1]);
    console::PrintLine(console::kColorReset, "Run 'awake help' to see the available commands.");
    return 1;
}
