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
# toolchain.ps1 - toolchain configuration and discovery.
#
# Dot-source this file to get:
#   $ToolchainSettings   merged view of every configuration file that was read
#   $ToolchainSources    the files that were read, highest priority first
#   Get-ToolSetting      read one "section.key" value
#   Save-ToolSetting     write one value back, preserving all other lines
#   Find-Tool            locate a tool (configured path first, then PATH and
#                        the usual installation folders)
#   Request-ToolPath     interactive fallback: ask the user and remember it
#   Get-CMakeCommand / Get-GitCommand / Get-SevenZipPath / Get-PwshPath
#   Get-VisualStudioRoot / Get-WindowsSdkRoot
#
# Configuration files, from highest to lowest priority:
#   1. the file pointed to by $env:AWAKE_TOOLCHAIN
#   2. <repo>\utils\toolchain.ini      (local, not committed)
#   3. %APPDATA%\awake\toolchain.ini   (per user, shared by all clones)
#   4. automatic discovery (no file needed)
#
# Relative paths inside the configuration are resolved against the repository
# root, so "C:\..." style absolute paths and "..\tools\cmake.exe" both work.
# =============================================================================

$script:RepoRoot = Split-Path $PSScriptRoot -Parent
$script:ToolchainSettings = @{}
$script:ToolchainSources = @()

function Get-ToolchainFilePaths {
    $paths = @()
    if ($env:AWAKE_TOOLCHAIN) { $paths += $env:AWAKE_TOOLCHAIN }
    $paths += (Join-Path $script:RepoRoot 'utils\toolchain.ini')
    if ($env:APPDATA) { $paths += (Join-Path $env:APPDATA 'awake\toolchain.ini') }
    return $paths
}

function Read-IniFile {
    param([string]$Path)

    $data = @{}
    $section = ''
    foreach ($line in (Get-Content -LiteralPath $Path -Encoding utf8 -ErrorAction Stop)) {
        $trim = $line.Trim()
        if (-not $trim -or $trim.StartsWith('#') -or $trim.StartsWith(';')) { continue }
        if ($trim -match '^\[(.+)\]$') {
            $section = $matches[1].Trim().ToLower()
            if (-not $data.ContainsKey($section)) { $data[$section] = @{} }
            continue
        }
        $equals = $trim.IndexOf('=')
        if ($equals -lt 1 -or -not $section) { continue }
        $key = $trim.Substring(0, $equals).Trim().ToLower()
        $value = $trim.Substring($equals + 1).Trim()
        if ($value) { $data[$section][$key] = $value }
    }
    return $data
}

function Initialize-ToolchainSettings {
    # Lowest priority first, so that a higher priority file overwrites it.
    $files = Get-ToolchainFilePaths
    for ($index = $files.Count - 1; $index -ge 0; $index--) {
        $path = $files[$index]
        if (-not (Test-Path -LiteralPath $path)) { continue }
        $data = Read-IniFile -Path $path
        foreach ($section in $data.Keys) {
            if (-not $script:ToolchainSettings.ContainsKey($section)) {
                $script:ToolchainSettings[$section] = @{}
            }
            foreach ($key in $data[$section].Keys) {
                $script:ToolchainSettings[$section][$key] = $data[$section][$key]
            }
        }
        $script:ToolchainSources = @($path) + $script:ToolchainSources
    }
    return $script:ToolchainSources
}

function Get-ToolSetting {
    param([string]$Section, [string]$Key)

    $sectionKey = $Section.ToLower()
    $keyName = $Key.ToLower()
    if (-not $script:ToolchainSettings.ContainsKey($sectionKey)) { return $null }
    if (-not $script:ToolchainSettings[$sectionKey].ContainsKey($keyName)) { return $null }
    $value = $script:ToolchainSettings[$sectionKey][$keyName]
    if (-not $value) { return $null }
    # Path-like values may be relative to the repository root; plain names
    # (a command on PATH, a CMake generator) are returned untouched.
    if ($value -match '[\\/]' -and -not [System.IO.Path]::IsPathRooted($value)) {
        return (Join-Path $script:RepoRoot $value)
    }
    return $value
}

function Get-ToolchainWriteTarget {
    # Write to the highest priority file that already exists; otherwise create
    # the per-user file so that a repository checkout stays clean.
    foreach ($path in (Get-ToolchainFilePaths)) {
        if (Test-Path -LiteralPath $path) { return $path }
    }
    $userFile = if ($env:APPDATA) { Join-Path $env:APPDATA 'awake\toolchain.ini' } else { $null }
    if (-not $userFile) {
        throw 'Neither AWAKE_TOOLCHAIN nor APPDATA is available; cannot store the toolchain settings.'
    }
    $folder = Split-Path $userFile -Parent
    if (-not (Test-Path -LiteralPath $folder)) {
        New-Item -ItemType Directory -Path $folder -Force | Out-Null
    }
    Set-Content -LiteralPath $userFile -Encoding utf8 -Value @(
        '# awake build toolchain configuration (created by the build scripts)',
        '# Delete an entry to fall back to automatic detection.',
        ''
    )
    return $userFile
}

function Save-ToolSetting {
    param([string]$Section, [string]$Key, [string]$Value)

    $path = Get-ToolchainWriteTarget
    $lines = @(Get-Content -LiteralPath $path -Encoding utf8 -ErrorAction SilentlyContinue)
    $sectionHeader = "[$Section]"
    $sectionIndex = -1
    for ($index = 0; $index -lt $lines.Count; $index++) {
        if ($lines[$index].Trim().ToLower() -eq $sectionHeader.ToLower()) { $sectionIndex = $index; break }
    }

    if ($sectionIndex -lt 0) {
        $lines += ''
        $lines += $sectionHeader
        $lines += "$Key = $Value"
    } else {
        $keyIndex = -1
        $insertAt = $sectionIndex + 1
        for ($index = $sectionIndex + 1; $index -lt $lines.Count; $index++) {
            $trim = $lines[$index].Trim()
            if ($trim -match '^\[') { break }
            $insertAt = $index + 1
            if ($trim -match '^([^=]+)=' -and $matches[1].Trim().ToLower() -eq $Key.ToLower()) {
                $keyIndex = $index
                break
            }
        }
        if ($keyIndex -ge 0) {
            $lines[$keyIndex] = "$Key = $Value"
        } else {
            # Insert the key at the end of the section. Note the explicit
            # empty-slice handling: a descending range such as $lines[5..4]
            # would repeat lines instead of producing nothing.
            $head = if ($insertAt -gt 0) { @($lines[0..($insertAt - 1)]) } else { @() }
            $tail = if ($insertAt -lt $lines.Count) { @($lines[$insertAt..($lines.Count - 1)]) } else { @() }
            $lines = @($head) + @("$Key = $Value") + @($tail)
        }
    }

    Set-Content -LiteralPath $path -Encoding utf8 -Value $lines
    # Refresh the merged view so the value is visible immediately.
    Initialize-ToolchainSettings | Out-Null
    Write-Host "[toolchain] Saved '$Section.$Key' to $path"
}

function Test-InteractiveHost {
    if ($env:CI) { return $false }
    if ([Console]::IsInputRedirected) { return $false }
    return [Environment]::UserInteractive
}

function Request-ToolPath {
    param(
        [string]$Name,
        [string]$Section,
        [string]$Key = 'path',
        [string]$Hint,
        [scriptblock]$Validator
    )

    $detail = if ($Hint) { " ($Hint)" } else { '' }
    if (-not (Test-InteractiveHost)) {
        throw "$Name was not found and the build is running non-interactively. Set '$Section.$Key' in utils\toolchain.ini$detail or point AWAKE_TOOLCHAIN at a configuration file."
    }

    Write-Host "[toolchain] $Name was not found$detail."
    while ($true) {
        $answer = Read-Host "Enter the full path (leave empty to abort)"
        if ([string]::IsNullOrWhiteSpace($answer)) {
            throw "$Name is required; the build was aborted."
        }
        $answer = $answer.Trim().Trim('"')
        if (-not [System.IO.Path]::IsPathRooted($answer)) {
            $answer = Join-Path $script:RepoRoot $answer
        }
        if (-not (Test-Path -LiteralPath $answer)) {
            Write-Host "[toolchain] '$answer' does not exist."
            continue
        }
        if ($Validator -and -not (& $Validator $answer)) {
            Write-Host "[toolchain] '$answer' does not look like $Name."
            continue
        }
        Save-ToolSetting -Section $Section -Key $Key -Value $answer
        return $answer
    }
    return $null
}

function Test-Executable {
    param([string]$Path)
    return (Test-Path -LiteralPath $Path -PathType Leaf)
}

function Find-Tool {
    param(
        [string]$Name,
        [string]$Section,
        [string]$Key = 'path',
        [string[]]$Patterns = @(),
        [string]$Hint,
        [scriptblock]$Validator
    )

    $configured = Get-ToolSetting -Section $Section -Key $Key
    if ($configured) {
        if (Test-Executable $configured) { return $configured }
        # A bare name (no path separator) is looked up on PATH, so entries like
        # "git" or "cmake" keep working.
        if ($configured -notmatch '[\\/]') {
            $named = Get-Command $configured -ErrorAction SilentlyContinue
            if ($named) { return $named.Source }
        }
        Write-Host "[toolchain] WARNING: '$Section.$Key' points to '$configured', which does not exist. Falling back to detection."
    }

    # PATH first, then the usual installation folders on every drive.
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $roots = @()
    foreach ($drive in (Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue)) {
        $roots += $drive.Root
    }
    foreach ($pattern in $Patterns) {
        foreach ($root in $roots) {
            $candidates = @(Get-ChildItem -Path (Join-Path $root $pattern) -ErrorAction SilentlyContinue)
            if ($candidates.Count -gt 0) { return $candidates[0].FullName }
        }
    }

    return (Request-ToolPath -Name $Name -Section $Section -Key $Key -Hint $Hint -Validator $Validator)
}

# -----------------------------------------------------------------------------
# Concrete tool lookups
# -----------------------------------------------------------------------------

function Get-CMakeCommand {
    $validator = { param($path) return (Split-Path $path -Leaf) -match '^cmake(\.exe)?$' }
    return (Find-Tool -Name 'cmake' -Section 'cmake' -Patterns @(
        'Program Files\CMake\bin\cmake.exe',
        'Program Files (x86)\CMake\bin\cmake.exe'
    ) -Hint 'for example C:\Program Files\CMake\bin\cmake.exe' -Validator $validator)
}

function Get-GitCommand {
    $validator = { param($path) return (Split-Path $path -Leaf) -match '^git(\.exe)?$' }
    return (Find-Tool -Name 'git' -Section 'git' -Patterns @(
        'Program Files\Git\bin\git.exe',
        'Program Files (x86)\Git\bin\git.exe'
    ) -Hint 'for example C:\Program Files\Git\bin\git.exe' -Validator $validator)
}

function Get-SevenZipPath {
    $validator = { param($path) return (Split-Path $path -Leaf) -match '^7z(a|r)?(\.exe)?$' }
    return (Find-Tool -Name '7z' -Section 'sevenzip' -Patterns @(
        'Program Files\7-Zip\7z.exe',
        'Program Files (x86)\7-Zip\7z.exe'
    ) -Hint 'for example C:\Program Files\7-Zip\7z.exe' -Validator $validator)
}

function Get-PwshPath {
    $validator = { param($path) return (Split-Path $path -Leaf) -match '^pwsh(\.exe)?$' }
    return (Find-Tool -Name 'pwsh' -Section 'powershell' -Patterns @(
        'Program Files\PowerShell\7\pwsh.exe',
        'Program Files\PowerShell\6\pwsh.exe'
    ) -Hint 'for example C:\Program Files\PowerShell\7\pwsh.exe' -Validator $validator)
}

function Get-VisualStudioRoot {
    # Returns the Visual Studio instance folder (the one containing the VC
    # directory), either from the configuration or from detection.
    $configured = Get-ToolSetting -Section 'visual-studio' -Key 'path'
    if ($configured) {
        if (Test-Path (Join-Path $configured 'VC\Auxiliary\Build\vcvarsall.bat')) { return $configured }
        if (Test-Path (Join-Path $configured 'VC\Tools\MSVC')) { return $configured }
        Write-Host "[toolchain] WARNING: 'visual-studio.path' points to '$configured', which has no VC tools. Falling back to detection."
    }

    $instances = @()
    foreach ($drive in (Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue)) {
        foreach ($programFiles in @('Program Files', 'Program Files (x86)')) {
            $root = Join-Path $drive.Root (Join-Path $programFiles 'Microsoft Visual Studio')
            if (-not (Test-Path $root)) { continue }
            $instances += Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
                ForEach-Object { Get-ChildItem -Path $_.FullName -Directory -ErrorAction SilentlyContinue } |
                Where-Object { Test-Path (Join-Path $_.FullName 'VC\Tools\MSVC') }
        }
    }
    if ($instances.Count -gt 0) {
        return ($instances | Sort-Object Name | Select-Object -Last 1).FullName
    }

    $validator = { param($path)
        return ((Test-Path (Join-Path $path 'VC\Auxiliary\Build\vcvarsall.bat')) -or
                (Test-Path (Join-Path $path 'VC\Tools\MSVC')))
    }
    return (Request-ToolPath -Name 'Visual Studio (C++ tools)' -Section 'visual-studio' `
        -Hint 'the folder that contains VC\Auxiliary\Build\vcvarsall.bat, for example C:\Program Files\Microsoft Visual Studio\18\Community' `
        -Validator $validator)
}

function Get-WindowsSdkRoot {
    $configured = Get-ToolSetting -Section 'windows-sdk' -Key 'root'
    if ($configured) {
        if (Test-Path (Join-Path $configured 'Include')) { return $configured }
        Write-Host "[toolchain] WARNING: 'windows-sdk.root' points to '$configured', which has no Include folder. Falling back to detection."
    }

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

    $validator = { param($path) return (Test-Path (Join-Path $path 'Include')) }
    return (Request-ToolPath -Name 'the Windows SDK (Windows Kits\10)' -Section 'windows-sdk' -Key 'root' `
        -Hint 'the folder that contains Include, for example C:\Program Files (x86)\Windows Kits\10' `
        -Validator $validator)
}

function Get-CMakeGenerator {
    param([string]$VisualStudioRoot)

    # The configured generator wins; otherwise the year in the Visual Studio
    # instance folder ("...\Microsoft Visual Studio\18\Community") decides.
    # An unknown version falls back to NMake, which needs no IDE at all.
    $configured = Get-ToolSetting -Section 'generator' -Key 'name'
    if ($configured) { return $configured }

    if ($VisualStudioRoot -and ($VisualStudioRoot -match '\\(\d+)\\[^\\]+\\?$')) {
        switch ($matches[1]) {
            '18' { return 'Visual Studio 18 2026' }
            '17' { return 'Visual Studio 17 2022' }
            '16' { return 'Visual Studio 16 2019' }
        }
    }
    return 'NMake Makefiles'
}

# Load the configuration as soon as the module is dot-sourced.
Initialize-ToolchainSettings | Out-Null
