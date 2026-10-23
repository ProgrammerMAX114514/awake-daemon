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
// test_timeparse.cpp - unit tests for the "for <duration>" / "until HH:MM"
// argument parsing that drives the timed keep-awake (F1).
// =============================================================================

#include "catch.hpp"

#include "timeparse.h"

#include <string>

TEST_CASE("ParseDuration understands single units", "[timeparse]") {
    unsigned int seconds = 0;
    REQUIRE(timeparse::ParseDuration("90s", seconds));
    REQUIRE(seconds == 90u);

    REQUIRE(timeparse::ParseDuration("30m", seconds));
    REQUIRE(seconds == 30u * 60u);

    REQUIRE(timeparse::ParseDuration("2h", seconds));
    REQUIRE(seconds == 2u * 60u * 60u);

    REQUIRE(timeparse::ParseDuration("1d", seconds));
    REQUIRE(seconds == 24u * 60u * 60u);
}

TEST_CASE("ParseDuration accepts compound periods", "[timeparse]") {
    unsigned int seconds = 0;
    REQUIRE(timeparse::ParseDuration("1h30m", seconds));
    REQUIRE(seconds == 5400u);

    REQUIRE(timeparse::ParseDuration("1d2h3m4s", seconds));
    REQUIRE(seconds == 24u * 3600u + 2u * 3600u + 3u * 60u + 4u);
}

TEST_CASE("ParseDuration is case insensitive", "[timeparse]") {
    unsigned int seconds = 0;
    REQUIRE(timeparse::ParseDuration("1H30M", seconds));
    REQUIRE(seconds == 5400u);
}

TEST_CASE("ParseDuration rejects bare numbers and empty input", "[timeparse]") {
    unsigned int seconds = 12345;
    REQUIRE_FALSE(timeparse::ParseDuration("30", seconds));
    REQUIRE_FALSE(timeparse::ParseDuration("", seconds));
    REQUIRE(seconds == 0u); // a failed parse leaves the output cleared
}

TEST_CASE("ParseDuration rejects zero and unknown units", "[timeparse]") {
    unsigned int seconds = 0;
    REQUIRE_FALSE(timeparse::ParseDuration("0m", seconds));
    REQUIRE_FALSE(timeparse::ParseDuration("30x", seconds));
    REQUIRE_FALSE(timeparse::ParseDuration("m30", seconds));
    REQUIRE_FALSE(timeparse::ParseDuration("30m45", seconds));
    REQUIRE_FALSE(timeparse::ParseDuration("-30m", seconds));
}

TEST_CASE("ParseDuration enforces the upper limit", "[timeparse]") {
    unsigned int seconds = 0;
    REQUIRE(timeparse::ParseDuration("365d", seconds));
    REQUIRE(seconds == timeparse::kMaxDurationSeconds);
    REQUIRE_FALSE(timeparse::ParseDuration("366d", seconds));
    REQUIRE_FALSE(timeparse::ParseDuration("999999999999999999999h", seconds));
}

TEST_CASE("FormatDuration prints the compact form", "[timeparse]") {
    REQUIRE(timeparse::FormatDuration(0u) == "0s");
    REQUIRE(timeparse::FormatDuration(45u) == "45s");
    REQUIRE(timeparse::FormatDuration(90u) == "1m30s");
    REQUIRE(timeparse::FormatDuration(30u * 60u) == "30m");
    REQUIRE(timeparse::FormatDuration(5400u) == "1h30m");
    REQUIRE(timeparse::FormatDuration(2u * 3600u) == "2h");
    REQUIRE(timeparse::FormatDuration(24u * 3600u + 3600u) == "1d1h");
}

TEST_CASE("ParseUntil returns the time left today", "[timeparse]") {
    // 12:00 now, target 18:00 -> six hours.
    unsigned int seconds = 0;
    bool rolled = true;
    REQUIRE(timeparse::ParseUntil("18:00", 12u * 3600u, seconds, rolled));
    REQUIRE(seconds == 6u * 3600u);
    REQUIRE_FALSE(rolled);
}

TEST_CASE("ParseUntil rolls a passed time to tomorrow", "[timeparse]") {
    // 19:00 now, target 18:00 -> 23 hours, and the caller must warn.
    unsigned int seconds = 0;
    bool rolled = false;
    REQUIRE(timeparse::ParseUntil("18:00", 19u * 3600u, seconds, rolled));
    REQUIRE(seconds == 23u * 3600u);
    REQUIRE(rolled);

    // Exactly the current minute counts as "already gone" as well.
    REQUIRE(timeparse::ParseUntil("19:00", 19u * 3600u, seconds, rolled));
    REQUIRE(seconds == 24u * 3600u);
    REQUIRE(rolled);
}

TEST_CASE("ParseUntil accepts a single digit hour", "[timeparse]") {
    unsigned int seconds = 0;
    bool rolled = false;
    REQUIRE(timeparse::ParseUntil("9:30", 9u * 3600u, seconds, rolled));
    REQUIRE(seconds == 30u * 60u);
    REQUIRE_FALSE(rolled);
}

TEST_CASE("ParseUntil rejects malformed times", "[timeparse]") {
    unsigned int seconds = 0;
    bool rolled = false;
    REQUIRE_FALSE(timeparse::ParseUntil("", 0u, seconds, rolled));
    REQUIRE_FALSE(timeparse::ParseUntil("1800", 0u, seconds, rolled));
    REQUIRE_FALSE(timeparse::ParseUntil("24:00", 0u, seconds, rolled));
    REQUIRE_FALSE(timeparse::ParseUntil("18:60", 0u, seconds, rolled));
    REQUIRE_FALSE(timeparse::ParseUntil("aa:bb", 0u, seconds, rolled));
    REQUIRE_FALSE(timeparse::ParseUntil("18:5", 0u, seconds, rolled));
}
