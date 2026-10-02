@echo off
rem Copyright 2026 ProgrammerMAX114514
rem
rem Licensed under the Apache License, Version 2.0 (the "License");
rem you may not use this file except in compliance with the License.
rem You may obtain a copy of the License at
rem
rem     http://www.apache.org/licenses/LICENSE-2.0
rem
rem Unless required by applicable law or agreed to in writing, software
rem distributed under the License is distributed on an "AS IS" BASIS,
rem WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
rem See the License for the specific language governing permissions and
rem limitations under the License.
rem
rem AI-GENERATED SOFTWARE: this file was generated with AI assistance.
rem Please review it carefully before use. The author is not liable for
rem any consequences arising from the use of this software.
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
