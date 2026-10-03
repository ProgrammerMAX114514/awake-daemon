# Copyright 2026 ProgrammerMAX114514
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# AI-GENERATED SOFTWARE: this file was generated with AI assistance.
# Please review it carefully before use. The author is not liable for
# any consequences arising from the use of this software.

# =============================================================================
# awake - keep-awake command line tool for Windows
#
# Defines two build targets:
#   awake          - the client command line tool (awake.exe)
#   awake.daemon   - the background daemon (awake.daemon.exe)
#
# Both targets link against the shared static library "awake_common",
# which provides console coloring, configuration file handling, named
# pipe IPC and process enumeration helpers.
# =============================================================================
cmake_minimum_required(VERSION 3.15)
project(awake VERSION 0.2.0 LANGUAGES CXX)

# Use C++17 for both targets.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statically link the C runtime (instead of the DLL runtime) so that the
# produced executables are self-contained and can be copied anywhere.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Enable a high warning level on MSVC.
if(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# -----------------------------------------------------------------------------
# Shared static library used by both the client and the daemon.
# -----------------------------------------------------------------------------
add_library(awake_common STATIC
    common/src/console.cpp
    common/src/config.cpp
    common/src/ipc.cpp
    common/src/process.cpp
    common/src/protocol.cpp
)
target_include_directories(awake_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)

# -----------------------------------------------------------------------------
# Client: awake.exe
# -----------------------------------------------------------------------------
add_executable(awake
    main/src/main.cpp
    main/src/commands.cpp
    main/src/status.cpp
)
target_include_directories(awake PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/main/include)
target_link_libraries(awake PRIVATE awake_common)

# -----------------------------------------------------------------------------
# Daemon: awake.daemon.exe
# -----------------------------------------------------------------------------
add_executable(awake.daemon
    daemon/src/main.cpp
    daemon/src/state.cpp
    daemon/src/power.cpp
    daemon/src/server.cpp
)
target_include_directories(awake.daemon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/daemon/include)
target_link_libraries(awake.daemon PRIVATE awake_common)
