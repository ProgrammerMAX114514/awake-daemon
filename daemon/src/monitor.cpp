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
// monitor.cpp - implementation of the daemon's power event monitor.
//
// A hidden top-level window is the classic, dependency-free way to receive
// WM_POWERBROADCAST (the broadcast reaches every top-level window of the
// process). The window lives on its own thread running a plain GetMessage
// loop, so the blocking pipe server thread and the power thread are not
// involved at all.
// =============================================================================

#include "monitor.h"

#include <windows.h>

#include "state.h"

namespace monitor {

namespace {

// Window class name; registered once per process by the monitor thread.
const wchar_t kWindowClassName[] = L"awake_power_monitor_window";

// Monitor thread bookkeeping (written by Start/StopPowerMonitor only).
HANDLE g_monitorThread = NULL;
DWORD g_monitorThreadId = 0;

// The window procedure only cares about power broadcasts; everything else
// falls through to DefWindowProc.
LRESULT CALLBACK PowerMonitorWndProc(HWND window, UINT message,
                                     WPARAM wParam, LPARAM lParam) {
    if (message == WM_POWERBROADCAST) {
        // PBT_APMRESUMEAUTOMATIC is sent after any wake from sleep; some
        // systems additionally send PBT_APMRESUMESUSPEND when the user was
        // involved. Both mean "the system is running again" - record the
        // resume and wake the power thread. PBT_APMSUSPEND (going to sleep)
        // needs no action.
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            daemonstate::RecordResume(); // also wakes the power thread
        }
        return TRUE; // handled
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

// Monitor thread: register the window class, create the hidden window and
// pump messages until WM_QUIT arrives.
DWORD WINAPI MonitorThreadProc(LPVOID /*param*/) {
    WNDCLASSEXW windowClass;
    ZeroMemory(&windowClass, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = PowerMonitorWndProc;
    windowClass.hInstance = GetModuleHandleW(NULL);
    windowClass.lpszClassName = kWindowClassName;
    RegisterClassExW(&windowClass); // fine if it already exists

    // A message-only child (HWND_MESSAGE) would NOT receive broadcasts, so
    // this must be a real top-level window - kept invisible, zero sized and
    // marked as a tool window so it never shows up anywhere.
    const HWND window = CreateWindowExW(WS_EX_TOOLWINDOW,
                                        kWindowClassName,
                                        L"awake power monitor",
                                        WS_POPUP,
                                        0, 0, 0, 0,
                                        NULL, NULL,
                                        GetModuleHandleW(NULL),
                                        NULL);
    if (window == NULL) {
        // Without the window there are no power broadcasts; the 30 second
        // poll in the power thread remains as the fallback. End the loop so
        // StopPowerMonitor does not wait forever.
        PostThreadMessage(GetCurrentThreadId(), WM_QUIT, 0, 0);
    }

    MSG message;
    BOOL received;
    while ((received = GetMessageW(&message, NULL, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (window != NULL) {
        DestroyWindow(window);
    }
    return 0;
}

} // namespace

void StartPowerMonitor() {
    // The handle is kept (not closed) so StopPowerMonitor can wait for a
    // clean exit of the message loop.
    g_monitorThread = CreateThread(NULL, 0, MonitorThreadProc, NULL, 0,
                                   &g_monitorThreadId);
}

void StopPowerMonitor() {
    if (g_monitorThread == NULL) {
        return;
    }
    // WM_QUIT ends the GetMessage loop; the thread destroys its window and
    // returns. The wait is bounded anyway to keep a hung loop from blocking
    // the daemon's shutdown path.
    PostThreadMessage(g_monitorThreadId, WM_QUIT, 0, 0);
    WaitForSingleObject(g_monitorThread, 2000);
    CloseHandle(g_monitorThread);
    g_monitorThread = NULL;
    g_monitorThreadId = 0;
}

} // namespace monitor
