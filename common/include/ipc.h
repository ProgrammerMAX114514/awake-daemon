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
// ipc.h - named pipe based communication between client and daemon.
//
// Transport:
//   The daemon owns one instance of the byte-mode named pipe
//   "\\.\pipe\awake_daemon". A client connects, writes one request line,
//   reads the response until the server disconnects, and closes its end.
//
// Protocol:
//   A request is a single line of plain ASCII text terminated by '\n'.
//   The response is one or more lines; the first token is "OK" or "ERR".
//
// The daemon is not meant to be started by hand. The client launches it
// with the hidden command line option --internal-daemon <token>; when the
// token is missing the daemon refuses to run.
// =============================================================================

#ifndef AWAKE_IPC_H
#define AWAKE_IPC_H

#include <string>

namespace ipc {

// Name of the named pipe used for client/daemon communication.
extern const char* const kPipeName;

// Secret argument required by awake.daemon.exe. The client passes it on
// launch so that the daemon cannot be started accidentally by hand.
extern const char* const kDaemonLaunchToken;

// Name of the mutex the daemon holds to guarantee a single instance.
extern const char* const kDaemonMutexName;

// Name of the daemon executable, expected in the client's directory.
extern const char* const kDaemonExeName;

// Sends one request line (a trailing '\n' is appended automatically) to the
// daemon and returns the full response text. Returns false when the daemon
// cannot be reached at all.
bool SendRequest(const std::string& request, std::string& response);

// Convenience wrapper: sends "PING" and reports whether the daemon answered.
bool IsDaemonRunning();

// Starts the daemon as a detached background process. The daemon
// executable is looked up next to the client executable. Returns false when
// the process could not be created.
bool StartDaemon();

// Polls the daemon with PING requests until it answers or the timeout
// (in milliseconds) elapses. Returns true once the daemon is reachable.
bool WaitForDaemon(unsigned int timeoutMs);

// Server side helper: reads one request line from a connected pipe instance
// (blocks until a '\n' arrives or the client disconnects).
std::string ReadRequestFromPipe(void* pipeHandle);

} // namespace ipc

#endif // AWAKE_IPC_H
