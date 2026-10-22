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
rem awake - release launcher (lives in utils\)
rem
rem Only starts the PowerShell 7 implementation (utils\pubrelease.ps1), which
rem builds the project, creates the GPG-signed tag and packages the zip.
rem
rem Usage: utils\pubrelease.bat <tag>
rem The tag is NOT pushed by this script.
rem =============================================================================

setlocal

if "%~1"=="" (
    echo [pubrelease] ERROR: missing tag argument. Usage: utils\pubrelease.bat ^<tag^>
    exit /b 1
)

set "PWSH="
where pwsh.exe >nul 2>&1
if not errorlevel 1 set "PWSH=pwsh.exe"
if not defined PWSH if exist "%ProgramFiles%\PowerShell\7\pwsh.exe" set "PWSH=%ProgramFiles%\PowerShell\7\pwsh.exe"
if not defined PWSH if exist "%ProgramW6432%\PowerShell\7\pwsh.exe" set "PWSH=%ProgramW6432%\PowerShell\7\pwsh.exe"
if not defined PWSH if exist "%SystemDrive%\Program Files\PowerShell\7\pwsh.exe" set "PWSH=%SystemDrive%\Program Files\PowerShell\7\pwsh.exe"
if not defined PWSH (
    echo [pubrelease] ERROR: PowerShell 7 was not found.
    echo [pubrelease] Install PowerShell 7 ^(winget install Microsoft.PowerShell^), or run:
    echo [pubrelease]   pwsh -NoProfile -File "%~dp0pubrelease.ps1" ^<tag^>
    exit /b 1
)

"%PWSH%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0pubrelease.ps1" %*
exit /b %errorlevel%
