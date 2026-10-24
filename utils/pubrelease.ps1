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
# pubrelease.ps1 - build, tag and package a release.
#
# Invoked by utils\pubrelease.bat (a launcher for PowerShell 7) or directly:
#
#   utils\pubrelease.bat 0.6.0-beta
#
# Steps: build (utils\build.ps1), create the GPG-signed tag, package the two
# executables plus LICENSE and NOTICE into
# build\Release\publish\tag-<tag>\awake-daemon.win64.zip.
# The tag is NOT pushed - push it with: git push origin <tag>
# Toolchain paths come from utils\toolchain.ps1.
# =============================================================================

param(
    [Parameter(Position = 0)]
    [string]$Tag = '',
    [switch]$NonInteractive
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

if ($NonInteractive) { $env:CI = '1' }

. (Join-Path $PSScriptRoot 'toolchain.ps1')

if ([string]::IsNullOrWhiteSpace($Tag)) {
    Write-Host '[pubrelease] ERROR: missing tag argument. Usage: utils\pubrelease.bat <tag>'
    exit 1
}

$sevenZip = Get-SevenZipPath
$git = Get-GitCommand

Write-Host "[pubrelease] 7-Zip: $sevenZip"
Write-Host "[pubrelease] Git  : $git"

Push-Location $root
try {
    & $git rev-parse -q --verify "refs/tags/$Tag" | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[pubrelease] ERROR: tag '$Tag' already exists."
        exit 1
    }

    Write-Host '[pubrelease] Building...'
    & (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'build failed.' }

    Write-Host "[pubrelease] Creating signed tag '$Tag'..."
    & $git tag -s $Tag -m "awake $Tag"
    if ($LASTEXITCODE -ne 0) { throw "failed to create tag '$Tag'." }

    $publishRoot = Join-Path $root "build\Release\publish\tag-$Tag"
    $stage = Join-Path $publishRoot 'stage'
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Path $stage -Force | Out-Null

    foreach ($file in @('awake.exe', 'awake.daemon.exe')) {
        Copy-Item (Join-Path $root "build\Release\$file") $stage -Force
    }
    foreach ($file in @('LICENSE', 'NOTICE', 'README.md', 'README.zh.md')) {
        Copy-Item (Join-Path $root $file) $stage -Force
    }

    $zipPath = Join-Path $publishRoot 'awake-daemon.win64.zip'
    if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
    Write-Host "[pubrelease] Creating '$zipPath'..."
    Push-Location $stage
    try {
        & $sevenZip a -tzip $zipPath '*' -bso0
        if ($LASTEXITCODE -ne 0) { throw 'zip packaging failed.' }
    } finally {
        Pop-Location
    }
    Remove-Item -Recurse -Force $stage

    Write-Host '[pubrelease] Done. Release artifacts:'
    Get-ChildItem $publishRoot | ForEach-Object { Write-Host "  $($_.Name)" }
    Write-Host '[pubrelease] Remaining manual step: create the GitHub Release for the tag'
    Write-Host '             and upload the zip plus ReleaseNotes.md.'
    Write-Host "[pubrelease] Remember to push the tag: git push origin $Tag"
} catch {
    Write-Host "[pubrelease] ERROR: $($_.Exception.Message)"
    exit 1
} finally {
    Pop-Location
}

exit 0
