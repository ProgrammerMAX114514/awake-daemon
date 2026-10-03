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
// server.h - the daemon's named pipe server (awake.daemon.exe).
//
// Serves client requests (awake.exe) on the named pipe
// "\\.\pipe\awake_daemon": PING, KEEP_AWAKE, SCREEN, GET_STATUS, GET_APPS,
// RELOAD and SHUTDOWN.
// =============================================================================

#ifndef AWAKE_DAEMON_SERVER_H
#define AWAKE_DAEMON_SERVER_H

namespace server {

// Creates the pipe, accepts one client at a time and dispatches requests.
// Blocks until the SHUTDOWN command was served, then returns.
void RunPipeServer();

} // namespace server

#endif // AWAKE_DAEMON_SERVER_H
