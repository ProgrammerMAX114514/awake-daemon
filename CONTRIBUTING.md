# Contributing to awake

Thank you for your interest in contributing! This project is AI-generated
(see the `NOTICE` file) - extra scrutiny of every contribution is very
welcome.

## Getting started

1. Fork/clone the repository.
2. Build the project: `utils\build.bat` (requires Visual Studio 2026 with
   the MSVC x64 toolset and CMake, see the paths inside the script).
3. Run the smoke test: `python utils\test\smoketest.py` (requires Python
   3.8+, no third-party packages). The configuration lives in
   `utils\test\smoketest.ini`.

## Ground rules

- **License**: by contributing you agree that your changes are licensed
  under the Apache License 2.0 (see `LICENSE`).
- **ASCII only**: all source code, scripts and documentation must contain
  standard ASCII characters only. The only exceptions are `README.zh.md`
  and the generated configuration template header, which must remain
  English/ASCII too.
- **License headers**: every new source file needs the Apache-2.0 header
  including the AI-generated software notice (copy it from any existing
  file, using the comment syntax of the file type).
- **Comments**: keep the detailed English comments that explain the "why"
  behind non-obvious decisions, especially around the power thread and the
  named pipe protocol.
- **Style**: match the style of the surrounding code (4-space indent,
  `namespace` modules, camelCase functions with a `module::` prefix).

## Commit and pull request guidelines

- Keep commits focused; one logical change per commit.
- Use the conventional style already in the log: `feat:`, `fix:`,
  `refactor:`, `docs:`, `chore:`, `test:`.
- Every pull request must pass the smoke test locally; include the test
  summary (e.g. "43 passed, 0 failed") in the description.
- Changes to the wire protocol must be made in `common/include/protocol.h`
  (single source of truth) and keep older clients working where possible -
  `ParseStatusResponse` ignores unknown KEY=VALUE lines on purpose.

## Reporting bugs

Use the GitHub issue templates. For security-related issues, see
`SECURITY.md`.
