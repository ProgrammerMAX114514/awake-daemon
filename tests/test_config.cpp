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

// -----------------------------------------------------------------------------
// File-level tests (R7b). These exercise the path-injectable overloads in a
// temporary directory, so they never touch the real awake.ini.
// -----------------------------------------------------------------------------

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

// A throwaway directory per test case, removed when the test ends.
struct TempDir {
    std::string path;

    TempDir() {
        char base[MAX_PATH] = { 0 };
        GetTempPathA(MAX_PATH, base);
        std::ostringstream name;
        name << "awake_unit_tests_" << GetCurrentProcessId() << "_"
             << GetTickCount64();
        path = std::string(base) + name.str();
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::string File(const char* name) const { return path + "\\" + name; }
};

void WriteTextFile(const std::string& path, const std::string& content) {
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    file << content;
}

std::string ReadTextFile(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace

TEST_CASE("EnsureConfigExists creates the template exactly once",
          "[config][file]") {
    TempDir dir;
    const std::string ini = dir.File("awake.ini");

    REQUIRE(config::EnsureConfigExists(ini));
    REQUIRE(config::ReadWatchList(ini).empty()); // fresh template, no entries

    // The template carries its explanatory comment header.
    const std::string content = ReadTextFile(ini);
    REQUIRE(content.find("# awake configuration file") != std::string::npos);

    // A second call must not clobber the file (entries survive).
    WriteTextFile(ini, std::string("# keep me\n[watch]\nfoo.exe\n"));
    REQUIRE(config::EnsureConfigExists(ini));
    REQUIRE(ReadTextFile(ini).find("# keep me") != std::string::npos);
}

TEST_CASE("ReadWatchList is section aware", "[config][file]") {
    TempDir dir;
    const std::string ini = dir.File("awake.ini");
    WriteTextFile(ini,
                  "# a comment\n"
                  "\n"
                  "[watch]\n"
                  "notepad.exe\n"
                  "  spaced.exe  \n"      // entries are trimmed
                  "\n"
                  "[license]\n"
                  "license_accepted = true\n" // settings of other sections
                  "[other]\n"
                  "evil.exe\n"            // entries of other sections
                  "[watch]\n"
                  "returned.exe\n");      // a second [watch] section still counts

    const std::vector<std::string> entries = config::ReadWatchList(ini);
    REQUIRE(entries.size() == 3);
    REQUIRE(entries[0] == "notepad.exe");
    REQUIRE(entries[1] == "spaced.exe");
    REQUIRE(entries[2] == "returned.exe");
}

TEST_CASE("ReadWatchList reads old files without any section header",
          "[config][file]") {
    TempDir dir;
    const std::string ini = dir.File("awake.ini");
    WriteTextFile(ini, "# comment\nold.exe\nanother.exe\n");

    const std::vector<std::string> entries = config::ReadWatchList(ini);
    REQUIRE(entries.size() == 2);
    REQUIRE(entries[0] == "old.exe");
    REQUIRE(entries[1] == "another.exe");
}

TEST_CASE("AddWatchEntry dedupes case-insensitively and appends to the end",
          "[config][file]") {
    TempDir dir;
    const std::string ini = dir.File("awake.ini");
    REQUIRE(config::EnsureConfigExists(ini));

    REQUIRE(config::AddWatchEntry(ini, "notepad.exe"));
    REQUIRE_FALSE(config::AddWatchEntry(ini, "notepad.exe"));
    REQUIRE_FALSE(config::AddWatchEntry(ini, "NOTEPAD.EXE")); // case-insensitive

    const std::vector<std::string> entries = config::ReadWatchList(ini);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0] == "notepad.exe");

    // An invalid image name is rejected without touching the file.
    REQUIRE_FALSE(config::AddWatchEntry(ini, "bad/name.exe"));
    REQUIRE(config::ReadWatchList(ini).size() == 1);
}

TEST_CASE("AddWatchEntry preserves comments when rewriting", "[config][file]") {
    TempDir dir;
    const std::string ini = dir.File("awake.ini");
    WriteTextFile(ini,
                  "# my hand written comment\n"
                  "[watch]\n"
                  "foo.exe\n");

    REQUIRE(config::AddWatchEntry(ini, "bar.exe"));
    const std::string content = ReadTextFile(ini);
    REQUIRE(content.find("# my hand written comment") != std::string::npos);
    REQUIRE(content.find("foo.exe") != std::string::npos);
    REQUIRE(content.find("bar.exe") != std::string::npos);

    const std::vector<std::string> entries = config::ReadWatchList(ini);
    REQUIRE(entries.size() == 2);
}

TEST_CASE("AddWatchEntry reopens a watch section after a settings section",
          "[config][file]") {
    TempDir dir;
    const std::string ini = dir.File("awake.ini");
    WriteTextFile(ini,
                  "[watch]\n"
                  "foo.exe\n"
                  "\n"
                  "[license]\n"
                  "license_accepted = true\n");

    // The entry must not land inside [license] where it would be read as a
    // setting; a new [watch] header is expected.
    REQUIRE(config::AddWatchEntry(ini, "bar.exe"));
    const std::string content = ReadTextFile(ini);
    REQUIRE(content.find("[watch]\nbar.exe") != std::string::npos);
    REQUIRE(config::ReadWatchList(ini).size() == 2);
}

TEST_CASE("RemoveWatchEntry is case-insensitive and keeps everything else",
          "[config][file]") {
    TempDir dir;
    const std::string ini = dir.File("awake.ini");
    WriteTextFile(ini,
                  "# comment\n"
                  "[watch]\n"
                  "foo.exe\n"
                  "bar.exe\n"
                  "; another comment\n");

    REQUIRE(config::RemoveWatchEntry(ini, "BAR.EXE"));
    const std::string content = ReadTextFile(ini);
    REQUIRE(content.find("bar.exe") == std::string::npos);
    REQUIRE(content.find("foo.exe") != std::string::npos);
    REQUIRE(content.find("# comment") != std::string::npos);
    REQUIRE(content.find("; another comment") != std::string::npos);

    // Removing a name that is not in the list fails without changes.
    REQUIRE_FALSE(config::RemoveWatchEntry(ini, "missing.exe"));
}

TEST_CASE("ResetConfig wipes the entries and restores the template",
          "[config][file]") {
    TempDir dir;
    const std::string ini = dir.File("awake.ini");
    REQUIRE(config::AddWatchEntry(ini, "foo.exe"));
    REQUIRE(config::ReadWatchList(ini).size() == 1);

    REQUIRE(config::ResetConfig(ini));
    REQUIRE(config::ReadWatchList(ini).empty());
    // The template header is back.
    REQUIRE(ReadTextFile(ini).find("# awake configuration file") !=
            std::string::npos);
}

TEST_CASE("AddWatchEntry creates the file when it does not exist",
          "[config][file]") {
    TempDir dir;
    const std::string ini = dir.File("awake.ini");

    REQUIRE(config::AddWatchEntry(ini, "solo.exe"));
    const std::vector<std::string> entries = config::ReadWatchList(ini);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0] == "solo.exe");
}
