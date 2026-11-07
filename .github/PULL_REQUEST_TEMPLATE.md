## Summary

What does this pull request change and why?

## Type of change

- [ ] Bug fix (non-breaking)
- [ ] New feature (non-breaking)
- [ ] Breaking change (fix or feature that changes existing behavior)
- [ ] Refactoring (no functional change)
- [ ] Documentation
- [ ] CI, release or repository configuration

## Checklist

- [ ] `utils\build.bat` builds cleanly (/W4, no new warnings).
- [ ] `python utils\test\smoketest.py` passes - summary:
- [ ] New/changed source files carry the Apache-2.0 header with the AI-generated notice.
- [ ] All added text is standard ASCII.
- [ ] README.md and README.zh.md updated (if user-visible behavior changed).
- [ ] Wire protocol changes went through `common/include/protocol.h`.

CI is deliberately not a checkbox. When the pull request is opened the run has not finished yet, so the only honest options would be leaving it unticked or ticking it on hope. The gate is branch protection instead: `Build and smoke test`, `Static analysis (/analyze)` and `Text hygiene` must all pass before the merge button works, and the author confirms that at merge time rather than asserting it here.

## Additional notes

Anything reviewers should look at specifically (for example power thread or named pipe behavior).

> **Merge with "Create a merge commit"**, not squash or rebase: that keeps the signed commits from the branch in the history. See the "Branching and pull requests" section of `CONTRIBUTING.md`.
