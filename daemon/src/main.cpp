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
#include "power.h"
#include "server.h"
#include "state.h"
#include "version.h"

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
    daemonstate::Init();
    HANDLE powerThread = power::StartPowerThread();
    if (powerThread == NULL) {
        console::PrintLine(console::kColorRed, "Error: failed to create the power thread.");
        daemonstate::Shutdown();
        return 1;
    }

    // Serve clients until SHUTDOWN arrives.
    server::RunPipeServer();

    // Stop the power thread (it resets the execution state on its way out)
    // and tear everything down.
    power::StopPowerThread(powerThread);
    daemonstate::Shutdown();

    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
