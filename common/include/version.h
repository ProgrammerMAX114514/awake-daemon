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
// version.h - the single source of the awake version number.
//
// Both the client (awake.exe) and the daemon (awake.daemon.exe) include
// this header, so the version is defined in exactly one place. Bump the
// numbers here when releasing a new version; everywhere else picks the
// change up automatically.
// =============================================================================

#ifndef AWAKE_VERSION_H
#define AWAKE_VERSION_H

namespace version {

// Semantic version components.
const int kMajor = 0;
const int kMinor = 2;
const int kPatch = 0;

// Pre-release suffix ("" for stable releases).
const char* const kSuffix = "-beta";

// Full version string, assembled from the components above, e.g. "0.2.0-beta".
const char* const kVersion = "0.2.0-beta";

} // namespace version

#endif // AWAKE_VERSION_H
