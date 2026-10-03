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
// power.cpp - implementation of the daemon's power thread.
// =============================================================================

#include "power.h"

#include <windows.h>

#include <set>
#include <string>
#include <vector>

#include "config.h"
#include "process.h"
#include "state.h"

namespace power {

namespace {

const unsigned int kPollIntervalMs = 30 * 1000; // watch list poll interval: 30 s

// Applies the currently desired execution state. MUST be called on the
// power thread only. SetThreadExecutionState(ES_CONTINUOUS) without
// ES_SYSTEM_REQUIRED clears a previously set "stay awake" requirement on
// this thread; with ES_SYSTEM_REQUIRED it blocks idle sleep/hibernation.
// Manual sleep (power button, Start menu, other applications) is never
// blocked - that behavior is exactly what the tool promises.
void ApplyDesiredState() {
    // Reload the configuration on every pass so that "awake add"/"awake del"
    // changes take effect without a daemon restart (hot reload).
    const std::vector<std::string> watchList = config::ReadWatchList();
    const std::set<std::string> running = process::FindRunningImages(watchList);

    const bool manual = daemonstate::GetManual();
    const bool screen = daemonstate::GetScreen();
    const bool appActive = !running.empty();

    // The system must stay in the working state when keep-awake is active,
    // a watched application runs, or the screen is held on (a sleeping
    // system would turn the display off).
    // ES_DISPLAY_REQUIRED additionally resets the display idle timer, which
    // keeps the screen on and prevents the screensaver and the idle lock.
    if (manual || appActive || screen) {
        DWORD flags = ES_CONTINUOUS | ES_SYSTEM_REQUIRED;
        if (screen) {
            flags |= ES_DISPLAY_REQUIRED;
        }
        SetThreadExecutionState(flags);
    } else {
        SetThreadExecutionState(ES_CONTINUOUS);
    }

    // Cache the result for GET_STATUS / GET_APPS answers.
    daemonstate::SetAppActive(appActive);
}

// Power thread: applies the desired state, then sleeps until either the
// poll interval (30 s) elapses or a client request signals the event.
// Before exiting it resets the execution state one last time so the
// daemon never leaves a stray "keep awake" requirement behind.
DWORD WINAPI PowerThreadProc(LPVOID /*param*/) {
    // One-shot notification right after the daemon starts: regardless of
    // the current keep-awake setting, tell the system once to stay awake.
    // ES_SYSTEM_REQUIRED without ES_CONTINUOUS affects only the next idle
    // evaluation (it resets the idle timer once) and does NOT latch a
    // persistent keep-awake requirement - the loop below keeps applying
    // the state that matches the current settings.
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);

    for (;;) {
        ApplyDesiredState();

        WaitForSingleObject(daemonstate::WakeEvent(), kPollIntervalMs);

        if (daemonstate::GetExitRequested()) {
            break;
        }
    }
    // Final cleanup: clear every requirement held by this thread.
    SetThreadExecutionState(ES_CONTINUOUS);
    return 0;
}

} // namespace

HANDLE StartPowerThread() {
    HANDLE thread = CreateThread(NULL, 0, PowerThreadProc, NULL, 0, NULL);
    return thread;
}

void StopPowerThread(HANDLE thread) {
    daemonstate::RequestExit();
    daemonstate::WakePowerThread();
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

} // namespace power
