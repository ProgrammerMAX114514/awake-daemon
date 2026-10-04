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
// protocol.h - the client/daemon wire protocol in one place.
//
// The client and the daemon communicate over the named pipe with a small
// line-based ASCII protocol (see ipc.h for the transport). This header is
// the single source of truth for that protocol:
//
//   - request verbs and request line builders,
//   - the DaemonStatus structure carried by the GET_STATUS response,
//   - the serialization (daemon side) and parsing (client side) of the
//     GET_STATUS "KEY=VALUE" response.
//
// Both sides include this header, so the wire format cannot silently
// drift apart between the two executables.
// =============================================================================

#ifndef AWAKE_PROTOCOL_H
#define AWAKE_PROTOCOL_H

#include <string>

namespace protocol {

// -----------------------------------------------------------------------------
// Request verbs (first word of the request line)
// -----------------------------------------------------------------------------

extern const char* const kReqPing;        // "PING"                  -> "OK PONG <version>"
extern const char* const kReqGetStatus;   // "GET_STATUS"            -> status response
extern const char* const kReqGetApps;     // "GET_APPS"              -> app list response
extern const char* const kReqReload;      // "RELOAD"                -> "OK ..."
extern const char* const kReqShutdown;    // "SHUTDOWN"              -> "OK ..." (daemon exits)

// Builds the versioned ping request line ("PING <client version>"). The
// daemon answers "OK PONG <daemon version>"; comparing the two versions
// lets the client warn about client/daemon mismatches.
std::string MakePingRequest();

// Parses the PING response. Returns true when the response starts with
// "OK PONG"; daemonVersion then holds the version reported by the daemon
// (may be empty for daemons older than the version handshake).
bool ParsePongResponse(const std::string& response, std::string& daemonVersion);

// Builds the request line for "awake 0"/"awake 1" ("KEEP_AWAKE 0|1").
std::string MakeKeepAwakeRequest(bool enable);

// Builds the request line for "awake screen on/off" ("SCREEN 1|0").
std::string MakeScreenRequest(bool enable);

// -----------------------------------------------------------------------------
// GET_STATUS payload
// -----------------------------------------------------------------------------

// The state carried by the GET_STATUS response. Both sides share this
// structure: the daemon fills and serializes it, the client parses it.
struct DaemonStatus {
    bool manual = false;  // keep-awake set with "awake 1"
    bool app = false;     // a watched application is currently running
    bool screen = false;  // screen keep-awake set with "awake screen on"
    bool active = false;  // anything currently prevents idle sleep
    std::string version;  // daemon version (empty for older daemons)
};

// Serializes a DaemonStatus into the GET_STATUS response body, starting
// with the mandatory "OK" line (daemon side).
std::string SerializeStatus(const DaemonStatus& status);

// Parses a GET_STATUS response into a DaemonStatus (client side). Fields
// that are missing from the response stay false; the return value reports
// whether the response started with "OK" at all.
bool ParseStatusResponse(const std::string& response, DaemonStatus& status);

} // namespace protocol

#endif // AWAKE_PROTOCOL_H
