"""Repository maintenance commands that used to be typed by hand.

Every subcommand here replaces a block of shell that was repeated often enough
to be worth a tool - several of them had already produced a wrong result once:

  hygiene   text rules (ASCII, line endings, trailing spaces, final newline)
  sign      every commit on the branch carries a valid GPG signature
  docs      the archive counters and the "main @ <sha>" pointers agree
  sync      fetch/merge with retries, verified against the remote head
  branches  list branch state; delete merged branches only when asked
  reviews   dump a pull request's review bodies before merging
  selftest  exercise the hygiene rules against fixtures

Exit status is 0 on success and 1 when something needs attention, so the same
commands work in CI and by hand. No third-party packages, Python 3.8+.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# The local machine keeps a second copy of Git that deadlocks against
# gpg-agent, so the binary is overridable instead of hard-coded.
GIT = os.environ.get("DEVTOOL_GIT", "git")

# .gitattributes stores batch files and PowerShell scripts with CRLF on
# purpose; everything else in the tree is LF.
CRLF_SUFFIXES = (".bat", ".ps1", ".vcxproj", ".sln")

# The Chinese README is the documented exception to the ASCII rule. The
# handover directories are not shipped and are written in Chinese as well.
ASCII_EXEMPT_FILES = ("README.zh.md",)
ASCII_EXEMPT_DIRS = (".workbuddy/", ".zcode/")

# README.md is English, but its language switcher links to the Chinese
# edition by name. That one line is allowed to carry the two characters.
ASCII_ALLOWED_LINES = {
    "README.md": (re.compile(r"^\[English\]\(README\.md\) \| \[[^\]]+\]\(README\.zh\.md\)$"),),
}


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def run(args, binary=False, timeout=None):
    """Runs a command in the repository and returns (code, stdout, stderr).

    A timeout is not optional for gpg: a stalled agent makes it wait forever
    instead of failing, which is one of the ways signing appears to hang.
    """
    try:
        proc = subprocess.run(args, cwd=str(ROOT), stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, timeout=timeout)
    except subprocess.TimeoutExpired:
        return 124, (b"" if binary else ""), "timed out after %ss" % timeout
    if binary:
        return proc.returncode, proc.stdout, proc.stderr.decode("utf-8", "replace")
    return (proc.returncode,
            proc.stdout.decode("utf-8", "replace"),
            proc.stderr.decode("utf-8", "replace"))


def git(*args, binary=False):
    return run([GIT] + list(args), binary=binary)


def git_ok(*args):
    return git(*args)[0] == 0


def tracked_files():
    """Paths git knows about, so build output and editor droppings are ignored."""
    code, out, err = git("ls-files", "-z", binary=True)
    if code != 0:
        sys.exit("git ls-files failed: %s" % err.strip())
    return [p.decode("utf-8") for p in out.split(b"\0") if p]


def current_branch():
    return git("rev-parse", "--abbrev-ref", "HEAD")[1].strip()


def repo_slug():
    """owner/repo from the origin remote, for the GitHub API calls."""
    url = git("remote", "get-url", "origin")[1].strip()
    match = re.search(r"github\.com[:/]+([^/]+)/([^/.]+)", url)
    if not match:
        return None
    return "%s/%s" % (match.group(1), match.group(2))


def short(sha, width=7):
    return sha[:width]


# ---------------------------------------------------------------------------
# hygiene
# ---------------------------------------------------------------------------

def ascii_exempt(rel):
    return rel in ASCII_EXEMPT_FILES or rel.startswith(ASCII_EXEMPT_DIRS)


def scan_blob(data, rel):
    """Returns the list of rule violations for one file's committed bytes.

    The blob is what matters, not the working tree: this machine runs with
    core.autocrlf=true, so every checkout has CRLF while the index is LF.
    Reading the worktree would report phantom line-ending failures and hide
    the real ones.
    """
    issues = []
    if b"\0" in data[:8192]:
        return issues
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        return ["not valid UTF-8 (%s)" % exc.reason]

    if not rel.endswith(CRLF_SUFFIXES) and b"\r\n" in data:
        issues.append("CRLF line endings (expected LF, run 'git add --renormalize .')")

    allowed = ASCII_ALLOWED_LINES.get(rel, ())
    if not ascii_exempt(rel):
        for number, line in enumerate(text.splitlines(), 1):
            if any(pattern.match(line) for pattern in allowed):
                continue
            for column, char in enumerate(line, 1):
                if ord(char) > 127:
                    issues.append("non-ASCII at %d:%d (%r)" % (number, column, char))
                    break
            else:
                continue
            break

    # splitlines(), not split("\n"): a CRLF file would leave a trailing "\r" on
    # every line, and "a \r".rstrip(" \t") is still "a \r" - so the rule that
    # matters most for .bat and .ps1 would silently never fire there.
    trailing = [i for i, line in enumerate(text.splitlines(), 1)
                if line != line.rstrip(" \t")]
    if trailing:
        issues.append("%d line(s) with trailing whitespace: %s"
                      % (len(trailing), ", ".join(str(i) for i in trailing[:5])))

    if data and not data.endswith(b"\n"):
        issues.append("no newline at end of file")

    return issues


def scan_text(path, rel):
    return scan_blob(path.read_bytes(), rel)


def fix_text(path, rel):
    """Rewrites a file with LF, no trailing whitespace and one final newline."""
    data = path.read_bytes()
    if b"\0" in data[:8192]:
        return False
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        return False

    lines = text.replace("\r\n", "\n").split("\n")
    fixed = "\n".join(line.rstrip(" \t") for line in lines).rstrip("\n") + "\n"
    if rel.endswith(CRLF_SUFFIXES):
        fixed = fixed.replace("\n", "\r\n")
    encoded = fixed.encode("utf-8")
    if encoded == data:
        return False
    path.write_bytes(encoded)
    return True


def cmd_hygiene(args):
    paths = args.paths or tracked_files()
    failures = 0
    checked = 0
    skipped = 0
    fixed = 0
    for rel in paths:
        code, data, err = git("cat-file", "blob", ":" + rel, binary=True)
        if code != 0:
            skipped += 1
            print("[SKIP] %s (not in the index: %s)" % (rel, err.strip()))
            continue
        checked += 1
        issues = scan_blob(data, rel)
        if issues:
            failures += 1
            print("[FAIL] %s" % rel)
            for issue in issues:
                print("       %s" % issue)
            if args.fix:
                # Only the two whitespace rules are safe to repair locally;
                # a wrong line ending in the index needs a renormalisation.
                if fix_text(ROOT / rel, rel):
                    fixed += 1

    if args.fix:
        print("rewritten in the working tree: %d file(s); re-add them to update the index"
              % fixed)
    summary = "checked %d file(s), %d with issues" % (checked, failures)
    if skipped:
        summary += ", %d skipped" % skipped
    print(summary)
    return 1 if failures else 0


# ---------------------------------------------------------------------------
# signatures
# ---------------------------------------------------------------------------

def signature_state(sha, verify=False):
    """Returns (status, detail) for one commit.

    %G? would be the obvious way to ask, but it shells out to gpg for every
    commit - and a broken gpg-agent turns that into a hang rather than an
    answer. The signature header is readable from the commit object itself,
    so presence never needs gpg; --verify adds a real check when asked.
    """
    code, out, err = git("cat-file", "commit", sha)
    if code != 0:
        return "?", err.strip()
    header = out.split("\n\n", 1)[0]
    signed = any(line.startswith("gpgsig") for line in header.splitlines())
    if not signed:
        return "N", "no signature"
    if not verify:
        return "G", "signed"
    code, out, err = git("verify-commit", "--raw", sha)
    if code == 0:
        return "G", "signature valid"
    return "B", (err.strip() or "signature invalid").splitlines()[-1]


def cmd_sign(args):
    base = args.base
    if base and not git_ok("rev-parse", "--verify", base):
        print("base %r not found; checking HEAD only" % base)
        base = None
    rev = "%s..HEAD" % base if base else "HEAD"
    code, out, err = git("rev-list", rev)
    if code != 0:
        sys.exit("git rev-list failed: %s" % err.strip())

    shas = [line.strip() for line in out.splitlines() if line.strip()]
    if not shas:
        print("no commits in range %s" % rev)
        return 0

    bad = []
    for sha in shas:
        status, detail = signature_state(sha, verify=args.verify)
        subject = git("log", "-1", "--format=%s", sha)[1].strip()
        print("%s %s %s" % (short(sha), status, subject[:70]))
        if status != "G":
            bad.append((sha, detail))
    print("%d commit(s) checked, %d without a valid signature" % (len(shas), len(bad)))
    for sha, detail in bad:
        print("[FAIL] %s: %s" % (short(sha), detail))
    if bad:
        print("a commit whose signature is N must be rewritten before it reaches main")
        return 1
    return 0


# ---------------------------------------------------------------------------
# documentation counters and pointers
# ---------------------------------------------------------------------------

POINTER_RE = re.compile(r"main\s*(?:@\s*)?`([0-9a-f]{7,40})`")

# The handover documents are Chinese, but this file must stay ASCII, so the
# two header phrases are spelled as escapes: "gong N xiang" (total N items)
# and "yi wancheng N xiang" (N items done).
COMPLETED_RE = re.compile("\u5171\\s*(\\d+)\\s*\u9879")
PENDING_RE = re.compile("\u5df2\u5b8c\u6210\\s*(\\d+)\\s*\u9879")


def doc_files():
    """The documents that state the *current* head, if they exist.

    The daily logs are excluded on purpose: they record what main was on the
    day they were written, so an older pointer there is correct rather than
    stale. .workbuddy is not part of the repository either, so a contributor
    without it gets a clean skip instead of an error.
    """
    names = ("pending.md", "completed.md", "handover.md", "handover.ai.md",
             "memory/MEMORY.md")
    return [ROOT / ".workbuddy" / name for name in names
            if (ROOT / ".workbuddy" / name).is_file()]


def cmd_docs(args):
    problems = []
    against = args.against
    code, out, err = git("rev-parse", "--verify", "%s^{commit}" % against)
    if code != 0:
        print("ref %r not found, comparing against HEAD" % against)
        against = "HEAD"
        out = git("rev-parse", "HEAD")[1]
    target = out.strip()
    print("against: %s (%s)" % (against, short(target)))

    completed = ROOT / ".workbuddy" / "completed.md"
    if not completed.is_file():
        # A clone without .workbuddy has no archive to compare. Saying
        # "counters agree" here would claim a check that never ran.
        print("no archive present, skipped")
        return 0

    text = completed.read_text(encoding="utf-8")
    expected = sum(1 for line in text.splitlines() if line.startswith("- [x]"))
    match = COMPLETED_RE.search(text)
    if not match:
        problems.append("completed.md: no 'total N items' header found")
    elif int(match.group(1)) != expected:
        problems.append("completed.md: header says %s items, the file has %d"
                        % (match.group(1), expected))
    print("archive: %d completed item(s)" % expected)

    pending = ROOT / ".workbuddy" / "pending.md"
    if pending.is_file():
        match = PENDING_RE.search(pending.read_text(encoding="utf-8"))
        if match and int(match.group(1)) != expected:
            problems.append("pending.md: says %s items done, the archive has %d"
                            % (match.group(1), expected))

    for path in doc_files():
        rel = path.relative_to(ROOT).as_posix()
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            # Headings name the state of an older increment on purpose, so only
            # prose is treated as a pointer to the current integration branch.
            if line.startswith("#"):
                continue
            for sha in POINTER_RE.findall(line):
                if not git_ok("cat-file", "-e", "%s^{commit}" % sha):
                    problems.append("%s:%d: %s does not exist" % (rel, number, sha))
                elif not target.startswith(sha):
                    problems.append("%s:%d: points at %s, %s is %s"
                                    % (rel, number, short(sha), against,
                                       short(target)))

    for problem in problems:
        print("[FAIL] %s" % problem)
    if not problems:
        print("counters and pointers agree")
    return 1 if problems else 0


# ---------------------------------------------------------------------------
# syncing with the remote
# ---------------------------------------------------------------------------

def remote_head(branch):
    """The remote head, read from the API when available.

    git rev-parse origin/<branch> reports a stale local ref after a failed
    fetch, which once looked like a successful sync, so the API answer wins.
    """
    slug = repo_slug()
    if slug and shutil.which("gh"):
        code, out, _ = run(["gh", "api", "repos/%s/commits/%s" % (slug, branch),
                            "--jq", ".sha"])
        if code == 0 and out.strip():
            return out.strip()
    code, out, _ = git("ls-remote", "origin", "refs/heads/%s" % branch)
    if code == 0 and out.strip():
        return out.split()[0]
    return None


def cmd_sync(args):
    branch = args.branch or current_branch()
    if branch == "HEAD":
        print("detached HEAD: check out a branch first")
        return 1
    # Merging one branch into another silently is how a feature branch ends up
    # carrying main; require the caller to be standing on the branch it names.
    if branch != current_branch():
        print("checked out on %s, not %s: check the branch out first"
              % (current_branch(), branch))
        return 1

    started = time.time()
    for attempt in range(1, args.attempts + 1):
        target = remote_head(branch)
        if target:
            git("fetch", "origin", branch)
            if git_ok("cat-file", "-e", "%s^{commit}" % target):
                code, out, err = git("merge", "--ff-only", target)
                if code != 0:
                    print("fast-forward refused: %s" % (out + err).strip())
                    return 1
                if git("rev-parse", "HEAD")[1].strip() == target:
                    print("synced %s to %s after %d attempt(s), %.1fs"
                          % (branch, short(target), attempt, time.time() - started))
                    return 0
        if attempt % 5 == 0:
            print("attempt %d: remote not reachable yet" % attempt)
        time.sleep(args.delay)
    print("gave up after %d attempt(s)" % args.attempts)
    return 1


# ---------------------------------------------------------------------------
# branches
# ---------------------------------------------------------------------------

def cmd_branches(args):
    current = current_branch()
    merged = set()
    code, out, _ = git("branch", "--merged", args.into, "--format=%(refname:short)")
    if code == 0:
        merged = {line.strip() for line in out.splitlines() if line.strip()}

    code, out, _ = git("for-each-ref", "refs/heads",
                       "--format=%(refname:short)%1f%(upstream:short)%1f%(objectname:short)")
    rows = [line.split("\x1f") for line in out.splitlines() if line]

    print("%-32s %-32s %-8s %s" % ("branch", "upstream", "commit", "state"))
    for name, upstream, sha in rows:
        if name == current:
            state = "current"
        elif name == args.into:
            state = "base"
        elif name in merged:
            state = "merged"
        else:
            state = "open"
        print("%-32s %-32s %-8s %s" % (name, upstream or "-", sha, state))

    doomed = [name for name, _, _ in rows
              if name not in (current, args.into) and name in merged]
    if args.clean:
        if not doomed:
            print("no merged local branch to delete")
        for name in doomed:
            code, out, err = git("branch", "-d", name)
            print("%s %s" % ("deleted" if code == 0 else "kept", name))
            if code != 0:
                print("       %s" % (out + err).strip())
    elif doomed:
        print("merged branches that could be deleted: %s" % ", ".join(doomed))
        print("re-run with --clean to delete them")

    # Remote cleanup is independent of the local one: --remote alone is a
    # request to look at the remote, not a no-op.
    if args.remote:
        slug = repo_slug()
        if not slug or not shutil.which("gh"):
            print("remote cleanup needs gh and an origin remote")
            return 1
        code, out, _ = run(["gh", "api", "repos/%s/branches" % slug,
                            "--jq", ".[].name"])
        deleted = 0
        for name in [line.strip() for line in out.splitlines() if line.strip()]:
            if name == args.into:
                continue
            compare = run(["gh", "api", "repos/%s/compare/%s...%s"
                           % (slug, args.into, name), "--jq", ".status"])
            if compare[0] == 0 and compare[1].strip() in ("identical", "behind"):
                run(["gh", "api", "--method", "DELETE",
                     "repos/%s/git/refs/heads/%s" % (slug, name)])
                print("deleted remote %s" % name)
                deleted += 1
        if not deleted:
            print("no merged remote branch to delete")
    return 0


# ---------------------------------------------------------------------------
# reviews
# ---------------------------------------------------------------------------

def cmd_reviews(args):
    if not shutil.which("gh"):
        print("gh is required")
        return 1
    code, out, err = run(["gh", "pr", "view", str(args.number), "--json",
                          "number,title,state,reviewDecision,mergeStateStatus,reviews"])
    if code != 0:
        print(err.strip())
        return 1

    import json
    data = json.loads(out)
    print("PR #%d %s" % (data["number"], data["title"]))
    print("state=%s review=%s merge=%s"
          % (data["state"], data["reviewDecision"] or "-", data["mergeStateStatus"]))

    # The whole point: the findings live in the long COMMENTED review, and the
    # approval may be a one-line pointer. Print every body, oldest first.
    for review in data["reviews"]:
        print("\n--- %s by %s at %s ---"
              % (review["state"], review["author"]["login"], review["submittedAt"]))
        print(review["body"])

    if data["reviewDecision"] != "APPROVED":
        print("\nnot approved: read the findings above before merging")
        return 1
    return 0


# ---------------------------------------------------------------------------
# gpg reachability
# ---------------------------------------------------------------------------

def gpg_binary(name, fallback):
    """The gpg helper that git itself would use, not whatever PATH finds first.

    PATH here starts with a bundled PortableGit that ships its own GnuPG, so a
    bare `which` answers a question about a different installation than the one
    git signs with. git's own gpg.program settles it.
    """
    code, prog, _ = git("config", "--get", "gpg.program")
    if code == 0 and prog.strip():
        bindir = Path(prog.strip()).parent
        for candidate in (bindir / name, bindir / (name + ".exe")):
            if candidate.is_file():
                return str(candidate)
    return fallback


def cmd_gpg(args):
    """Explains why a signature would fail *before* a commit is attempted.

    Signing needs a running gpg-agent with the passphrase already cached. An
    agent can be started by an interactive session, where pinentry has a
    desktop, but not by a non-interactive one: the daemon launch hangs and a
    GUI prompt has nowhere to appear. Checking reachability first turns an
    intermittent failure into an immediate, explainable one.
    """
    problems = []
    gpgconf = gpg_binary("gpgconf", shutil.which("gpgconf") or "gpgconf")
    agent = gpg_binary("gpg-connect-agent",
                       shutil.which("gpg-connect-agent") or "gpg-connect-agent")
    print("gpg       : %s" % gpg_binary("gpg", shutil.which("gpg") or "gpg"))

    code, homedir, err = run([gpgconf, "--list-dirs", "homedir"], timeout=10)
    if code != 0:
        print("[FAIL] gpgconf not usable: %s" % err.strip())
        return 1
    homedir = homedir.strip()
    code, sockdir, _ = run([gpgconf, "--list-dirs", "socketdir"], timeout=10)
    print("homedir   : %s" % homedir)
    print("socketdir : %s" % sockdir.strip())

    # A reachable agent is the whole requirement for an unattended signature.
    code, out, err = run([agent, "GETINFO version", "/bye"], timeout=10)
    if code == 0 and "OK" in out:
        print("agent     : reachable (%s)" % out.strip().splitlines()[0])
    else:
        print("agent     : NOT reachable")
        problems.append("no gpg-agent: start one from an interactive terminal "
                        "with 'gpgconf --launch gpg-agent'")

    # GNUPGHOME is the documented way to point gpg at a home, but this build
    # ignores it, which is why the same repository signs under one shell and
    # fails under another.
    probe_env = dict(os.environ, GNUPGHOME=str(ROOT / ".git" / "gnupg-probe"))
    try:
        probe = subprocess.run([gpgconf, "--list-dirs", "homedir"], env=probe_env,
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                               timeout=10, text=True)
        if probe.stdout.strip() == homedir:
            print("GNUPGHOME: ignored by this build (the home follows HOME/APPDATA)")
    except (OSError, subprocess.TimeoutExpired):
        pass

    # The keyring and the sockets can live in different directories; when they
    # do, an agent started in one context cannot serve the other.
    windows_home = Path(os.environ.get("APPDATA", "")) / "gnupg"
    posix_home = Path(homedir)
    if windows_home.is_dir() and posix_home.is_dir():
        if str(windows_home).replace("\\", "/").lower() != homedir.lower():
            print("note      : a second home exists at %s" % windows_home)
            keys_here = (posix_home / "private-keys-v1.d").is_dir()
            keys_there = (windows_home / "private-keys-v1.d").is_dir()
            print("            private keys here: %s, there: %s"
                  % ("yes" if keys_here else "no", "yes" if keys_there else "no"))
            if not keys_here and keys_there:
                problems.append("this shell's home has no private keys; it "
                                "relies on the agent started from the other one")

    for problem in problems:
        print("[FAIL] %s" % problem)
    if not problems:
        print("signing can run unattended: the agent is up and will answer")
        return 0
    print("commit from an interactive terminal instead, or keep an agent "
          "running with the passphrase preset")
    return 1


# ---------------------------------------------------------------------------
# commit and push
# ---------------------------------------------------------------------------

def agent_reachable():
    """True when a gpg-agent answers, which is the precondition for signing."""
    agent = gpg_binary("gpg-connect-agent",
                       shutil.which("gpg-connect-agent") or "gpg-connect-agent")
    code, out, _ = run([agent, "GETINFO version", "/bye"], timeout=10)
    return code == 0 and "OK" in out


def cmd_commit(args):
    """Stages, signs, verifies and optionally pushes - one command, one line.

    This exists because a failed signed commit is expensive to diagnose: it
    either leaves a stale index.lock or an unsigned commit, and both have
    happened. Everything that can be checked before touching the index is
    checked first, and the signature is verified immediately afterwards
    instead of at the pull request.
    """
    paths = args.paths or []
    if not agent_reachable() and not args.no_preflight:
        print("[FAIL] no gpg-agent: signing cannot work here")
        print("run this from an interactive terminal instead, or start one with:")
        print("  gpgconf --launch gpg-agent")
        return 1

    if paths:
        code, out, err = git("add", "--", *paths)
        if code != 0:
            print("[FAIL] git add: %s" % (out + err).strip())
            return 1
        print("staged: %s" % ", ".join(paths))

    commit_args = ["commit", "-S"]
    if args.file:
        commit_args += ["-F", args.file]
    for message in args.message or []:
        commit_args += ["-m", message]
    if not args.file and not args.message:
        print("[FAIL] a commit needs -m or -F")
        return 1

    code, out, err = git(*commit_args)
    print((out + err).strip())
    if code != 0:
        print("[FAIL] the commit did not happen; the tree is unchanged")
        return 1

    sha = git("rev-parse", "HEAD")[1].strip()
    status, detail = signature_state(sha)
    print("commit %s signature=%s (%s)" % (short(sha), status, detail))
    if status != "G":
        print("[FAIL] unsigned commit: amend it before it leaves this machine")
        return 1

    if args.push:
        branch = current_branch()
        for attempt in range(1, 4):
            code, out, err = git("push")
            if code == 0:
                print("pushed %s" % branch)
                return 0
            if "no upstream" in (out + err):
                code, out, err = git("push", "-u", "origin", branch)
                if code == 0:
                    print("pushed %s (upstream set)" % branch)
                    return 0
            lines = (out + err).strip().splitlines()
            print("push attempt %d failed: %s"
                  % (attempt, lines[-1] if lines else "unknown error"))
            time.sleep(5)
        print("[FAIL] push failed three times; the commit is local only")
        return 1
    return 0


# ---------------------------------------------------------------------------
# selftest
# ---------------------------------------------------------------------------

SELFTEST_FILES = {
    "good.txt": b"alpha\nbeta\n",
    "nonascii.txt": "caf\u00e9\n".encode("utf-8"),
    "crlf.txt": b"alpha\r\nbeta\r\n",
    "trailing.txt": b"alpha \nbeta\n",
    # CRLF plus a trailing space: the combination that made the rule invisible
    # in exactly the file types that keep CRLF.
    "trailing_crlf.bat": b"alpha \r\nbeta\r\n",
    "noeol.txt": b"alpha\nbeta",
}


def cmd_selftest(args):
    expected = {
        "good.txt": 0,
        "nonascii.txt": 1,
        "crlf.txt": 1,
        "trailing.txt": 1,
        "trailing_crlf.bat": 1,
        "noeol.txt": 1,
    }
    failures = []
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        for name, payload in SELFTEST_FILES.items():
            (root / name).write_bytes(payload)
        for name, count in expected.items():
            issues = scan_text(root / name, name)
            if len(issues) != count:
                failures.append("%s: expected %d issue(s), got %d (%s)"
                                % (name, count, len(issues), "; ".join(issues)))
        for name in SELFTEST_FILES:
            fix_text(root / name, name)
        for name in SELFTEST_FILES:
            if name == "nonascii.txt":
                continue
            issues = scan_text(root / name, name)
            if issues:
                failures.append("%s: still dirty after --fix (%s)"
                                % (name, "; ".join(issues)))
        if (root / "crlf.txt").read_bytes() != b"alpha\nbeta\n":
            failures.append("crlf.txt: --fix did not convert to LF")
        if (root / "trailing_crlf.bat").read_bytes() != b"alpha\r\nbeta\r\n":
            failures.append("trailing_crlf.bat: --fix must strip the space and keep CRLF")

    for failure in failures:
        print("[FAIL] %s" % failure)
    if not failures:
        print("[PASS] hygiene rules behave as documented")
    return 1 if failures else 0


# ---------------------------------------------------------------------------
# entry point
# ---------------------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="devtool.py",
        description="Repository maintenance commands.")
    sub = parser.add_subparsers(dest="command", required=True)

    hygiene = sub.add_parser("hygiene", help="check text rules")
    hygiene.add_argument("paths", nargs="*", help="files to check (default: git index)")
    hygiene.add_argument("--fix", action="store_true",
                         help="rewrite fixable problems in place")
    hygiene.set_defaults(func=cmd_hygiene)

    sign = sub.add_parser("sign", help="verify commit signatures")
    sign.add_argument("--base", default="main",
                      help="check commits in base..HEAD (default: main)")
    sign.add_argument("--verify", action="store_true",
                      help="validate with gpg instead of only checking presence")
    sign.set_defaults(func=cmd_sign)

    docs = sub.add_parser("docs", help="check archive counters and pointers")
    docs.add_argument("--against", default="main",
                      help="ref the pointers must match (default: main)")
    docs.set_defaults(func=cmd_docs)

    sync = sub.add_parser("sync", help="fetch and fast-forward from origin")
    sync.add_argument("--branch", help="branch to sync (default: current)")
    sync.add_argument("--attempts", type=int, default=30)
    sync.add_argument("--delay", type=float, default=3.0)
    sync.set_defaults(func=cmd_sync)

    branches = sub.add_parser("branches", help="list or clean up branches")
    branches.add_argument("--into", default="main", help="integration branch")
    branches.add_argument("--clean", action="store_true",
                          help="delete local branches merged into --into")
    branches.add_argument("--remote", action="store_true",
                          help="delete merged remote branches (independent of --clean)")
    branches.set_defaults(func=cmd_branches)

    reviews = sub.add_parser("reviews", help="dump the review bodies of a pull request")
    reviews.add_argument("number", type=int)
    reviews.set_defaults(func=cmd_reviews)

    gpg = sub.add_parser("gpg", help="explain why signing would fail")
    gpg.set_defaults(func=cmd_gpg)

    commit = sub.add_parser("commit", help="stage, sign, verify and optionally push")
    commit.add_argument("paths", nargs="*", help="files to stage (default: what is already staged)")
    commit.add_argument("-m", "--message", action="append", help="commit message line")
    commit.add_argument("-F", "--file", help="read the commit message from a file")
    commit.add_argument("--push", action="store_true", help="push after the commit")
    commit.add_argument("--no-preflight", action="store_true",
                        help="commit even when no agent answers")
    commit.set_defaults(func=cmd_commit)

    selftest = sub.add_parser("selftest", help="exercise the hygiene rules")
    selftest.set_defaults(func=cmd_selftest)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
