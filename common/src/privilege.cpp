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
// privilege.cpp - implementation of the elevation check.
// =============================================================================

#include "privilege.h"

#include <windows.h>

namespace privilege {

bool IsProcessElevated() {
    // TokenElevation is available since Windows Vista and does not require
    // administrator rights to query for the own process.
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }
    TOKEN_ELEVATION elevation = { 0 };
    DWORD returned = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation,
                                        &elevation, sizeof(elevation), &returned);
    CloseHandle(token);
    if (!ok) {
        return false;
    }
    return elevation.TokenIsElevated != 0;
}

} // namespace privilege
