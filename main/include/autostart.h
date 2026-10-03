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
// autostart.h - daemon autostart at logon via the registry (awake.exe).
//
// The autostart is stored as a REG_SZ value named "awake" under the
// per-user registry key
//   HKCU\Software\Microsoft\Windows\CurrentVersion\Run
// whose data launches awake.daemon.exe with the hidden internal token.
// The daemon then runs in the same state as after "awake daemon on"
// (keep-awake inactive until requested).
// =============================================================================

#ifndef AWAKE_AUTOSTART_H
#define AWAKE_AUTOSTART_H

#include <string>

namespace autostart {

// Returns true when the registry Run value currently exists.
bool IsEnabled();

// Creates/overwrites the registry Run value so that the daemon starts at
// logon. The daemon executable is expected next to awake.exe; when it is
// missing, the function fails and fills error with the reason.
bool Enable(std::string& error);

// Deletes the registry Run value. Returns true also when the value did
// not exist (idempotent). Fills error on registry failures.
bool Disable(std::string& error);

} // namespace autostart

#endif // AWAKE_AUTOSTART_H
