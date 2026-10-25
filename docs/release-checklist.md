# Release checklist

Every release goes through these steps. They are written down because the failures here are silent: a missed version line ships a binary that reports the wrong version, and a missed packaging step produces a download nobody can verify.

## Before tagging

1. **Version, in one place.** `CMakeLists.txt` has `project(awake VERSION x.y.z)` and `set(AWAKE_VERSION_SUFFIX "...")`. The suffix is empty for a stable release and holds the pre-release marker otherwise (`-beta`, `-beta-pre1.0.0`, ...). Nothing else defines the version: `common/include/version.h` is generated from it, so do not edit that file.
2. **Both READMEs.** Update the `Current version` line in `README.md` and the corresponding version line at the top of `README.zh.md`. The smoke test compares them against the build and fails when they drift, which is how this step was learned.
3. **`SECURITY.md`.** The supported-versions table lists only the current release line.
4. **`CHANGELOG.md`.** Move the `[Unreleased]` entries into a new `## [x.y.z] - YYYY-MM-DD` section and leave `Nothing yet.` under Unreleased. The version sections go back to 0.1.0; keep it that way.
5. **`.github/release-notes/<tag>.md`.** Write the notes for the GitHub release. A pre-release starts with a `> [!WARNING]` block before the date line; a stable release does not.
6. **Build and test from a clean tree.** `utils\build.bat`, then the unit tests and the smoke test. A dirty tree means the binary may not match the commit you are about to tag.

## Tagging and publishing

7. **Stop the daemon first.** `awake status` to confirm nothing is being held, then `awake daemon off`. `pubrelease` links and copies `awake.daemon.exe`; a running daemon makes that fail with "file in use" and the whole command aborts.
8. **Package and sign.** `utils\pubrelease.bat <tag>` builds a Release build, creates the GPG-signed tag and packs `build\Release\publish\tag-<tag>\awake-daemon.win64.zip` with the executables, `LICENSE`, `NOTICE` and both READMEs. The script does not push anything.
9. **Push `main` and the tag.** `git push origin main` and `git push origin <tag>`. Pushing a tag is a separate operation and is easy to forget; the release only happens once the tag is on the remote.
10. **Let `release.yml` do the rest.** It verifies that the tag matches the version compiled into the binaries, generates `SHA256SUMS.txt`, injects the `Full Changelog` link into the notes and creates the release. `prerelease` is decided by `contains(tag, '-')`, so a tag with a dash is marked pre-release automatically and `x.y.z` is not.
11. **Verify the result.** `gh release view <tag>`: the assets should be the zip and `SHA256SUMS.txt`, and the body should match `release-notes/<tag>.md` apart from the injected link. For a stable release, confirm that the pre-release flag is off.

## Things that have gone wrong before

- A release was tagged locally but `main` was never pushed, leaving the tag ahead of the branch. Push both, and check with `git rev-parse origin/main` rather than assuming they move together.
- `pubrelease` aborted because the daemon was still running; the fix is step 7, not retrying.
- The smoke test failed on a version bump because an assertion hard-coded `awake 0.`; it now matches the shape of the version instead.
- The release notes were hand-edited into hard-wrapped paragraphs that rendered differently from the repository copy. `docs/review-workflow.md` and this file are subject to the same rule as the rest of the documentation: one paragraph per physical line.
