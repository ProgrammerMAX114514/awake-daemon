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
// logexport.h - pack the daemon log files into a .zip (F7).
//
// The writer produces a minimal STORED (uncompressed) zip archive with no
// external dependencies - small enough for an issue attachment and readable
// by every unzip tool. Log lines are plain ASCII, so compression would not
// buy much anyway.
// =============================================================================

#ifndef AWAKE_LOGEXPORT_H
#define AWAKE_LOGEXPORT_H

#include <string>

namespace logexport {

// Zips every daemon-*.log file of the daemon log directory into zipPath
// (created/overwritten). Returns true on success; on failure error describes
// what went wrong and fileCount is 0. On success fileCount holds the number
// of archived log files.
bool ZipLogDirectory(const std::string& zipPath, int& fileCount,
                     std::string& error);

// The directory the daemon writes its logs to
// (%LOCALAPPDATA%\awake\logs\daemon). Always returns a path.
std::string LogDirectory();

} // namespace logexport

#endif // AWAKE_LOGEXPORT_H
