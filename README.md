# awake

[English](README.md) | [简体中文](README.zh.md)

`awake` is a Windows command line tool that keeps the system awake. It ships as two executables:

- **awake.exe** - the client command line tool.
- **awake.daemon.exe** - the background daemon. It is started by the client and cannot be launched directly.

## Features

- **Keep-awake** (`awake 1`): blocks *idle* sleep and hibernation only. Manual sleep via the power button, the Start menu, or sleep commands issued by other applications still works normally.
- **Screen keep-awake** (`awake screen on`): additionally keeps the screen on - blocks the display turning off, the screensaver and the lock screen caused by idle timeout.
- **Application watch list**: image names configured in `awake.ini`; while any of them is running, the daemon blocks idle sleep/hibernation the same way (checked every 30 seconds).
- **Colored console output** via ANSI escape sequences.
- The client starts the daemon automatically when needed (for `awake 0` / `awake 1`).

## Requirements

- Windows 10 or later
- Visual Studio 2026 (MSVC x64 toolset)
- CMake 4.x

The paths to the toolchain are configured in `build.bat`; adjust them to match your installation.

## Build

Run `build.bat` from the project root. It initializes the MSVC x64 environment, configures CMake with the Visual Studio 2026 generator and builds the Release configuration. The executables are written to:

```
build\Release\awake.exe
build\Release\awake.daemon.exe
```

Both executables statically link the C runtime and have no runtime dependencies on the build directory - copy them anywhere you like.

## Usage

```
awake help              Show the help message.
awake 0                 Disable keep-awake (restore default power behavior).
awake 1                 Enable keep-awake (block idle sleep and hibernation).
awake status            Show the current keep-awake status (never starts the daemon).
awake screen on         Keep the screen on (blocks screen off, screensaver and idle lock).
awake screen off        Stop keeping the screen on.
awake screen status     Show the current screen keep-awake status.
awake daemon on         Start the background daemon.
awake daemon status     Show daemon status and the watched applications
                        (green = configured and running, red = configured but not running).
awake daemon off        Stop the background daemon.
awake reload            Re-read the configuration file immediately.
awake reset             Reset the configuration file (asks for confirmation).
awake add <imagename>   Add an image name to the watch list.
awake del <imagename>   Remove an image name from the watch list.
```

Notes:

- `awake 0` and `awake 1` start the daemon automatically if it is not running.
- The keep-awake state is kept in the daemon's memory only. After the daemon restarts it is inactive again.
- When the daemon starts it sends a one-shot stay-awake notification to the system (resets the idle timer once), regardless of the current setting.

## Configuration file

`awake.ini` lives next to `awake.exe` (portable). It is created automatically with a descriptive comment header when missing. Comments start with `#` or `;`; every other non-empty line is one image name, for example:

```ini
[watch]
notepad.exe
```

While any listed image name is running, the daemon blocks idle sleep/hibernation. Use `awake add` / `awake del` to edit the list comfortably; changes are picked up automatically within 30 seconds, or immediately with `awake reload`. To discard all entries and restore the initial template, run `awake reset` - it warns you and asks for a second confirmation before overwriting the file, and a running daemon reloads the configuration right away.

## How it works

- The daemon holds the Windows execution state `ES_CONTINUOUS | ES_SYSTEM_REQUIRED` via `SetThreadExecutionState`, which blocks idle sleep/hibernation without intercepting manual sleep requests.
- A dedicated "power thread" owns all execution state calls, applies changes on demand and at least every 30 seconds, and resets the state before the daemon exits.
- Client and daemon communicate over the named pipe `\\.\pipe\awake_daemon` using a small line-based ASCII protocol. A mutex guarantees a single daemon instance.
- The daemon refuses to run unless launched by the client with a hidden `--internal-daemon <token>` option, so it cannot be started accidentally from the command line.
