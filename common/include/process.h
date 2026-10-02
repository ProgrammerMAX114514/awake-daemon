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
// process.h - helpers for enumerating running processes.
//
// Used by two callers:
//   - the daemon, to decide whether a watched application currently runs,
//   - the client, to color the entries of "awake daemon status".
// =============================================================================

#ifndef AWAKE_PROCESS_H
#define AWAKE_PROCESS_H

#include <set>
#include <string>
#include <vector>

namespace process {

// Takes one snapshot of the running processes and returns the subset of the
// given image names that are currently running. Both the input names and the
// returned names are compared/matched case-insensitively; the returned set
// contains the lower case forms of the matched names.
std::set<std::string> FindRunningImages(const std::vector<std::string>& imageNames);

// Convenience wrapper around FindRunningImages() for a single name.
bool IsImageRunning(const std::string& imageName);

} // namespace process

#endif // AWAKE_PROCESS_H
