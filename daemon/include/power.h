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
// power.h - the daemon's power thread (awake.daemon.exe).
//
// The power thread exclusively owns all SetThreadExecutionState calls (the
// ES_CONTINUOUS flag is per-thread, so all calls must happen on the same
// thread). It applies the desired state immediately after a client request
// and at least every 30 seconds (poll interval), then resets the state once
// more before the daemon exits.
// =============================================================================

#ifndef AWAKE_DAEMON_POWER_H
#define AWAKE_DAEMON_POWER_H

#include <windows.h>

namespace power {

// Creates and starts the power thread. Returns the thread handle, or NULL
// when the thread could not be created.
HANDLE StartPowerThread();

// Asks the power thread to stop (via the shared state), waits for it to
// finish its final state reset and closes the thread handle. Blocks until
// the thread has exited.
void StopPowerThread(HANDLE thread);

} // namespace power

#endif // AWAKE_DAEMON_POWER_H
