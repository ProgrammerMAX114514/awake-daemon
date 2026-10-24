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
// main.cpp - awake.daemon.exe, the background daemon (entry point).
//
// Responsibilities of this file only: validate the launch, guarantee a
// single instance, initialize the shared state and bring the power thread
// and the pipe server up and down. The actual work lives in:
//
//   power.cpp  - holds the Windows execution state, scans the watch list
//   server.cpp - serves client requests on the named pipe
//   state.cpp  - the state shared between the two threads
//
// The daemon must not be started by hand: it refuses to run unless it was
// launched by the client with the hidden option "--internal-daemon <token>".
// =============================================================================

#include <windows.h>

#include <cstring>

#include "config.h"
#include "console.h"
#include "ipc.h"
#include "logging.h"
#include "privilege.h"
#include "monitor.h"
#include "power.h"
#include "privilege.h"
#include "server.h"
#include "state.h"
#include "version.h"

int main(int argc, char** argv) {
    console::EnableColors();

    // Warn when the daemon was started with administrator rights: such a
    // daemon cannot be stopped or contacted from a non-elevated shell, which
    // looks like "the process runs but does not respond". The warning is
    // informational only and never blocks the daemon.
    if (privilege::IsProcessElevated()) {
        console::PrintLine(console::kColorYellow,
                           "Warning: awake.daemon.exe is running elevated; a non-elevated shell cannot control it.");
    }

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

    // An elevated daemon is a footgun: its pipe carries a high integrity
    // label that a non-elevated client cannot write to, which would leave
    // the user with a daemon nobody can control (checklist item 2). All
    // features work without admin rights, so refuse to run elevated (option
    // A of the elevation matrix decision). Automation environments whose
    // runners are always elevated (GitHub Actions) set
    // AWAKE_ALLOW_ELEVATED=1 to opt out - with a loud warning.
    char allowElevated[8] = { 0 };
    const bool overrideActive =
        GetEnvironmentVariableA("AWAKE_ALLOW_ELEVATED", allowElevated,
                                 sizeof(allowElevated)) > 0 &&
        allowElevated[0] != '\0';
    if (privilege::IsProcessElevated()) {
        if (!overrideActive) {
            console::PrintErrLine(console::kColorRed,
                                  "Error: the daemon must not run elevated; start it from a non-elevated shell.");
            return 1;
        }
        console::PrintErrLine(console::kColorYellow,
                              "Warning: running elevated by AWAKE_ALLOW_ELEVATED override; a non-elevated client cannot control this daemon.");
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

    // Initialize the shared state, prepare logging and start the power thread.
    daemonstate::Init();
    daemonlog::ReloadFromConfig();
    daemonlog::Info(std::string("daemon started (") + version::kVersion + ")");
    HANDLE powerThread = power::StartPowerThread();
    if (powerThread == NULL) {
        console::PrintLine(console::kColorRed, "Error: failed to create the power thread.");
        daemonstate::Shutdown();
        return 1;
    }

    // Watch system power events (sleep/resume); on resume the power thread
    // re-applies the execution state immediately (F13).
    monitor::StartPowerMonitor();

    // Serve clients until SHUTDOWN arrives.
    server::RunPipeServer();

    // Stop the power monitor first so no resume event lands during teardown,
    // then stop the power thread (it resets the execution state on its way
    // out) and tear everything down.
    daemonlog::Info("daemon stopping");
    monitor::StopPowerMonitor();
    power::StopPowerThread(powerThread);
    daemonstate::Shutdown();

    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
