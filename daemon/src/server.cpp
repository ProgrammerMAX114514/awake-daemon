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
// server.cpp - implementation of the daemon's named pipe server.
// =============================================================================

#include "server.h"

#include <windows.h>

#include <set>
#include <string>
#include <vector>

#include "config.h"
#include "ipc.h"
#include "process.h"
#include "protocol.h"
#include "state.h"

namespace server {

namespace {

// Builds the GET_STATUS response: "OK" plus KEY=VALUE lines carrying the
// cached daemon state (serialized by the shared protocol layer).
std::string HandleGetStatus() {
    protocol::DaemonStatus status;
    status.manual = daemonstate::GetManual();
    status.app = daemonstate::GetAppActive();
    status.screen = daemonstate::GetScreen();
    status.active = (status.manual || status.app || status.screen);
    return protocol::SerializeStatus(status);
}

// Builds the GET_APPS response: one "APP <name> <running>" line per watch
// list entry (running is freshly checked), terminated by "END".
std::string HandleGetApps() {
    const std::vector<std::string> watchList = config::ReadWatchList();
    const std::set<std::string> running = process::FindRunningImages(watchList);

    std::string response = "OK\n";
    for (size_t i = 0; i < watchList.size(); ++i) {
        response += "APP " + watchList[i] + " ";
        response += (running.count(config::ToLowerAscii(watchList[i])) > 0) ? "1" : "0";
        response += "\n";
    }
    response += "END\n";
    return response;
}

// Parses one request line and produces the response. May set the shutdown
// flag for the SHUTDOWN command.
std::string HandleRequest(const std::string& request) {
    // Strip the trailing newline/whitespace that the request line carries,
    // so that argument-less verbs ("PING\n") compare equal to their name.
    std::string line = request;
    while (!line.empty() && (line[line.size() - 1] == '\n' ||
                             line[line.size() - 1] == '\r' ||
                             line[line.size() - 1] == ' ')) {
        line.erase(line.size() - 1);
    }

    // Split off the first word and the rest of the line.
    const size_t firstSpace = line.find(' ');
    const std::string verb = line.substr(0, firstSpace == std::string::npos ? line.size() : firstSpace);

    if (verb == "PING") {
        return "OK PONG\n";
    }
    if (verb == "KEEP_AWAKE") {
        // The line is already trimmed, so the argument is clean.
        const std::string arg = (firstSpace == std::string::npos)
                                    ? ""
                                    : line.substr(firstSpace + 1);
        if (arg == "1") {
            daemonstate::SetManual(true);
            daemonstate::WakePowerThread(); // apply immediately instead of waiting for the next poll
            return "OK keep-awake enabled\n";
        }
        if (arg == "0") {
            daemonstate::SetManual(false);
            daemonstate::WakePowerThread();
            return "OK keep-awake disabled\n";
        }
        return "ERR KEEP_AWAKE expects 0 or 1\n";
    }
    if (verb == "SCREEN") {
        // Same argument handling as KEEP_AWAKE: the line is already
        // trimmed, so the argument is a clean "0" or "1".
        const std::string arg = (firstSpace == std::string::npos)
                                    ? ""
                                    : line.substr(firstSpace + 1);
        if (arg == "1") {
            daemonstate::SetScreen(true);
            daemonstate::WakePowerThread(); // apply immediately instead of waiting for the next poll
            return "OK screen keep-awake enabled\n";
        }
        if (arg == "0") {
            daemonstate::SetScreen(false);
            daemonstate::WakePowerThread();
            return "OK screen keep-awake disabled\n";
        }
        return "ERR SCREEN expects 0 or 1\n";
    }
    if (verb == "GET_STATUS") {
        return HandleGetStatus();
    }
    if (verb == "GET_APPS") {
        return HandleGetApps();
    }
    if (verb == "RELOAD") {
        // Wake the power thread: its next pass re-reads the configuration
        // file and re-applies the desired state immediately, instead of
        // waiting up to 30 seconds for the regular poll.
        daemonstate::WakePowerThread();
        return "OK configuration reload requested\n";
    }
    if (verb == "SHUTDOWN") {
        daemonstate::RequestExit();
        daemonstate::WakePowerThread();
        return "OK shutting down\n";
    }
    return "ERR unknown command\n";
}

} // namespace

// Creates the pipe, accepts one client at a time and dispatches requests.
// Returns when the SHUTDOWN command was served.
void RunPipeServer() {
    for (;;) {
        // A single pipe instance is enough for this tool. A second client
        // that arrives while one is being served receives ERROR_PIPE_BUSY
        // and waits via WaitNamedPipe (handled in ipc.cpp).
        HANDLE pipe = CreateNamedPipeA(ipc::kPipeName,
                                       PIPE_ACCESS_DUPLEX,
                                       PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       1,      // one instance at a time
                                       4096,   // output buffer
                                       4096,   // input buffer
                                       0,      // default timeout
                                       NULL);  // default security
        if (pipe == INVALID_HANDLE_VALUE) {
            // Extremely unlikely (e.g. another process grabbed the name);
            // wait and retry instead of spinning hot.
            Sleep(1000);
            continue;
        }

        // Block until a client connects. ERROR_PIPE_CONNECTED just means
        // the client connected between CreateNamedPipe and this call,
        // which is fine.
        const BOOL connected = ConnectNamedPipe(pipe, NULL)
                                   ? TRUE
                                   : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            const std::string request = ipc::ReadRequestFromPipe(pipe);
            if (!request.empty()) {
                const std::string response = HandleRequest(request);
                DWORD written = 0;
                WriteFile(pipe, response.c_str(),
                          static_cast<DWORD>(response.size()), &written, NULL);
                FlushFileBuffers(pipe); // make sure the client sees everything
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);

        if (daemonstate::GetExitRequested()) {
            return;
        }
    }
}

} // namespace server
