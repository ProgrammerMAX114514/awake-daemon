// =============================================================================
// ipc.cpp - implementation of the named pipe client and launcher helpers.
// =============================================================================

#include "ipc.h"

#include <windows.h>

#include "config.h"

namespace ipc {

const char* const kPipeName         = "\\\\.\\pipe\\awake_daemon";
const char* const kDaemonLaunchToken = "awake-internal-daemon-launch";
const char* const kDaemonMutexName   = "Local\\awake_daemon_single_instance";
const char* const kDaemonExeName     = "awake.daemon.exe";

bool SendRequest(const std::string& request, std::string& response) {
    response.clear();

    // Open a connection to the daemon's pipe. Two transient situations must
    // be retried:
    //   - ERROR_PIPE_BUSY: the single pipe instance is currently serving
    //     another client (wait for a free instance).
    //   - ERROR_FILE_NOT_FOUND: the daemon is between two request cycles,
    //     i.e. it just closed the served instance and has not re-created
    //     the pipe yet. A client firing back-to-back requests (PING followed
    //     by the real command) can hit that gap, so retry briefly.
    HANDLE pipe = INVALID_HANDLE_VALUE;
    const ULONGLONG deadline = GetTickCount64() + 2000; // 2 s connection budget
    for (;;) {
        pipe = CreateFileA(kPipeName,
                           GENERIC_READ | GENERIC_WRITE,
                           0,           // no sharing: exclusive client end
                           NULL,
                           OPEN_EXISTING,
                           0,
                           NULL);
        if (pipe != INVALID_HANDLE_VALUE) {
            break;
        }
        const DWORD error = GetLastError();
        const bool transient = (error == ERROR_PIPE_BUSY || error == ERROR_FILE_NOT_FOUND);
        if (!transient || GetTickCount64() >= deadline) {
            return false; // the daemon is not reachable
        }
        if (error == ERROR_PIPE_BUSY) {
            WaitNamedPipeA(kPipeName, 500);
        } else {
            Sleep(25);
        }
    }

    // Write the request line. The protocol only carries short ASCII text,
    // so a single unbuffered write is always sufficient.
    std::string line = request;
    if (line.empty() || line[line.size() - 1] != '\n') {
        line += '\n';
    }
    DWORD written = 0;
    if (!WriteFile(pipe, line.c_str(), static_cast<DWORD>(line.size()), &written, NULL)) {
        CloseHandle(pipe);
        return false;
    }

    // Read the response until the server disconnects its end, which it does
    // right after answering. ERROR_BROKEN_PIPE therefore ends the loop in
    // the normal, expected way.
    char buffer[512];
    for (;;) {
        DWORD bytesRead = 0;
        const BOOL ok = ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, NULL);
        if (!ok || bytesRead == 0) {
            break;
        }
        response.append(buffer, bytesRead);
    }
    CloseHandle(pipe);
    return true;
}

bool IsDaemonRunning() {
    std::string response;
    if (!SendRequest("PING", response)) {
        return false;
    }
    return response.compare(0, 7, "OK PONG") == 0;
}

bool StartDaemon() {
    // The daemon is expected to sit next to the client executable.
    const std::string exePath = config::GetExeDirectory() + "\\" + kDaemonExeName;

    // Command line: "<path to daemon>" --internal-daemon <token>
    std::string commandLine = "\"" + exePath + "\" --internal-daemon " + kDaemonLaunchToken;

    STARTUPINFOA startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo;
    ZeroMemory(&processInfo, sizeof(processInfo));

    // DETACHED_PROCESS: the daemon gets no console and keeps running when
    // the launching terminal is closed.
    // CREATE_NEW_PROCESS_GROUP: isolates the daemon from Ctrl+C events that
    // were generated in the launching console's process group.
    const BOOL created = CreateProcessA(NULL,                  // application name (from command line)
                                        &commandLine[0],       // mutable command line buffer
                                        NULL, NULL,            // default security attributes
                                        FALSE,                 // do not inherit handles
                                        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
                                        NULL, NULL,            // default environment and CWD
                                        &startupInfo,
                                        &processInfo);
    if (!created) {
        return false;
    }
    // The daemon runs on its own from now on; drop our references.
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}

bool WaitForDaemon(unsigned int timeoutMs) {
    const unsigned int attemptIntervalMs = 250;
    unsigned int waitedMs = 0;
    for (;;) {
        if (IsDaemonRunning()) {
            return true;
        }
        if (waitedMs >= timeoutMs) {
            return false;
        }
        Sleep(attemptIntervalMs);
        waitedMs += attemptIntervalMs;
    }
}

std::string ReadRequestFromPipe(void* pipeHandle) {
    HANDLE pipe = static_cast<HANDLE>(pipeHandle);
    std::string request;
    char buffer[256];
    for (;;) {
        DWORD bytesRead = 0;
        if (!ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, NULL) || bytesRead == 0) {
            break; // client disconnected before sending a full line
        }
        request.append(buffer, bytesRead);
        // Stop at the first newline; that terminates the request line.
        if (request.find('\n') != std::string::npos) {
            break;
        }
    }
    return request;
}

} // namespace ipc
