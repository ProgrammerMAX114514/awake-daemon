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
# build.ps1 - configure and build awake in Release configuration.
#
# Invoked by utils\build.bat (which is only a launcher for pwsh). The
# executables always end up in build\Release\, no matter which CMake
# generator was used:
#
#   1. "Visual Studio 18 2026" - the normal path, also writes IDE files.
#   2. "NMake Makefiles" into build\nmake - used when the Visual Studio
#      generator cannot run (for example when MSBuild is blocked by a
#      security policy).
# =============================================================================

param(
    [string]$BuildDir = '',
    [switch]$NoFallback
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

Write-Host '[build] Setting up the MSVC x64 environment...'
. (Join-Path $PSScriptRoot 'msvc-env.ps1')

$cmake = 'F:\Program Files\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = 'cmake' }

Push-Location $root
try {
    $vsFailed = $false

    Write-Host '[build] Configuring with CMake (Visual Studio generator)...'
    & $cmake -S . -B build -G 'Visual Studio 18 2026' -A x64
    if ($LASTEXITCODE -ne 0) {
        $vsFailed = $true
        if ($NoFallback) {
            throw 'CMake configuration failed with the Visual Studio generator.'
        }
        Write-Host '[build] Visual Studio generator unavailable, falling back to NMake...'
        if (Test-Path 'build\nmake\CMakeCache.txt') {
            Remove-Item -Recurse -Force 'build\nmake'
        }
        & $cmake -S . -B build\nmake -G 'NMake Makefiles' -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }

        Write-Host '[build] Building (Release, NMake)...'
        & $cmake --build build\nmake
        if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

        if (-not (Test-Path 'build\Release')) {
            New-Item -ItemType Directory -Path 'build\Release' | Out-Null
        }
        Copy-Item 'build\nmake\awake.exe' 'build\Release\' -Force
        Copy-Item 'build\nmake\awake.daemon.exe' 'build\Release\' -Force
    } else {
        Write-Host '[build] Building (Release)...'
        & $cmake --build build --config Release
        if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    }

    Write-Host '[build] Verifying the executables...'
    foreach ($exe in @('build\Release\awake.exe', 'build\Release\awake.daemon.exe')) {
        if (-not (Test-Path $exe)) { throw "$exe is missing." }
    }
    & 'build\Release\awake.exe' version

    Write-Host '[build] Done. Executables are in build\Release\:'
    Get-ChildItem 'build\Release\awake.exe', 'build\Release\awake.daemon.exe' |
        ForEach-Object { Write-Host "  $($_.Name)  $($_.Length) bytes" }
} catch {
    Write-Host "[build] ERROR: $($_.Exception.Message)"
    exit 1
} finally {
    Pop-Location
}

exit 0
