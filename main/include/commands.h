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

// Prints the "command -> accepted spellings" table. One command per line,
// aligned in two columns; shared by the help text and "awake aliases".
void PrintAliasTable();

// "awake aliases" (also: "awake a", "awake alias"): prints every command
// together with its aliases. Documentation only, no side effects.
int CmdAliases();

// Prints the "unknown command" error together with the closest commands and
// their descriptions; nothing is executed. input is the full command text
// without the leading "awake". scope is NULL at the top level, or the parent
// command ("daemon"/"screen") when a subcommand was misspelled or missing.
void PrintCommandSuggestions(const std::string& input, const char* scope = NULL);

// First-run license gate. When the configuration file does not record an
// accepted license, the full Apache License 2.0 text is printed, the user
// is asked for confirmation and the answer is stored in awake.ini.
// Returns false when the user declined or when standard input is not an
// interactive console (the caller then aborts with a non-zero exit code).
bool EnsureLicenseAccepted();

// "awake 0" / "awake 1": disable/enable keep-awake via the daemon.
// seconds > 0 enables it for a limited period (F1): the daemon then releases
// the state by itself. Returns the process exit code.
int CmdSetKeepAwake(bool enable, unsigned int seconds = 0);

// "awake screen on" / "awake screen off": screen keep-awake via the daemon,
// with the same optional period.
int CmdSetScreenKeepAwake(bool enable, unsigned int seconds = 0);

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

// F3: machine-readable output mode (--json) for "list" and "status".
void SetJsonOutput(bool enabled);
bool JsonOutput();

// "awake list": the watch list, one image name per line (or a JSON array
// with --json). Plain text on purpose - meant for scripts.
int CmdList();

// "awake log": show or change the daemon log settings (no argument shows
// them; on, off, level, max-files, max-size modify them; export packs the
// log files into a .zip for an issue attachment).
int CmdLog(int argc, char** argv);

} // namespace cli

#endif // AWAKE_COMMANDS_H
