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
rem the actual work: MSVC environment setup (utils\msvc-env.ps1), CMake
rem configuration and the Release build. The launcher exists so that the
rem documented command stays "utils\build.bat" and so that the shell is not
rem involved in the batch quoting rules.
rem
rem Extra arguments are forwarded to build.ps1 (for example -NoFallback).
rem The script can be invoked from any working directory.
rem =============================================================================

setlocal

rem Locate PowerShell 7: PATH first, then the usual installation folders.
rem %ProgramFiles% can point to a non-system drive, so the system drive is
rem checked explicitly as well.
set "PWSH="
where pwsh.exe >nul 2>&1
if not errorlevel 1 set "PWSH=pwsh.exe"
if not defined PWSH if exist "%ProgramFiles%\PowerShell\7\pwsh.exe" set "PWSH=%ProgramFiles%\PowerShell\7\pwsh.exe"
if not defined PWSH if exist "%ProgramW6432%\PowerShell\7\pwsh.exe" set "PWSH=%ProgramW6432%\PowerShell\7\pwsh.exe"
if not defined PWSH if exist "%SystemDrive%\Program Files\PowerShell\7\pwsh.exe" set "PWSH=%SystemDrive%\Program Files\PowerShell\7\pwsh.exe"
if not defined PWSH (
    echo [build] ERROR: PowerShell 7 was not found.
    echo [build] Install PowerShell 7 ^(winget install Microsoft.PowerShell^), or run:
    echo [build]   pwsh -NoProfile -File "%~dp0build.ps1"
    exit /b 1
)

"%PWSH%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
exit /b %errorlevel%
