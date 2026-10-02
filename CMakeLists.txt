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
project(awake CXX)

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
)
target_include_directories(awake_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)

# -----------------------------------------------------------------------------
# Client: awake.exe
# -----------------------------------------------------------------------------
add_executable(awake main/src/main.cpp)
target_link_libraries(awake PRIVATE awake_common)

# -----------------------------------------------------------------------------
# Daemon: awake.daemon.exe
# -----------------------------------------------------------------------------
add_executable(awake.daemon daemon/src/daemon.cpp)
target_link_libraries(awake.daemon PRIVATE awake_common)
