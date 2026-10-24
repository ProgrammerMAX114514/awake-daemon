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
// monitor.h - power event monitor of the daemon (awake.daemon.exe).
//
// Runs a hidden top-level window on a dedicated thread. Windows broadcasts
// WM_POWERBROADCAST to every top-level window, so the window receives the
// suspend/resume notifications without registering any extra API; on resume
// the shared state is updated and the power thread is woken to re-apply the
// execution state immediately (F13).
// =============================================================================

#ifndef AWAKE_DAEMON_MONITOR_H
#define AWAKE_DAEMON_MONITOR_H

namespace monitor {

// Creates the hidden power event window and its message loop thread.
// Call once after daemonstate::Init().
void StartPowerMonitor();

// Stops the monitor thread (posts WM_QUIT to it) and waits for it to exit.
// Call once before daemonstate::Shutdown().
void StopPowerMonitor();

} // namespace monitor

#endif // AWAKE_DAEMON_MONITOR_H
