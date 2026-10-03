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
// state.h - the shared daemon state (awake.daemon.exe).
//
// The daemon has two threads: the main thread serves pipe requests and the
// power thread applies the execution state. All data shared between them
// (the keep-awake flags and the shutdown flag, guarded by a critical
// section, plus the wake event that interrupts the power thread's poll
// wait) lives behind this interface so that no raw globals cross
// translation unit boundaries.
// =============================================================================

#ifndef AWAKE_DAEMON_STATE_H
#define AWAKE_DAEMON_STATE_H

#include <windows.h>

namespace daemonstate {

// Creates the critical section and the wake event. Call once at startup.
void Init();

// Destroys the wake event and the critical section. Call once at exit,
// after the power thread has been stopped.
void Shutdown();

// Keep-awake set with "awake 1" / cleared with "awake 0".
bool GetManual();
void SetManual(bool enabled);

// Screen keep-awake set with "awake screen on" / cleared with "... off".
bool GetScreen();
void SetScreen(bool enabled);

// True while a watched application is currently running. Only the power
// thread writes this (it owns the process scan results); the server thread
// only reads it for GET_STATUS.
bool GetAppActive();
void SetAppActive(bool active);

// Shutdown flag: set by the SHUTDOWN command. RequestExit() also clears
// the manual keep-awake flag so that the power thread's final pass leaves
// no execution state requirement behind.
bool GetExitRequested();
void RequestExit();

// Wakes the power thread so it applies the desired state immediately
// instead of waiting for the next 30 second poll.
void WakePowerThread();

// Returns the wake event handle, used by the power thread's wait.
HANDLE WakeEvent();

} // namespace daemonstate

#endif // AWAKE_DAEMON_STATE_H
