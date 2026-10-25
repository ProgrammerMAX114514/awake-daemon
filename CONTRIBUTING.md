# Contributing to awake

Thank you for your interest in contributing! This project is AI-generated (see the `NOTICE` file) - extra scrutiny of every contribution is very welcome.

## Getting started

1. Fork/clone the repository.
2. Build the project: `utils\build.bat` (requires Visual Studio 2026 with the MSVC x64 toolset, CMake and PowerShell 7; the batch file only starts `utils\build.ps1`, which finds the toolchain on its own). Custom installations are supported through `utils\toolchain.ini` - copy `utils\toolchain.ini.example` and adjust the paths.
3. Run the smoke test: `python utils\test\smoketest.py` (requires Python 3.8+, no third-party packages). The configuration lives in `utils\test\smoketest.ini`.
4. For release hardening (R16), the verification tooling lives in `utils\test\` as well: `acceptance.py` (three-tier acceptance orchestrator), `stress.py` (concurrency storm + pipe fuzzing), `soak.py` (long-run stability, `--minutes 1440` for the full pass) and `manual-checklist.md` (the formal manual review checklist). The CI "Verification" workflow runs a bounded round on demand.

## Ground rules

- **License**: by contributing you agree that your changes are licensed under the Apache License 2.0 (see `LICENSE`).
- **ASCII only**: all source code, scripts and documentation must contain standard ASCII characters only. The only exceptions are `README.zh.md` and the generated configuration template header, which must remain English/ASCII too.
- **License headers**: every new source file needs the Apache-2.0 header including the AI-generated software notice (copy it from any existing file, using the comment syntax of the file type).
- **Comments**: keep the detailed English comments that explain the "why" behind non-obvious decisions, especially around the power thread and the named pipe protocol.
- **Style**: match the style of the surrounding code (4-space indent, `namespace` modules, camelCase functions with a `module::` prefix).
- **Version numbers**: the version is declared only in `CMakeLists.txt` (`project(awake VERSION ...)` plus `AWAKE_VERSION_SUFFIX`). `version.h` is generated from `common/include/version.h.in` during configuration - never edit a version anywhere else. The smoke test verifies that the client and both READMEs agree with `CMakeLists.txt`.

## Compatibility promise (1.0 and later)

Starting with 1.0.0 the public surface is stable:

- **Command line**: existing commands, aliases, options and the keys they print keep their meaning. Adding a command, an alias or a status field is allowed in a minor release; removing or renaming one requires a major version bump.
- **Wire protocol**: the verbs and the KEY=VALUE fields declared in `common/include/protocol.h` follow the same rule. Clients ignore unknown lines, so a newer daemon may add fields without breaking older clients.
- **Configuration**: `awake.ini` stays backward compatible. New keys are optional and existing files keep working unchanged.
- **Behaviour**: bug fixes may change behaviour that was clearly wrong. Any such change is documented in the CHANGELOG.

## Commit and pull request guidelines

- Keep commits focused; one logical change per commit.
- Use the conventional style already in the log: `feat:`, `fix:`, `refactor:`, `docs:`, `chore:`, `test:`.
- Every pull request must pass the smoke test locally; include the test summary (e.g. "91 passed, 0 failed") in the description. Run `python utils\test\acceptance.py --tier auto` for changes that touch the daemon core or the wire protocol.
- New behaviour in `common/` should come with a unit test in `tests/` (Catch2 is vendored there; run `ctest --test-dir build` or the `awake_tests.exe` binary). The smoke test covers the CLI end to end, the unit tests cover the parsing and helper logic.
- CI (`.github/workflows/ci.yml`) builds the project and runs the same smoke test on every push and pull request; a red run blocks the merge. Releases are produced by `release.yml` from a version tag - see the README for the short version.
- Changes to the wire protocol must be made in `common/include/protocol.h` (single source of truth) and keep older clients working where possible - `ParseStatusResponse` ignores unknown KEY=VALUE lines on purpose.

## Branching and pull requests

From 1.0.0 on, every change reaches `main` through a pull request. `main` is protected, so a direct push is refused: the branch is the place to work, the pull request is the place to review and to let CI decide.

- **Branch naming**: `feat/`, `fix/`, `docs/`, `ci/`, `test/`, `chore/` followed by a short description, for example `fix/smoke-version-assert`.
- **Flow**: commit on the branch (GPG signed) -> `git push -u origin <branch>` -> `gh pr create` -> CI must be green -> fill in the pull request template -> merge.
- **Merge strategy: create a merge commit.** This keeps the signed commits from the branch in the history and adds a single merge commit. Squash and rebase rewrite the commits, which replaces your signature with the one GitHub applies, so use them only when collapsing a messy history is worth that trade.
- **Two accounts, one review.** The author account (`ProgrammerMAX114514`) writes and signs the code; an independent reviewer account (`BuddyPRReviewer`) reviews it. GitHub does not allow an account to approve its own pull request, so the reviewer must be a second account with write access. The reviewer is added as a collaborator with `push` permission and is listed in `.github/CODEOWNERS`, so every pull request requests that review automatically.
- **Hotfixes too.** A broken `main` is fixed on a branch like everything else. The only exception is a broken workflow that prevents pull requests from running at all; then push directly, and open a pull request afterwards that documents what was done.
- **Releases**: the tag is created on `main` after the pull request is merged, following the release steps in the README.

Required before a pull request can be merged (configured as a branch protection rule on `main`):

- the `CI` workflow is green (build, unit tests, smoke test, Debug unit tests, `/analyze`);
- the branch is up to date with `main`;
- one approving review from `@BuddyPRReviewer` (`required_approving_review_count = 1`, `require_code_owner_reviews = true`);
- the approval is still valid: `dismiss_stale_reviews` and `require_last_push_approval` are on, so a new commit on the branch invalidates the previous approval and the reviewer has to approve again;
- no direct pushes, no force pushes.

Branch protection also applies to administrators (`enforce_admins = true`), so the author account cannot merge its own pull request even though it owns the repository.

### Reviewing a pull request

Reviews are left by an independent account, so that "this was reviewed" is a fact GitHub enforces rather than a convention. The full procedure - the two accounts, the commands, the rules for the reviewer and the troubleshooting notes - is in [docs/review-workflow.md](docs/review-workflow.md).

Three rules are repeated here because breaking them damages the repository or the record rather than a single review: pass the reviewer token through `GH_TOKEN` instead of changing the `gh` login; never run `gh auth setup-git` in a review session, because it replaces the Windows Git Credential Manager and hands pushes to the account `gh` holds, which breaks the GPG signing chain of the author account; and state in every review comment that an AI assistant wrote it, for example `> **This review was generated by an AI assistant.**`.

## Reporting bugs

Use the GitHub issue templates. For security-related issues, see `SECURITY.md`.
