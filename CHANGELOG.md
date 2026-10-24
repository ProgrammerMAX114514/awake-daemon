# Changelog

All notable changes of the awake project. Formats follow the conventions
from [keepachangelog.com](https://keepachangelog.com/); versions are
semantic (`MAJOR.MINOR.PATCH`). The
detailed notes for every release live in
[.github/release-notes/](.github/release-notes/) and on the
[GitHub releases page](https://github.com/ProgrammerMAX114514/awake-daemon/releases).

## [Unreleased]

Nothing yet.

## [1.0.0] - 2026-10-24

First stable release. **No functional changes since 0.8.0-beta-pre1.0.0** -
this release is the outcome of the stabilization round: the 0.8.0 feature
set with the pre-release suffix removed.

From 1.0.0 on, the command line interface and the daemon protocol are
covered by the stability promise described in CONTRIBUTING.md: existing
commands, aliases, options and wire verbs keep their meaning, and an
incompatible change requires a major version bump. New commands and status
fields may still be added in minor releases.

- Verified with the R16 acceptance orchestrator (auto and semi tiers) and
  the manual review checklist.
- Distribution stays .zip only. Installers and package-manager manifests
  are planned for a later release.

## [0.8.0-beta-pre1.0.0] - 2026-10-24

First pre-release of the upcoming 1.0.0 stable version (not fully tested
yet - the 24-hour soak is still pending). Completes the planned 1.0
feature set.

- Script-friendly output: `awake list`, `--json` for `list`/`status`,
  `--no-color` / `NO_COLOR`.
- Status details: power plan sleep/hibernate timeouts, screensaver setting
  and the execution state requests the daemon holds.
- Daemon logging (enabled by default): per-day rolling files in
  `%LOCALAPPDATA%\awake\logs\daemon`, configurable level and limits, hot
  reload, dependency-free zip export for issue attachments.
- Release hardening tooling (R16): `acceptance.py` (three-tier acceptance
  orchestrator), `stress.py` (concurrency storm + pipe fuzzing), `soak.py`
  (long-run stability) and the formal manual review checklist; new
  `verification.yml` workflow.
- Elevation guard: the daemon refuses to run elevated (its pipe would be
  uncontrollable from a normal shell); `AWAKE_ALLOW_ELEVATED=1` overrides
  for automation environments.
- Performance: the parsed watch list is cached by file stat and the process
  scan stops once every watched name is found.
- A vanished `awake.ini` is recreated automatically by the daemon.

## [0.7.0-beta] - 2026-10-24

- Power event awareness: on resume from sleep/hibernation the daemon
  immediately re-applies the execution state; `awake status` shows when the
  last resume was handled.
- Automated releases: pushing a version tag builds, tests and publishes a
  signed GitHub release (tag/version consistency enforced).
- Unit test foundation (Catch2, runs in CI).
- The version banner names the build commit; an AI-generated reminder is
  printed to stderr on interactive runs.
- Fixed: daemon autostart no longer fails on a fresh profile where the
  registry Run key does not exist yet.
- Fixed: a false "version mismatch" warning in `awake daemon on`.

## [0.6.0-beta] - 2026-10-23

- `awake aliases`: every command alias as an aligned table (also in help).
- Helpful mistakes: misspelled commands get up to three "Did you mean"
  suggestions ranked by edit distance.
- Atomic configuration writes: readers can never see a half-written
  `awake.ini`.
- Explainable daemon start failures (Win32 error codes with remedies).
- Faster startup detection and lower idle costs (empty watch list skips the
  process snapshot; set-based process matching).
- Path-free build scripts (PowerShell 7 + toolchain detection, NMake
  fallback) and a single version source in `CMakeLists.txt`.

## [0.5.0-beta] - 2026-10-22

- Elevation warning: an elevated client/daemon prints a warning (such a
  daemon cannot be controlled from a normal shell); `awake status` marks an
  elevated daemon with `(elevated)`.
- First-run license agreement: the Apache-2.0 text is shown once and the
  acceptance is stored in `awake.ini` (`help`/`version` stay exempt).
- Build scripts hardened (registry-policy-proof environment assembly).

## [0.4.0-beta] - 2026-10-03

- Protocol hardening: oversized/never-terminated requests are dropped;
  version handshake between client and daemon (major mismatch errors,
  minor mismatch warns).
- Autostart self-check: `awake status` flags a stale registry entry and
  `awake daemon enable` repairs it.
- Documentation set (CONTRIBUTING, SECURITY, issue/PR templates, badges,
  architecture diagram) and the semi-automated smoke test.

## [0.3.0-beta] - 2026-10-03

- Daemon autostart at logon (`awake daemon enable/disable`) with the state
  shown in `awake status`.
- Command aliases (`on`, `off`, `st`, `scr`, `d`, ...).
- Modularized codebase (client and daemon split into focused translation
  units; wire protocol centralized in `common/`).

## [0.2.0-beta] - 2026-10-03

- Unified status view: `awake status` now shows the daemon, keep-awake,
  screen and watch list state in one command (the separate `screen status`
  and `daemon status` commands were removed).

## [0.1.0-beta] - 2026-10-02

Initial release.

- Keep-awake (`awake 1`): blocks idle sleep/hibernation, never manual sleep.
- Screen keep-awake (`awake screen on`): blocks display-off, screensaver and
  idle lock.
- Application watch list (`awake add/del`): automatic keep-awake while a
  watched application runs.
- Background daemon with logon autostart, named pipe IPC, colored output and
  the `reload`/`reset` configuration commands.

[Unreleased]: https://github.com/ProgrammerMAX114514/awake-daemon/compare/0.8.0-beta-pre1.0.0...HEAD
[0.8.0-beta-pre1.0.0]: https://github.com/ProgrammerMAX114514/awake-daemon/compare/0.7.0-beta...0.8.0-beta-pre1.0.0
[0.7.0-beta]: https://github.com/ProgrammerMAX114514/awake-daemon/compare/0.6.0-beta...0.7.0-beta
[0.6.0-beta]: https://github.com/ProgrammerMAX114514/awake-daemon/compare/0.5.0-beta...0.6.0-beta
[0.5.0-beta]: https://github.com/ProgrammerMAX114514/awake-daemon/compare/0.4.0-beta...0.5.0-beta
[0.4.0-beta]: https://github.com/ProgrammerMAX114514/awake-daemon/compare/0.3.0-beta...0.4.0-beta
[0.3.0-beta]: https://github.com/ProgrammerMAX114514/awake-daemon/compare/0.2.0-beta...0.3.0-beta
[0.2.0-beta]: https://github.com/ProgrammerMAX114514/awake-daemon/compare/0.1.0-beta...0.2.0-beta
[0.1.0-beta]: https://github.com/ProgrammerMAX114514/awake-daemon/releases/tag/0.1.0-beta
