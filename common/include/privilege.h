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
// privilege.h - process elevation (administrator) detection.
//
// A daemon started from an elevated console cannot be controlled from a
// non-elevated one: taskkill is denied and the named pipe client fails
// with access denied, which looks like "the process is running but does
// not respond". Both executables therefore warn once when they run
// elevated, and the status view reports the daemon's elevation.
// =============================================================================

#ifndef AWAKE_PRIVILEGE_H
#define AWAKE_PRIVILEGE_H

namespace privilege {

// Returns true when the current process runs with an elevated token (i.e.
// it was started with "Run as administrator" and UAC consent was given).
// Returns false when the token cannot be queried, so a failure never
// produces a false warning.
bool IsProcessElevated();

} // namespace privilege

#endif // AWAKE_PRIVILEGE_H
