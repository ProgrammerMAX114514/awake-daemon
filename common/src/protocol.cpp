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
// protocol.cpp - implementation of the client/daemon wire protocol.
// =============================================================================

#include "protocol.h"

#include "version.h"

namespace protocol {

const char* const kReqPing      = "PING";
const char* const kReqGetStatus = "GET_STATUS";
const char* const kReqGetApps   = "GET_APPS";
const char* const kReqReload    = "RELOAD";
const char* const kReqShutdown  = "SHUTDOWN";

std::string MakeKeepAwakeRequest(bool enable) {
    return enable ? "KEEP_AWAKE 1" : "KEEP_AWAKE 0";
}

std::string MakeScreenRequest(bool enable) {
    return enable ? "SCREEN 1" : "SCREEN 0";
}

std::string MakePingRequest() {
    // The client announces its own version with the ping; the daemon
    // echoes its version back in the PONG (see ParsePongResponse).
    return std::string(kReqPing) + " " + version::kVersion;
}

std::string SerializeStatus(const DaemonStatus& status) {
    std::string response = "OK\n";
    response += "MANUAL=" + std::string(status.manual ? "1" : "0") + "\n";
    response += "APP=" + std::string(status.app ? "1" : "0") + "\n";
    response += "SCREEN=" + std::string(status.screen ? "1" : "0") + "\n";
    response += "ACTIVE=" + std::string(status.active ? "1" : "0") + "\n";
    response += "VERSION=" + status.version + "\n";
    return response;
}

bool ParseStatusResponse(const std::string& response, DaemonStatus& status) {
    const bool ok = response.compare(0, 2, "OK") == 0;

    // Walk the response line by line and pick out the known KEY=VALUE
    // pairs. Unknown lines are ignored, so future protocol extensions do
    // not break older clients.
    size_t lineStart = 0;
    while (lineStart <= response.size()) {
        size_t lineEnd = response.find('\n', lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = response.size();
        }
        const std::string line = response.substr(lineStart, lineEnd - lineStart);
        if (line.compare(0, 7, "MANUAL=") == 0) {
            status.manual = (line.compare(7, 1, "1") == 0);
        } else if (line.compare(0, 4, "APP=") == 0) {
            status.app = (line.compare(4, 1, "1") == 0);
        } else if (line.compare(0, 7, "SCREEN=") == 0) {
            status.screen = (line.compare(7, 1, "1") == 0);
        } else if (line.compare(0, 7, "ACTIVE=") == 0) {
            status.active = (line.compare(7, 1, "1") == 0);
        } else if (line.compare(0, 8, "VERSION=") == 0) {
            status.version = line.substr(8);
        }
        if (lineEnd == response.size()) {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return ok;
}

bool ParsePongResponse(const std::string& response, std::string& daemonVersion) {
    daemonVersion.clear();
    if (response.compare(0, 7, "OK PONG") != 0) {
        return false;
    }
    // Everything after "OK PONG " on the first line is the daemon version.
    const size_t lineEnd = response.find('\n');
    const size_t versionStart = 7; // right after "OK PONG"
    if (lineEnd != std::string::npos && lineEnd > versionStart) {
        daemonVersion = response.substr(versionStart, lineEnd - versionStart);
    }
    return true;
}

} // namespace protocol
