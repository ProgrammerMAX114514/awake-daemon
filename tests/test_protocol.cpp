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
