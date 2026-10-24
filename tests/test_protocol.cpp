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
// test_protocol.cpp - unit tests for the wire protocol.
//
// These cover the promise made in protocol.h: both executables share this
// code, so the serialized form on one side must always be readable by the
// parser on the other side, and unknown keys must be tolerated (R5).
// =============================================================================

#include "catch.hpp"

#include "protocol.h"
#include "version.h"

#include <string>

TEST_CASE("request builders produce the documented lines", "[protocol]") {
    REQUIRE(protocol::MakePingRequest() == std::string("PING ") + version::kVersion);
    REQUIRE(protocol::MakeKeepAwakeRequest(true) == "KEEP_AWAKE 1");
    REQUIRE(protocol::MakeKeepAwakeRequest(false) == "KEEP_AWAKE 0");
    REQUIRE(protocol::MakeScreenRequest(true) == "SCREEN 1");
    REQUIRE(protocol::MakeScreenRequest(false) == "SCREEN 0");
}

TEST_CASE("ParsePongResponse extracts the daemon version", "[protocol]") {
    std::string version;
    REQUIRE(protocol::ParsePongResponse("OK PONG 0.6.0-beta\n", version));
    REQUIRE(version == "0.6.0-beta");
}

TEST_CASE("ParsePongResponse tolerates extra separators", "[protocol]") {
    std::string version;
    REQUIRE(protocol::ParsePongResponse("OK PONG   1.2.3\n", version));
    REQUIRE(version == "1.2.3");
}

TEST_CASE("ParsePongResponse rejects other responses", "[protocol]") {
    std::string version = "unchanged";
    REQUIRE_FALSE(protocol::ParsePongResponse("ERR busy\n", version));
    REQUIRE_FALSE(protocol::ParsePongResponse("", version));
    // The version output is cleared even when the response is rejected.
    REQUIRE(version.empty());
}

TEST_CASE("status response survives a serialize/parse round trip", "[protocol]") {
    protocol::DaemonStatus sent;
    sent.manual = true;
    sent.app = false;
    sent.screen = true;
    sent.active = true;
    sent.elevated = true;
    sent.version = version::kVersion;

    const std::string body = protocol::SerializeStatus(sent);
    REQUIRE(body.compare(0, 2, "OK") == 0);

    protocol::DaemonStatus received;
    REQUIRE(protocol::ParseStatusResponse(body, received));
    REQUIRE(received.manual == sent.manual);
    REQUIRE(received.app == sent.app);
    REQUIRE(received.screen == sent.screen);
    REQUIRE(received.active == sent.active);
    REQUIRE(received.elevated == sent.elevated);
    REQUIRE(received.version == sent.version);
}

TEST_CASE("unknown status keys are ignored", "[protocol]") {
    const std::string body =
        "OK\n"
        "MANUAL=1\n"
        "FUTURE_FLAG=1\n"
        "APP=0\n";
    protocol::DaemonStatus status;
    REQUIRE(protocol::ParseStatusResponse(body, status));
    REQUIRE(status.manual);
    REQUIRE_FALSE(status.app);
    REQUIRE_FALSE(status.screen);
    REQUIRE_FALSE(status.elevated);
}

TEST_CASE("a response without OK is reported as failed", "[protocol]") {
    protocol::DaemonStatus status;
    REQUIRE_FALSE(protocol::ParseStatusResponse("BROKEN\nMANUAL=1\n", status));
}

TEST_CASE("missing keys keep their default values", "[protocol]") {
    protocol::DaemonStatus status;
    status.version = "stale";
    REQUIRE(protocol::ParseStatusResponse("OK\n", status));
    REQUIRE_FALSE(status.manual);
    REQUIRE_FALSE(status.active);
    // Parsing does not reset fields that the response does not mention.
    REQUIRE(status.version == "stale");
}

// -----------------------------------------------------------------------------
// Malformed / truncated input boundaries (R7b)
// -----------------------------------------------------------------------------

TEST_CASE("a truncated PONG without a newline still yields the version",
          "[protocol]") {
    std::string version;
    REQUIRE(protocol::ParsePongResponse("OK PONG 0.7.0-beta", version));
    REQUIRE(version == "0.7.0-beta");

    // A PONG cut off inside the version simply yields the visible part.
    REQUIRE(protocol::ParsePongResponse("OK PONG 0.7", version));
    REQUIRE(version == "0.7");

    // Nothing after the verb: the version stays empty but the response is
    // still recognized as a pong.
    REQUIRE(protocol::ParsePongResponse("OK PONG", version));
    REQUIRE(version.empty());
}

TEST_CASE("a truncated status response parses its complete lines",
          "[protocol]") {
    protocol::DaemonStatus status;
    // The final line has no trailing newline - the parser must not drop it.
    REQUIRE(protocol::ParseStatusResponse("OK\nMANUAL=1\nSCREEN=1", status));
    REQUIRE(status.manual);
    REQUIRE(status.screen);
}

TEST_CASE("duplicate status keys: the last occurrence wins", "[protocol]") {
    protocol::DaemonStatus status;
    REQUIRE(protocol::ParseStatusResponse(
        "OK\nMANUAL=1\nMANUAL=0\nVERSION=1.0.0\nVERSION=2.0.0\n", status));
    REQUIRE_FALSE(status.manual);
    REQUIRE(status.version == "2.0.0");
}

TEST_CASE("a version string keeps its inner spaces but loses the newline",
          "[protocol]") {
    protocol::DaemonStatus status;
    REQUIRE(protocol::ParseStatusResponse("OK\nVERSION=1.2.3-beta (git abc)\n",
                                          status));
    REQUIRE(status.version == "1.2.3-beta (git abc)");
    REQUIRE(status.version.find('\n') == std::string::npos);
}

TEST_CASE("an empty status response is rejected but harmless", "[protocol]") {
    protocol::DaemonStatus status;
    status.manual = true;
    REQUIRE_FALSE(protocol::ParseStatusResponse("", status));
    // A rejected response leaves the fields it did not mention untouched.
    REQUIRE(status.manual);
}

TEST_CASE("remain seconds round trip through the status response",
          "[protocol]") {
    protocol::DaemonStatus sent;
    sent.manualRemaining = 3725; // 1h2m5s of a timed keep-awake
    sent.screenRemaining = -1;   // "no expiry" marker
    const std::string body = protocol::SerializeStatus(sent);

    protocol::DaemonStatus received;
    REQUIRE(protocol::ParseStatusResponse(body, received));
    REQUIRE(received.manualRemaining == 3725);
    REQUIRE(received.screenRemaining == -1);
}

// -----------------------------------------------------------------------------
// Power event awareness (F13)
// -----------------------------------------------------------------------------

TEST_CASE("resumeAgo round trips through the status response", "[protocol]") {
    protocol::DaemonStatus sent;
    sent.resumeAgo = 3725; // 1h2m5s since the last resume
    const std::string body = protocol::SerializeStatus(sent);

    protocol::DaemonStatus received;
    REQUIRE(protocol::ParseStatusResponse(body, received));
    REQUIRE(received.resumeAgo == 3725);
}

TEST_CASE("resumeAgo -1 (no resume recorded) round trips", "[protocol]") {
    protocol::DaemonStatus sent; // default resumeAgo = -1
    const std::string body = protocol::SerializeStatus(sent);
    REQUIRE(body.find("RESUME_AGO=-1\n") != std::string::npos);

    protocol::DaemonStatus received;
    REQUIRE(protocol::ParseStatusResponse(body, received));
    REQUIRE(received.resumeAgo == -1);
}

TEST_CASE("a daemon without F13 keeps resumeAgo at -1", "[protocol]") {
    protocol::DaemonStatus status;
    // A response without the RESUME_AGO key (older daemon) must not change
    // the default.
    REQUIRE(protocol::ParseStatusResponse("OK\nMANUAL=1\n", status));
    REQUIRE(status.resumeAgo == -1);
}

TEST_CASE("a malformed RESUME_AGO value degrades to 0, not a crash",
          "[protocol]") {
    protocol::DaemonStatus status;
    REQUIRE(protocol::ParseStatusResponse("OK\nRESUME_AGO=\n", status));
    // atoi("") == 0; acceptable degradation for a malformed line.
    REQUIRE(status.resumeAgo == 0);
}
