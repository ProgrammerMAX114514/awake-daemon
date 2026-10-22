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
# Dot-source this file to set INCLUDE/LIB/PATH for the current session.
# The Visual Studio instance and the Windows SDK come from utils\toolchain.ps1
# (configuration files first, then detection, then an interactive question).
#
# Two strategies are used:
#   1. Preferred: call vcvarsall.bat of the instance and import the
#      environment it prints.
#   2. Fallback: assemble INCLUDE/LIB/PATH from the MSVC toolset and the SDK
#      directories without touching the registry. Some environments block the
#      programs vcvarsall relies on (for example reg.exe), which leaves
#      cl.exe unusable even though the compiler is installed.
# =============================================================================

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'toolchain.ps1')

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

function Get-NewestSdkVersion {
    param([string]$KitsRoot)
    $versions = Get-ChildItem -Path (Join-Path $KitsRoot 'Include') -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName 'um') } |
        Sort-Object Name
    if (-not $versions) { return $null }
    return $versions[-1].Name
}

function Get-ToolsetName {
    param([string]$VisualStudioRoot)
    # An explicit version wins, otherwise the newest toolset is used.
    $configured = Get-ToolSetting -Section 'msvc' -Key 'version'
    if ($configured) { return $configured }
    $toolsRoot = Join-Path $VisualStudioRoot 'VC\Tools\MSVC'
    $toolsets = Get-ChildItem -Path $toolsRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name
    if (-not $toolsets) { return $null }
    return $toolsets[-1].Name
}

function Enable-ManualMsvcEnvironment {
    param([string]$VisualStudioRoot)

    $toolsetName = Get-ToolsetName -VisualStudioRoot $VisualStudioRoot
    if (-not $toolsetName) { return $false }
    $toolsetRoot = Join-Path $VisualStudioRoot "VC\Tools\MSVC\$toolsetName"
    if (-not (Test-Path $toolsetRoot)) { return $false }

    $kitsRoot = Get-WindowsSdkRoot
    $sdkVersion = Get-ToolSetting -Section 'windows-sdk' -Key 'version'
    if (-not $sdkVersion) { $sdkVersion = Get-NewestSdkVersion -KitsRoot $kitsRoot }
    if (-not $sdkVersion) { throw "No SDK headers under '$kitsRoot\Include'." }

    $msvcInclude = Join-Path $toolsetRoot 'include'
    $msvcLib = Join-Path $toolsetRoot 'lib\x64'
    $msvcBin = Join-Path $toolsetRoot 'bin\Hostx64\x64'
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
    $env:VCToolsVersion = $toolsetName

    Write-Host "[msvc-env] Environment ready (manual): MSVC $toolsetName, SDK $sdkVersion."
    return (Test-ToolchainReady)
}

# ---------------------------------------------------------------------------
# Entry point of the dot-sourced script
# ---------------------------------------------------------------------------

if (Test-ToolchainReady) {
    Write-Host '[msvc-env] A complete MSVC environment is already available, keeping it.'
    return
}

$vsRoot = Get-VisualStudioRoot
Write-Host "[msvc-env] Visual Studio: $vsRoot"

$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvarsall.bat'
if (Test-Path $vcvars) {
    Write-Host "[msvc-env] Using '$vcvars'..."
    Import-VcVarsEnvironment -VcVarsPath $vcvars
    if (Test-ToolchainReady) {
        Write-Host '[msvc-env] Environment ready (vcvarsall).'
        return
    }
    Write-Host '[msvc-env] vcvarsall left an incomplete environment, using the manual fallback.'
}

if (-not (Enable-ManualMsvcEnvironment -VisualStudioRoot $vsRoot)) {
    throw 'cl.exe is still not available after the manual environment setup. Check the Visual Studio and Windows SDK settings in utils\toolchain.ini.'
}
