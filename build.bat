@echo off
rem =============================================================================
rem awake - one-shot build script
rem
rem Sets up the MSVC x64 environment, then configures and builds the project
rem with CMake (Visual Studio 2026 generator, Release configuration).
rem The resulting executables are placed in build\Release\.
rem =============================================================================

setlocal

echo [build] Setting up MSVC x64 environment...
call "F:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 (
    echo [build] ERROR: failed to initialize the MSVC x64 environment.
    exit /b 1
)

echo [build] Configuring with CMake...
"F:\Program Files\CMake\bin\cmake.exe" -S . -B build -G "Visual Studio 18 2026" -A x64
if errorlevel 1 (
    echo [build] ERROR: CMake configuration failed.
    exit /b 1
)

echo [build] Building (Release)...
"F:\Program Files\CMake\bin\cmake.exe" --build build --config Release
if errorlevel 1 (
    echo [build] ERROR: build failed.
    exit /b 1
)

echo [build] Done. Executables are in build\Release\:
dir /b build\Release\awake.exe build\Release\awake.daemon.exe

endlocal
