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
# msvc-env.ps1 - prepare an MSVC x64 build environment.
#
# Dot-source this file (". .\msvc-env.ps1") before invoking CMake. Two
# strategies are used:
#
#   1. Preferred: call vcvarsall.bat of the newest Visual Studio found under
#      "%ProgramFiles%\Microsoft Visual Studio" (or the x86 variant) and
#      import the environment it prints.
#   2. Fallback: assemble INCLUDE/LIB/PATH from the MSVC toolset and the
#      Windows SDK directories without touching the registry. Some
#      environments block the programs vcvarsall relies on (for example
#      reg.exe), which leaves cl.exe unusable even though the compiler is
#      installed.
#
# Optional environment variables:
#   AWAKE_SDK_ROOT - Windows Kits root when the SDK lives elsewhere than the
#                    default locations (for example F:\Windows Kits\10).
# =============================================================================

$ErrorActionPreference = 'Stop'

function Test-CommandAvailable {
    param([string]$Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Test-ToolchainReady {
    # cl.exe alone is not enough: linking also needs rc.exe (resources) and
    # the INCLUDE/LIB variables. A half-applied environment (for example a
    # vcvarsall run that could not finish because reg.exe is blocked) has
    # cl.exe but neither rc.exe nor INCLUDE.
    if (-not (Test-CommandAvailable 'cl.exe')) { return $false }
    if (-not (Test-CommandAvailable 'rc.exe')) { return $false }
    if ([string]::IsNullOrWhiteSpace($env:INCLUDE)) { return $false }
    return $true
}

function Import-VcVarsEnvironment {
    param([string]$VcVarsPath)

    # "cmd /c call vcvarsall.bat x64 && set" prints the resulting environment;
    # importing it is the supported way to inherit the setup from another
    # process.
    $lines = & cmd.exe /c "call `"$VcVarsPath`" x64 >nul 2>&1 && set"
    foreach ($line in $lines) {
        $index = $line.IndexOf('=')
        if ($index -gt 0) {
            $name = $line.Substring(0, $index)
            $value = $line.Substring($index + 1)
            Set-Item -Path "Env:$name" -Value $value -ErrorAction SilentlyContinue
        }
    }
}

function Get-VisualStudioRoots {
    # Visual Studio is not necessarily installed on the system drive, so every
    # fixed drive is probed. %ProgramFiles% alone is not enough (it can point
    # to C: while the IDE lives on another drive).
    $roots = @()
    foreach ($drive in (Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue)) {
        foreach ($programFiles in @('Program Files', 'Program Files (x86)')) {
            $candidate = Join-Path $drive.Root (Join-Path $programFiles 'Microsoft Visual Studio')
            if (Test-Path $candidate) { $roots += $candidate }
        }
    }
    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:ProgramW6432)) {
        if ([string]::IsNullOrWhiteSpace($base)) { continue }
        $candidate = Join-Path $base 'Microsoft Visual Studio'
        if ((Test-Path $candidate) -and ($roots -notcontains $candidate)) { $roots += $candidate }
    }
    return $roots
}

function Find-VcVarsPath {
    $candidates = @()
    foreach ($root in (Get-VisualStudioRoots)) {
        foreach ($edition in Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue) {
            foreach ($instance in Get-ChildItem -Path $edition.FullName -Directory -ErrorAction SilentlyContinue) {
                $vcvars = Join-Path $instance.FullName 'VC\Auxiliary\Build\vcvarsall.bat'
                if (Test-Path $vcvars) { $candidates += $vcvars }
            }
        }
    }
    if ($candidates.Count -eq 0) { return $null }
    # Newest Visual Studio (year first in the folder name) wins.
    return ($candidates | Sort-Object -Descending | Select-Object -First 1)
}

function Find-WindowsSdkRoot {
    $candidates = @($env:AWAKE_SDK_ROOT, $env:WINDOWSSDKDIR)
    foreach ($drive in (Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue)) {
        $candidates += (Join-Path $drive.Root 'Windows Kits\10')
        $candidates += (Join-Path $drive.Root 'Program Files (x86)\Windows Kits\10')
        $candidates += (Join-Path $drive.Root 'Program Files\Windows Kits\10')
    }
    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        if (Test-Path (Join-Path $candidate 'Include')) { return $candidate }
    }
    return $null
}

function Find-NewestSdkVersion {
    param([string]$KitsRoot)
    $versions = Get-ChildItem -Path (Join-Path $KitsRoot 'Include') -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName 'um') } |
        Sort-Object Name
    if (-not $versions) { return $null }
    return $versions[-1].Name
}

function Enable-ManualMsvcEnvironment {
    param([string]$VsInstanceRoot)

    $msvcRoot = Join-Path $VsInstanceRoot 'VC\Tools\MSVC'
    if (-not (Test-Path $msvcRoot)) { return $false }
    $toolset = Get-ChildItem -Path $msvcRoot -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name | Select-Object -Last 1
    if (-not $toolset) { return $false }

    $kitsRoot = Find-WindowsSdkRoot
    if (-not $kitsRoot) {
        throw 'No Windows SDK found. Set AWAKE_SDK_ROOT to the Windows Kits directory.'
    }
    $sdkVersion = Find-NewestSdkVersion -KitsRoot $kitsRoot
    if (-not $sdkVersion) {
        throw "No SDK headers under '$kitsRoot\Include'."
    }

    $msvcInclude = Join-Path $toolset.FullName 'include'
    $msvcLib = Join-Path $toolset.FullName 'lib\x64'
    $msvcBin = Join-Path $toolset.FullName 'bin\Hostx64\x64'
    $sdkInclude = Join-Path $kitsRoot "Include\$sdkVersion"
    $sdkLib = Join-Path $kitsRoot "Lib\$sdkVersion"
    $sdkBin = Join-Path $kitsRoot "bin\$sdkVersion\x64"

    $env:INCLUDE = @(
        $msvcInclude,
        (Join-Path $sdkInclude 'ucrt'),
        (Join-Path $sdkInclude 'shared'),
        (Join-Path $sdkInclude 'um'),
        (Join-Path $sdkInclude 'winrt'),
        (Join-Path $sdkInclude 'cppwinrt')
    ) -join ';'
    $env:LIB = @(
        $msvcLib,
        (Join-Path $sdkLib 'ucrt\x64'),
        (Join-Path $sdkLib 'um\x64')
    ) -join ';'
    $env:PATH = "$msvcBin;$sdkBin;$env:PATH"
    $env:VCToolsVersion = $toolset.Name

    Write-Host "[msvc-env] Environment ready (manual): MSVC $($toolset.Name), SDK $sdkVersion."
    return (Test-ToolchainReady)
}

# ---------------------------------------------------------------------------
# Entry point of the dot-sourced script
# ---------------------------------------------------------------------------

if (Test-ToolchainReady) {
    Write-Host '[msvc-env] A complete MSVC environment is already available, keeping it.'
    return
}

$vcvars = Find-VcVarsPath
$vsRoot = $null
if ($vcvars) {
    # ...\Build\vcvarsall.bat -> Build -> Auxiliary -> VC -> <Visual Studio instance>
    $vsRoot = (Get-Item $vcvars).Directory.Parent.Parent.Parent.FullName
    Write-Host "[msvc-env] Using '$vcvars'..."
    Import-VcVarsEnvironment -VcVarsPath $vcvars
    if (Test-ToolchainReady) {
        Write-Host '[msvc-env] Environment ready (vcvarsall).'
        return
    }
    Write-Host '[msvc-env] vcvarsall left an incomplete environment, using the manual fallback.'
} else {
    Write-Host '[msvc-env] No vcvarsall.bat found, using the manual fallback.'
    # Without vcvarsall, look for the toolset directly under the known roots.
    $instances = @()
    foreach ($root in (Get-VisualStudioRoots)) {
        $instances += Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { Get-ChildItem -Path $_.FullName -Directory -ErrorAction SilentlyContinue } |
            Where-Object { Test-Path (Join-Path $_.FullName 'VC\Tools\MSVC') }
    }
    if ($instances) {
        $vsRoot = ($instances | Sort-Object Name | Select-Object -Last 1).FullName
    }
}

if (-not $vsRoot) {
    throw 'No Visual Studio installation with the C++ tools was found.'
}
if (-not (Enable-ManualMsvcEnvironment -VsInstanceRoot $vsRoot)) {
    throw 'cl.exe is still not available after the manual environment setup.'
}
