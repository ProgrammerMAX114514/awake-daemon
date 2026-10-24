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
// logging.h - daemon file logging (F7).
//
// Settings live in awake.ini ([settings] section) and are (re-)read by
// ReloadFromConfig(), which the daemon calls at startup and after every
// RELOAD command:
//
//   log_enabled    = true | false      (default false)
//   log_level      = info | debug      (default info)
//   log_max_files  = 1..100            (rolling limit, default 10)
//   log_max_size_mb = 1..100           (per-file size limit, default 5)
//
// Files are written to "%LOCALAPPDATA%\awake\logs\daemon" as
// daemon-<timestamp>.log; when a file grows beyond the size limit a new one
// is started and the oldest files beyond log_max_files are deleted.
// Every function is thread-safe: the server, power and monitor threads all
// write through the same mutex.
// =============================================================================

#ifndef AWAKE_DAEMON_LOGGING_H
#define AWAKE_DAEMON_LOGGING_H

#include <string>

namespace daemonlog {

// (Re-)reads the log settings from the configuration file. Creates the log
// directory and the first log file when logging is enabled.
void ReloadFromConfig();

// The directory that holds the daemon-<timestamp>.log files (for the
// client's export command). Always returns a path, even when logging is
// currently disabled.
std::string LogDirectory();

// True when logging is currently enabled (after the latest settings).
bool Enabled();

// Appends one line ("<timestamp> [level] message") when logging is enabled
// and the level passes the configured threshold. "info" always passes when
// logging is on; "debug" lines are dropped unless the level is "debug".
// The file is opened and closed per call, so exports always see a complete
// file. Rolls the file over when it outgrows the size limit.
void Write(const char* level, const std::string& message);

// Level shorthands.
void Info(const std::string& message);
void Debug(const std::string& message);

} // namespace daemonlog

#endif // AWAKE_DAEMON_LOGGING_H
