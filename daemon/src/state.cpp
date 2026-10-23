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

// Expiry points of timed keep-awake periods as GetTickCount64() values; 0
// means "no expiry". A monotonic clock is used on purpose: changing the system
// time or crossing a daylight saving boundary cannot move an expiry point.
ULONGLONG g_manualDeadline = 0;
ULONGLONG g_screenDeadline = 0;

// Turns a period in seconds into a deadline; 0 stays 0 (no expiry).
ULONGLONG DeadlineFromNow(unsigned int seconds) {
    if (seconds == 0) {
        return 0;
    }
    return GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000ULL;
}

// Remaining seconds for a flag that is currently on. Caller holds the lock.
int RemainingSeconds(bool enabled, ULONGLONG deadline) {
    if (!enabled) {
        return 0;
    }
    if (deadline == 0) {
        return -1; // no expiry
    }
    const ULONGLONG now = GetTickCount64();
    if (now >= deadline) {
        return 0; // expired, ExpireTimers() will clear it
    }
    // Round up, so "1 second left" is not shown as 0 while still active.
    return static_cast<int>(((deadline - now) + 999ULL) / 1000ULL);
}

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
    SetManualTimed(enabled, 0);
}

void SetManualTimed(bool enabled, unsigned int seconds) {
    EnterCriticalSection(&g_lock);
    g_manualKeepAwake = enabled;
    g_manualDeadline = DeadlineFromNow(enabled ? seconds : 0);
    LeaveCriticalSection(&g_lock);
}

bool GetScreen() {
    EnterCriticalSection(&g_lock);
    const bool screen = g_screenKeepAwake;
    LeaveCriticalSection(&g_lock);
    return screen;
}

void SetScreen(bool enabled) {
    SetScreenTimed(enabled, 0);
}

void SetScreenTimed(bool enabled, unsigned int seconds) {
    EnterCriticalSection(&g_lock);
    g_screenKeepAwake = enabled;
    g_screenDeadline = DeadlineFromNow(enabled ? seconds : 0);
    LeaveCriticalSection(&g_lock);
}

int GetManualRemaining() {
    EnterCriticalSection(&g_lock);
    const int remaining = RemainingSeconds(g_manualKeepAwake, g_manualDeadline);
    LeaveCriticalSection(&g_lock);
    return remaining;
}

int GetScreenRemaining() {
    EnterCriticalSection(&g_lock);
    const int remaining = RemainingSeconds(g_screenKeepAwake, g_screenDeadline);
    LeaveCriticalSection(&g_lock);
    return remaining;
}

bool ExpireTimers() {
    EnterCriticalSection(&g_lock);
    const ULONGLONG now = GetTickCount64();
    bool changed = false;
    if (g_manualKeepAwake && g_manualDeadline != 0 && now >= g_manualDeadline) {
        g_manualKeepAwake = false;
        g_manualDeadline = 0;
        changed = true;
    }
    if (g_screenKeepAwake && g_screenDeadline != 0 && now >= g_screenDeadline) {
        g_screenKeepAwake = false;
        g_screenDeadline = 0;
        changed = true;
    }
    LeaveCriticalSection(&g_lock);
    return changed;
}

unsigned long long MillisecondsUntilExpiry() {
    EnterCriticalSection(&g_lock);
    const ULONGLONG now = GetTickCount64();
    unsigned long long best = 0;
    const ULONGLONG deadlines[2] = { g_manualDeadline, g_screenDeadline };
    const bool active[2] = { g_manualKeepAwake, g_screenKeepAwake };
    for (int i = 0; i < 2; ++i) {
        if (!active[i] || deadlines[i] == 0) {
            continue;
        }
        // An already expired deadline maps to 1 ms: the next pass clears it,
        // and 0 stays reserved for "no timer at all".
        const unsigned long long left = (now >= deadlines[i]) ? 1ULL : (deadlines[i] - now);
        if (best == 0 || left < best) {
            best = left;
        }
    }
    LeaveCriticalSection(&g_lock);
    return best;
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
