// =============================================================================
// daemon.cpp - awake.daemon.exe, the background daemon.
//
// Responsibilities:
//   1. Hold the Windows execution state that blocks idle sleep/hibernation
//      when required (see below).
//   2. Every 30 seconds, scan the running processes and keep the execution
//      state while any image name from awake.ini is running.
//   3. Serve client requests (awake.exe) on the named pipe
//      "\\.\pipe\awake_daemon": PING, KEEP_AWAKE, GET_STATUS, GET_APPS and
//      SHUTDOWN.
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

// -----------------------------------------------------------------------------
// Shared daemon state (guarded by g_lock)
// -----------------------------------------------------------------------------

namespace {

const unsigned int kPollIntervalMs = 30 * 1000; // watch list poll interval: 30 s

CRITICAL_SECTION g_lock;          // guards everything below
HANDLE g_wakeEvent = NULL;        // wakes the power thread on demand
bool   g_manualKeepAwake = false; // set by "awake 1", cleared by "awake 0"
bool   g_appKeepAwake = false;    // true while a watched application runs
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
    EnterCriticalSection(&g_lock);
    manual = g_manualKeepAwake;
    LeaveCriticalSection(&g_lock);

    const bool appActive = !running.empty();

    if (manual || appActive) {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
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
    LeaveCriticalSection(&g_lock);

    std::string response = "OK\n";
    response += "MANUAL=" + std::string(manual ? "1" : "0") + "\n";
    response += "APP=" + std::string(app ? "1" : "0") + "\n";
    response += "ACTIVE=" + std::string((manual || app) ? "1" : "0") + "\n";
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
    // Split off the first word and the rest of the line.
    const size_t firstSpace = request.find(' ');
    const std::string verb = request.substr(0, firstSpace == std::string::npos ? request.size() : firstSpace);

    if (verb == "PING") {
        return "OK PONG\n";
    }
    if (verb == "KEEP_AWAKE") {
        // Extract the argument and strip the trailing newline/whitespace
        // that the request line carries.
        std::string arg = (firstSpace == std::string::npos)
                              ? ""
                              : request.substr(firstSpace + 1);
        while (!arg.empty() && (arg[arg.size() - 1] == '\n' ||
                                arg[arg.size() - 1] == '\r' ||
                                arg[arg.size() - 1] == ' ')) {
            arg.erase(arg.size() - 1);
        }
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
    if (verb == "GET_STATUS") {
        return HandleGetStatus();
    }
    if (verb == "GET_APPS") {
        return HandleGetApps();
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
        console::PrintLine(console::kColorRed, "Error: awake.daemon.exe cannot be started directly.");
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
