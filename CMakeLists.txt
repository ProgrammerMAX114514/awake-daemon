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
project(awake VERSION 0.6.0 LANGUAGES CXX)

# The version number lives in exactly one place: the project() call above.
# version.h is generated from common/include/version.h.in during configuration,
# so the client, the daemon and any future target always report the same
# version without anybody having to keep two files in sync.
#   Stable release:  set AWAKE_VERSION_SUFFIX to ""
#   Pre-release:     "-beta", "-rc1", ...
set(AWAKE_VERSION_SUFFIX "-beta")
set(AWAKE_VERSION_FULL "${PROJECT_VERSION}${AWAKE_VERSION_SUFFIX}")

# The commit hash is shown in the version banner (F20). Reconfiguring happens
# on every build (utils/build.ps1 always runs the configure step), so the hash
# follows new commits without a manual clean. Source exports and CI archives
# without a .git directory fall back to "unknown".
set(AWAKE_GIT_COMMIT "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE AWAKE_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE AWAKE_GIT_RESULT)
    if(AWAKE_GIT_RESULT EQUAL 0 AND AWAKE_GIT_HASH)
        set(AWAKE_GIT_COMMIT "${AWAKE_GIT_HASH}")
        # "-dirty" marks a binary built from uncommitted changes.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE AWAKE_GIT_DIRTY)
        if(NOT AWAKE_GIT_DIRTY EQUAL 0)
            set(AWAKE_GIT_COMMIT "${AWAKE_GIT_COMMIT}-dirty")
        endif()
    endif()
endif()

if(AWAKE_GIT_COMMIT STREQUAL "unknown")
    set(AWAKE_VERSION_DISPLAY "${AWAKE_VERSION_FULL}")
else()
    set(AWAKE_VERSION_DISPLAY "${AWAKE_VERSION_FULL} (git ${AWAKE_GIT_COMMIT})")
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/include/version.h
    @ONLY
)

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
    common/src/license.cpp
    common/src/privilege.cpp
    common/src/process.cpp
    common/src/protocol.cpp
    common/src/timeparse.cpp
)
# The generated directory comes first so that "version.h" always resolves to
# the configured copy.
target_include_directories(awake_common PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/generated/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)
# advapi32 provides GetTokenInformation (elevation check); the registry
# functions used by the autostart code live in the same library.
target_link_libraries(awake_common PUBLIC advapi32)

# -----------------------------------------------------------------------------
# Client: awake.exe
# -----------------------------------------------------------------------------
add_executable(awake
    main/src/main.cpp
    main/src/commands.cpp
    main/src/status.cpp
    main/src/autostart.cpp
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

# -----------------------------------------------------------------------------
# Unit tests (R7). They link the same static library the executables use, so a
# behaviour change in common/ shows up here before it reaches a binary.
# AWAKE_BUILD_TESTS=OFF skips them (useful for packaging-only builds).
# -----------------------------------------------------------------------------
option(AWAKE_BUILD_TESTS "Build the unit tests" ON)
if(AWAKE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
