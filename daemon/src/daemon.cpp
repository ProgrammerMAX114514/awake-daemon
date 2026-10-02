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
// daemon.cpp - awake.daemon.exe, the background daemon.
//
// Responsibilities:
//   1. Hold the Windows execution state that blocks idle sleep/hibernation
//      when required (see below).
//   2. Every 30 seconds, scan the running processes and keep the execution
//      state while any image name from awake.ini is running.
//   3. Serve client requests (awake.exe) on the named pipe
//      "\\.\pipe\awake_daemon": PING, KEEP_AWAKE, SCREEN, GET_STATUS,
//      GET_APPS, RELOAD and SHUTDOWN.
//
// The daemon must not be started by hand: it refuses to run unless it was
// launched by the client with the hidden option "--internal-daemon <token>".
//
// Threading model:
//   - The main thread runs the blocking pipe server loop.
//   - A single "power thread" exclusively owns all SetThreadExecutionState
//     calls (the ES_CONTINUOUS flag is per-thread, so all calls must happen
//     on the same thread). It applies the desired state immediately after
//     a client request and at least every 30 seconds (poll interval), then
//     resets the state once more before the daemon exits.
// =============================================================================

#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "config.h"
#include "console.h"
#include "ipc.h"
#include "process.h"
#include "version.h"

// -----------------------------------------------------------------------------
// Shared daemon state (guarded by g_lock)
// -----------------------------------------------------------------------------

namespace {

const unsigned int kPollIntervalMs = 30 * 1000; // watch list poll interval: 30 s

CRITICAL_SECTION g_lock;          // guards everything below
HANDLE g_wakeEvent = NULL;        // wakes the power thread on demand
bool   g_manualKeepAwake = false; // set by "awake 1", cleared by "awake 0"
bool   g_appKeepAwake = false;    // true while a watched application runs
bool   g_screenKeepAwake = false; // set by "awake screen on", cleared by "awake screen off"
bool   g_exitRequested = false;   // set by the SHUTDOWN command

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

    bool manual;
    bool screen;
    EnterCriticalSection(&g_lock);
    manual = g_manualKeepAwake;
    screen = g_screenKeepAwake;
    LeaveCriticalSection(&g_lock);

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
    EnterCriticalSection(&g_lock);
    g_appKeepAwake = appActive;
    LeaveCriticalSection(&g_lock);
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

        WaitForSingleObject(g_wakeEvent, kPollIntervalMs);

        EnterCriticalSection(&g_lock);
        const bool exitRequested = g_exitRequested;
        LeaveCriticalSection(&g_lock);
        if (exitRequested) {
            break;
        }
    }
    // Final cleanup: clear every requirement held by this thread.
    SetThreadExecutionState(ES_CONTINUOUS);
    return 0;
}

// -----------------------------------------------------------------------------
// Request handling
// -----------------------------------------------------------------------------

// Builds the GET_STATUS response body: "OK" plus KEY=VALUE lines carrying
// the cached daemon state.
std::string HandleGetStatus() {
    EnterCriticalSection(&g_lock);
    const bool manual = g_manualKeepAwake;
    const bool app = g_appKeepAwake;
    const bool screen = g_screenKeepAwake;
    LeaveCriticalSection(&g_lock);

    std::string response = "OK\n";
    response += "MANUAL=" + std::string(manual ? "1" : "0") + "\n";
    response += "APP=" + std::string(app ? "1" : "0") + "\n";
    response += "SCREEN=" + std::string(screen ? "1" : "0") + "\n";
    response += "ACTIVE=" + std::string((manual || app || screen) ? "1" : "0") + "\n";
    return response;
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
            EnterCriticalSection(&g_lock);
            g_manualKeepAwake = true;
            LeaveCriticalSection(&g_lock);
            SetEvent(g_wakeEvent); // apply immediately instead of waiting for the next poll
            return "OK keep-awake enabled\n";
        }
        if (arg == "0") {
            EnterCriticalSection(&g_lock);
            g_manualKeepAwake = false;
            LeaveCriticalSection(&g_lock);
            SetEvent(g_wakeEvent);
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
            EnterCriticalSection(&g_lock);
            g_screenKeepAwake = true;
            LeaveCriticalSection(&g_lock);
            SetEvent(g_wakeEvent); // apply immediately instead of waiting for the next poll
            return "OK screen keep-awake enabled\n";
        }
        if (arg == "0") {
            EnterCriticalSection(&g_lock);
            g_screenKeepAwake = false;
            LeaveCriticalSection(&g_lock);
            SetEvent(g_wakeEvent);
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
        SetEvent(g_wakeEvent);
        return "OK configuration reload requested\n";
    }
    if (verb == "SHUTDOWN") {
        EnterCriticalSection(&g_lock);
        g_exitRequested = true;
        g_manualKeepAwake = false; // the power thread's final pass resets the state
        LeaveCriticalSection(&g_lock);
        SetEvent(g_wakeEvent);
        return "OK shutting down\n";
    }
    return "ERR unknown command\n";
}

// -----------------------------------------------------------------------------
// Pipe server loop (main thread)
// -----------------------------------------------------------------------------

// Creates the pipe, accepts one client at a time and dispatches requests.
// Returns when the SHUTDOWN command was served.
void PipeServerLoop() {
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

        EnterCriticalSection(&g_lock);
        const bool exitRequested = g_exitRequested;
        LeaveCriticalSection(&g_lock);
        if (exitRequested) {
            return;
        }
    }
}

} // namespace

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {
    console::EnableColors();

    // Refuse to run when not launched by the client with the secret token.
    // This is what makes the daemon "not callable from the command line".
    bool launchedInternally = false;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--internal-daemon") == 0 &&
            std::strcmp(argv[i + 1], ipc::kDaemonLaunchToken) == 0) {
            launchedInternally = true;
        }
    }
    if (!launchedInternally) {
        console::Printf(console::kColorRed, "Error: awake.daemon.exe (version %s) cannot be started directly.\n",
                        version::kVersion);
        console::PrintLine(console::kColorReset, "Use 'awake daemon on' to start the daemon.");
        return 1;
    }

    // Guarantee that only one daemon instance runs at a time.
    const HANDLE mutex = CreateMutexA(NULL, TRUE, ipc::kDaemonMutexName);
    if (mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1; // another daemon already owns the pipe; exit silently
    }

    // Make sure a configuration file exists before anything reads it.
    config::EnsureConfigExists();

    // Initialize the shared state and start the power thread.
    InitializeCriticalSection(&g_lock);
    g_wakeEvent = CreateEventA(NULL, FALSE, FALSE, NULL); // auto-reset event
    HANDLE powerThread = CreateThread(NULL, 0, PowerThreadProc, NULL, 0, NULL);
    if (powerThread == NULL) {
        console::PrintLine(console::kColorRed, "Error: failed to create the power thread.");
        return 1;
    }

    // Serve clients until SHUTDOWN arrives.
    PipeServerLoop();

    // Stop the power thread. The event (already set by SHUTDOWN) makes it
    // leave its wait, reset the execution state and return.
    EnterCriticalSection(&g_lock);
    g_exitRequested = true;
    LeaveCriticalSection(&g_lock);
    SetEvent(g_wakeEvent);
    WaitForSingleObject(powerThread, INFINITE);
    CloseHandle(powerThread);
    CloseHandle(g_wakeEvent);
    DeleteCriticalSection(&g_lock);

    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
