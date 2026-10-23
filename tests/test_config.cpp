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
// test_config.cpp - unit tests for the pure parts of the config module.
//
// The file-system facing functions (ReadWatchList, AddWatchEntry, ...) read
// and write "<exe dir>\awake.ini" and are covered by the smoke test; the
// helpers below are the ones the parser is built from, so they get direct
// coverage here.
// =============================================================================

#include "catch.hpp"

#include "config.h"

#include <string>

TEST_CASE("IsValidImageName accepts plain image names", "[config]") {
    REQUIRE(config::IsValidImageName("notepad.exe"));
    REQUIRE(config::IsValidImageName("game_1.exe"));
    REQUIRE(config::IsValidImageName("My App.exe")); // spaces are legal in file names
    REQUIRE(config::IsValidImageName("a"));          // the validator does not require a suffix
}

TEST_CASE("IsValidImageName rejects empty names", "[config]") {
    REQUIRE_FALSE(config::IsValidImageName(""));
}

TEST_CASE("IsValidImageName rejects path separators", "[config]") {
    REQUIRE_FALSE(config::IsValidImageName("sub\\app.exe"));
    REQUIRE_FALSE(config::IsValidImageName("sub/app.exe"));
    REQUIRE_FALSE(config::IsValidImageName("C:app.exe"));
}

TEST_CASE("IsValidImageName rejects wildcards and shell metacharacters", "[config]") {
    REQUIRE_FALSE(config::IsValidImageName("*.exe"));
    REQUIRE_FALSE(config::IsValidImageName("app?.exe"));
    REQUIRE_FALSE(config::IsValidImageName("\"app.exe\""));
    REQUIRE_FALSE(config::IsValidImageName("app.exe|more"));
    REQUIRE_FALSE(config::IsValidImageName("app<1>.exe"));
}

TEST_CASE("IsValidImageName rejects control and non-ASCII characters", "[config]") {
    REQUIRE_FALSE(config::IsValidImageName(std::string("app\t.exe")));
    REQUIRE_FALSE(config::IsValidImageName(std::string("app\n.exe")));
    // UTF-8 encoded non-ASCII bytes are outside the accepted range as well.
    REQUIRE_FALSE(config::IsValidImageName(std::string("a\xE4\xB8\xAD.exe")));
}

TEST_CASE("IsValidImageName enforces the length limit", "[config]") {
    const std::string atLimit(260, 'a');
    const std::string overLimit(261, 'a');
    REQUIRE(config::IsValidImageName(atLimit));
    REQUIRE_FALSE(config::IsValidImageName(overLimit));
}

TEST_CASE("ToLowerAscii lowercases letters only", "[config]") {
    REQUIRE(config::ToLowerAscii("NotePad.EXE") == "notepad.exe");
    REQUIRE(config::ToLowerAscii("a-b_1.EXE") == "a-b_1.exe");
    REQUIRE(config::ToLowerAscii("") == "");
    // Bytes outside A-Z pass through untouched (the function is ASCII only).
    REQUIRE(config::ToLowerAscii("\xC4") == "\xC4");
}

TEST_CASE("TrimText removes surrounding blanks", "[config]") {
    REQUIRE(config::TrimText("  notepad.exe  ") == "notepad.exe");
    REQUIRE(config::TrimText("\t\r\nname\n") == "name");
    REQUIRE(config::TrimText("") == "");
    REQUIRE(config::TrimText("   \t ") == "");
    // Inner blanks are part of the value and must survive.
    REQUIRE(config::TrimText(" a b ") == "a b");
}
