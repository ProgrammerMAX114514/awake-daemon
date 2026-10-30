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
# Invoked by utils\build.bat (which is only a launcher for PowerShell 7).
# Toolchain paths come from utils\toolchain.ps1 (configuration files first,
# detection second, an interactive question as the last resort).
#
# The executables always end up in build\Release\, no matter which CMake
# generator was used:
#   1. the configured generator, or the Visual Studio generator matching the
#      installed Visual Studio;
#   2. "NMake Makefiles" into build\nmake when that generator cannot run
#      (MSBuild is blocked by a security policy, or the environment holds
#      duplicated proxy variables that make MSBuild fail to start cl.exe).
# =============================================================================

param(
    [switch]$NoFallback,
    [switch]$NonInteractive
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

if ($NonInteractive) { $env:CI = '1' }

# MSBuild assembles the child process environment in a case-sensitive
# dictionary, so the same variable stored under two spellings - typically
# "HTTPS_PROXY" next to "https_proxy", which some tools add - makes it
# throw "System.ArgumentException: An item with the same key has already
# been added" while starting cl.exe. The compiler is then never found and
# the Visual Studio generator cannot be used at all. Windows itself treats
# both spellings as one variable, so the duplicate is invisible from
# "set HTTPS_PROXY" or $env:HTTPS_PROXY and easy to miss. Look for it up
# front and name the cause, instead of failing once and switching
# generators without saying why.
function Get-DuplicatedProxyNames {
    $names = [Environment]::GetEnvironmentVariables().Keys |
        Where-Object { $_ -match '(?i)^(https?|no|all)_proxy$' }
    $result = @()
    foreach ($name in $names) {
        if ((@($names | Where-Object { $_ -ieq $name })).Count -le 1) { continue }
        $canonical = $name.ToUpperInvariant()
        if ($result -notcontains $canonical) { $result += $canonical }
    }
    return $result
}

$duplicatedProxy = Get-DuplicatedProxyNames
if ($duplicatedProxy.Count -gt 0) {
    Write-Host ('[build] WARNING: duplicated proxy variables: ' + ($duplicatedProxy -join ', '))
    Write-Host '[build]   Each of these exists in more than one spelling (for example'
    Write-Host '[build]   HTTPS_PROXY and https_proxy). MSBuild refuses to start cl.exe in'
    Write-Host '[build]   that environment, so the Visual Studio generator cannot be used.'
    Write-Host '[build]   Remove one spelling from the user environment variables to fix it.'
}

Write-Host '[build] Setting up the MSVC x64 environment...'
. (Join-Path $PSScriptRoot 'msvc-env.ps1')

$cmake = Get-CMakeCommand
$vsRoot = Get-VisualStudioRoot
$generator = Get-CMakeGenerator -VisualStudioRoot $vsRoot
$platform = Get-ToolSetting -Section 'generator' -Key 'platform'
if (-not $platform) { $platform = 'x64' }

Write-Host "[build] CMake    : $cmake"
Write-Host "[build] Generator: $generator"

Push-Location $root
try {
    $useNMake = ($generator -eq 'NMake Makefiles')
    if (-not $useNMake -and $duplicatedProxy.Count -gt 0) {
        if ($NoFallback) {
            throw 'Duplicated proxy variables would stop MSBuild; remove one spelling or drop -NoFallback.'
        }
        Write-Host '[build] Using NMake because of the duplicated proxy variables above.'
        $useNMake = $true
    }

    if (-not $useNMake) {
        Write-Host '[build] Configuring with CMake...'
        & $cmake -S . -B build -G $generator -A $platform
        if ($LASTEXITCODE -ne 0) {
            $useNMake = $true
        } else {
            Write-Host '[build] Building (Release)...'
            & $cmake --build build --config Release
            if ($LASTEXITCODE -ne 0) {
                # MSBuild can fail even after a successful configuration: for
                # example when the inherited environment carries the same
                # variable in two spellings (HTTPS_PROXY / https_proxy), which
                # its process launcher rejects. NMake does not care.
                Write-Host '[build] MSBuild failed.'
                $useNMake = $true
            }
        }
        if ($useNMake) {
            if ($NoFallback) {
                throw "Building with the '$generator' generator failed."
            }
            Write-Host '[build] Falling back to the NMake generator...'
        }
    }

    if ($useNMake) {
        if (Test-Path 'build\nmake\CMakeCache.txt') {
            Remove-Item -Recurse -Force 'build\nmake'
        }
        Write-Host '[build] Configuring with CMake (NMake)...'
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
