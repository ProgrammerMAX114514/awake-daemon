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
rem awake - release packaging script (lives in utils\)
rem
rem Usage: utils\pubrelease.bat <tag>
rem
rem For the given tag (for example 0.2.0-beta) this script:
rem   1. builds the project (Release) via utils\build.bat,
rem   2. creates the GPG-signed git tag <tag> ("git tag -s"),
rem   3. packages the executables plus LICENSE and NOTICE into
rem      build\Release\publish\tag-<tag>\awake-daemon.win64.zip using 7-Zip.
rem
rem The script can be invoked from any working directory; it always operates
rem on the repository root (the parent of utils\). The tag is NOT pushed -
rem push it manually with: git push origin <tag>
rem =============================================================================

setlocal

set "SEVENZIP=C:\Program Files\7-Zip\7z.exe"

rem --- Argument check -------------------------------------------------------
if "%~1"=="" (
    echo [pubrelease] ERROR: missing tag argument. Usage: utils\pubrelease.bat ^<tag^>
    exit /b 1
)
set "TAG=%~1"

rem --- Change to the repository root (parent of utils\) ---------------------
cd /d "%~dp0.."

rem --- Prerequisite checks --------------------------------------------------
if not exist "%SEVENZIP%" (
    echo [pubrelease] ERROR: 7-Zip not found at "%SEVENZIP%".
    exit /b 1
)
git rev-parse -q --verify "refs/tags/%TAG%" >nul 2>&1
if not errorlevel 1 (
    echo [pubrelease] ERROR: tag "%TAG%" already exists.
    exit /b 1
)

rem --- Build -----------------------------------------------------------------
echo [pubrelease] Building...
call "%~dp0build.bat"
if errorlevel 1 (
    echo [pubrelease] ERROR: build failed.
    exit /b 1
)

rem --- Tag (GPG-signed) -------------------------------------------------------
echo [pubrelease] Creating signed tag "%TAG%"...
git tag -s "%TAG%" -m "awake %TAG%"
if errorlevel 1 (
    echo [pubrelease] ERROR: failed to create tag "%TAG%".
    exit /b 1
)

rem --- Package ----------------------------------------------------------------
rem Stage the distributable files in a temporary folder so that the zip
rem archive contains them at its root (no build\Release\... path prefix).
set "PUB=build\Release\publish\tag-%TAG%"
set "STAGE=%PUB%\stage"
mkdir "%STAGE%" 2>nul
copy /y "build\Release\awake.exe"       "%STAGE%" >nul
copy /y "build\Release\awake.daemon.exe" "%STAGE%" >nul
copy /y "LICENSE" "%STAGE%" >nul
copy /y "NOTICE"  "%STAGE%" >nul

echo [pubrelease] Creating "%PUB%\awake-daemon.win64.zip"...
pushd "%STAGE%"
"%SEVENZIP%" a -tzip "..\awake-daemon.win64.zip" * -bso0
popd
if errorlevel 1 (
    echo [pubrelease] ERROR: zip packaging failed.
    popd
    exit /b 1
)
rd /s /q "%STAGE%"

echo [pubrelease] Done. Release artifacts:
dir /b "%PUB%"
echo [pubrelease] Remember to push the tag: git push origin %TAG%

endlocal
