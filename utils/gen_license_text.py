"""Generates common/src/license.cpp from LICENSE (build-time helper, not shipped)."""

import pathlib

root = pathlib.Path(__file__).resolve().parents[1]
text = (root / "LICENSE").read_text(encoding="utf-8")
text.encode("ascii")  # fail loudly if the license text is not pure ASCII
text = text.replace("\r\n", "\n").rstrip("\n")

header = '''// Copyright 2026 ProgrammerMAX114514
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
// license.cpp - the Apache License 2.0 text, embedded in the binary.
//
// The client shows this text when the user has not accepted the license yet
// (see config::IsLicenseAccepted). Embedding it keeps the agreement flow
// working even when the LICENSE file next to the executable is missing.
//
// GENERATED from the LICENSE file in the repository root - do not edit by
// hand. Regenerate with utils/gen_license_text.py after changing LICENSE.
// =============================================================================

#include "license.h"

namespace license {

const char* const kName = "Apache License 2.0";
const char* const kUrl = "https://www.apache.org/licenses/LICENSE-2.0";

const char* const kText = R"AWAKE_LICENSE('''

footer = ''')AWAKE_LICENSE";

} // namespace license
'''

(root / "common" / "src" / "license.cpp").write_text(header + text + footer, encoding="ascii", newline="\n")
print("wrote common/src/license.cpp:", len(text), "bytes of license text")
