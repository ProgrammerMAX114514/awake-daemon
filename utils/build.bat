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
rem awake - build launcher (lives in utils\)
rem
rem Only starts the PowerShell 7 implementation (utils\build.ps1), which does
rem the actual work: toolchain settings (utils\toolchain.ps1), MSVC
rem environment (utils\msvc-env.ps1), CMake configuration and the Release
rem build. The launcher keeps the documented entry point "utils\build.bat".
rem
rem The PowerShell executable is taken from, in this order:
rem   1. the AWAKE_PWSH environment variable
rem   2. pwsh.exe on PATH
rem   3. Program Files\PowerShell\7 on every drive
rem   (the [powershell] entry of utils\toolchain.ini is honoured by the
rem   PowerShell scripts themselves)
rem
rem Extra arguments are forwarded to build.ps1 (for example -NoFallback).
rem The script can be invoked from any working directory.
rem =============================================================================

setlocal

call :find_pwsh
if not defined PWSH (
    echo [build] ERROR: PowerShell 7 was not found.
    echo [build] Install PowerShell 7 or set AWAKE_PWSH to the pwsh.exe path.
    exit /b 1
)

"%PWSH%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
exit /b %errorlevel%

:find_pwsh
set "PWSH="
if defined AWAKE_PWSH if exist "%AWAKE_PWSH%" set "PWSH=%AWAKE_PWSH%"
if defined PWSH exit /b 0
where pwsh.exe >nul 2>&1
if not errorlevel 1 (
    for /f "delims=" %%P in ('where pwsh.exe') do if not defined PWSH set "PWSH=%%P"
)
if defined PWSH exit /b 0
for %%D in (C D E F G H) do (
    if not defined PWSH if exist "%%D:\Program Files\PowerShell\7\pwsh.exe" set "PWSH=%%D:\Program Files\PowerShell\7\pwsh.exe"
)
exit /b 0
