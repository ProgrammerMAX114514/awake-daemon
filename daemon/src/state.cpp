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
// state.cpp - implementation of the shared daemon state.
// =============================================================================

#include "state.h"

namespace daemonstate {

namespace {

CRITICAL_SECTION g_lock;          // guards the flags below
HANDLE g_wakeEvent = NULL;        // wakes the power thread on demand
bool   g_manualKeepAwake = false; // set by "awake 1", cleared by "awake 0"
bool   g_appKeepAwake = false;    // true while a watched application runs
bool   g_screenKeepAwake = false; // set by "awake screen on", cleared by "awake screen off"
bool   g_exitRequested = false;   // set by the SHUTDOWN command

} // namespace

void Init() {
    InitializeCriticalSection(&g_lock);
    // Auto-reset event: a single SetEvent releases exactly one wait.
    g_wakeEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
}

void Shutdown() {
    if (g_wakeEvent != NULL) {
        CloseHandle(g_wakeEvent);
        g_wakeEvent = NULL;
    }
    DeleteCriticalSection(&g_lock);
}

bool GetManual() {
    EnterCriticalSection(&g_lock);
    const bool manual = g_manualKeepAwake;
    LeaveCriticalSection(&g_lock);
    return manual;
}

void SetManual(bool enabled) {
    EnterCriticalSection(&g_lock);
    g_manualKeepAwake = enabled;
    LeaveCriticalSection(&g_lock);
}

bool GetScreen() {
    EnterCriticalSection(&g_lock);
    const bool screen = g_screenKeepAwake;
    LeaveCriticalSection(&g_lock);
    return screen;
}

void SetScreen(bool enabled) {
    EnterCriticalSection(&g_lock);
    g_screenKeepAwake = enabled;
    LeaveCriticalSection(&g_lock);
}

bool GetAppActive() {
    EnterCriticalSection(&g_lock);
    const bool app = g_appKeepAwake;
    LeaveCriticalSection(&g_lock);
    return app;
}

void SetAppActive(bool active) {
    EnterCriticalSection(&g_lock);
    g_appKeepAwake = active;
    LeaveCriticalSection(&g_lock);
}

bool GetExitRequested() {
    EnterCriticalSection(&g_lock);
    const bool exitRequested = g_exitRequested;
    LeaveCriticalSection(&g_lock);
    return exitRequested;
}

void RequestExit() {
    EnterCriticalSection(&g_lock);
    g_exitRequested = true;
    g_manualKeepAwake = false; // the power thread's final pass resets the state
    LeaveCriticalSection(&g_lock);
}

void WakePowerThread() {
    SetEvent(g_wakeEvent);
}

HANDLE WakeEvent() {
    return g_wakeEvent;
}

} // namespace daemonstate
