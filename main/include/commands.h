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
// commands.h - the client command implementations (awake.exe).
//
// Each Cmd* function implements one "awake <command>" invocation; the
// dispatcher in main.cpp parses argv and calls into this namespace.
// =============================================================================

#ifndef AWAKE_COMMANDS_H
#define AWAKE_COMMANDS_H

#include <string>

namespace cli {

// Compares the daemon's version with the client version. Returns false
// when the major versions differ (incompatible); a minor mismatch only
// prints a yellow warning. Call this after establishing contact.
bool CheckDaemonVersion(const std::string& daemonVersion);

// Prints the colored usage help. Shown for "awake help", for "awake"
// without arguments and as a hint after unknown commands.
void PrintHelp();

// "awake 0" / "awake 1": disable/enable keep-awake via the daemon.
// Returns the process exit code.
int CmdSetKeepAwake(bool enable);

// "awake screen on" / "awake screen off": screen keep-awake via the daemon.
int CmdSetScreenKeepAwake(bool enable);

// "awake status": the single status view (daemon, keep-awake, screen and
// watch list). Never starts the daemon.
int CmdStatus();

// "awake daemon on" / "awake daemon off": start/stop the background daemon.
int CmdDaemonOn();
int CmdDaemonOff();

// "awake daemon enable" / "awake daemon disable": manage the daemon
// autostart at logon (per-user registry Run key).
int CmdDaemonEnable();
int CmdDaemonDisable();

// "awake reload": ask the daemon to re-read the configuration file now.
int CmdReload();

// "awake reset": restore the configuration file after a double confirmation.
int CmdReset();

// "awake add <imagename>" / "awake del <imagename>": edit the watch list.
// command is "add" or "del" (used for the output messages).
int CmdEditWatchList(const char* command, const char* imageName);

} // namespace cli

#endif // AWAKE_COMMANDS_H
