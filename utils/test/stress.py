#!/usr/bin/env python3
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
# stress.py - concurrency storm and pipe fuzzing against the running daemon
# (R16, tier "auto").
#
# Usage:
#   python utils/test/stress.py [--config ...] [--processes 8]
#                               [--requests 25] [--fuzz-rounds 50]
#                               [--skip-storm] [--skip-fuzz]
#
#   storm : N worker threads fire a mix of client commands in parallel; every
#           invocation must exit 0 and the daemon must still serve afterwards.
#   fuzz  : randomized raw pipe attacks in a single connection per round
#           (oversized frames, garbage bytes, missing newlines, malformed
#           commands, fragmented valid commands) - the daemon must survive
#           every round and keep serving.
#
# The script assumes a RUNNING daemon (start one with 'awake daemon on').
# Exits non-zero when any check fails. It is standalone-runnable and is also
# invoked by utils/test/acceptance.py.
#
# Raw pipe access is done through PowerShell one-liners because Python's
# stdlib cannot open named pipe clients (same trick as smoketest.py).
# =============================================================================

import argparse
import concurrent.futures
import json
import os
import random
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoketest  # noqa: E402  (reuses the config/client helpers)


# ---------------------------------------------------------------------------
# Storm (concurrent clients)
# ---------------------------------------------------------------------------

# One cycle of the mixed command load a worker fires repeatedly. All of these
# must exit 0 on a healthy daemon.
STORM_COMMANDS = [
    ["st"],
    ["--json", "st"],
    ["list"],
    ["--json", "list"],
    ["daemon", "on"],
    ["1"],
    ["0"],
    ["scr", "1"],
    ["scr", "0"],
]


def storm_worker(worker_id, cfg, iterations):
    """Fires the mixed command cycle; returns a list of failure strings."""
    failures = []
    for i in range(iterations):
        args = STORM_COMMANDS[(worker_id + i) % len(STORM_COMMANDS)]
        try:
            code, out = smoketest.run_client(cfg, args)
            if code != 0:
                # Carry the output: "exited 5" alone says nothing about what
                # went wrong, and the storm runs 200 commands in seconds, so
                # reproducing a single failure by hand is impractical.
                failures.append(
                    "worker %d iteration %d: awake %s exited %d; output: %s"
                    % (worker_id, i, " ".join(args), code,
                       (" ".join(out.split())[:200] or "(empty)")))
        except Exception as exc:  # timeouts, launch failures
            failures.append("worker %d iteration %d: %s raised %r"
                            % (worker_id, i, " ".join(args), exc))
    return failures


def run_storm(cfg, processes, requests):
    """Runs the concurrent storm. Returns (failures, total_requests)."""
    print("[storm] %d workers x %d requests ..." % (processes, requests))
    started = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=processes) as pool:
        futures = [pool.submit(storm_worker, w, cfg, requests)
                   for w in range(processes)]
        failures = []
        for future in futures:
            failures.extend(future.result())
    elapsed = time.time() - started
    total = processes * requests
    print("[storm] done in %.1fs (%d requests)" % (elapsed, total))
    return failures, total


# ---------------------------------------------------------------------------
# Fuzz (raw pipe attacks, one PowerShell connection per round)
# ---------------------------------------------------------------------------

# Verbs that are safe to send in malformed form: none of them can stop the
# daemon (SHUTDOWN is deliberately NOT in this list).
FUZZ_MODES = ["oversize", "garbage", "newline", "malformed", "fragment"]

FUZZ_MALFORMED = [
    "KEEP_AWAKE 9",
    "SCREEN yes",
    "GET_STATUS extra junk",
    "KEEP_AWAKE",
    "unknown verb",
    "   ",
    "",
]


def daemon_alive(cfg):
    """True when the daemon answers a --json status request."""
    code, out = smoketest.run_client_stdout(cfg, ["--json", "st"])
    try:
        return json.loads(out).get("daemon") == "running"
    except ValueError:
        return False


def fuzz_round(cfg, mode, rng):
    """Executes one randomized pipe attack as a single PowerShell script that
    opens the pipe, performs the writes and closes it."""
    seed = rng.randint(1, 1000000)

    if mode == "oversize":
        size = rng.randint(1024, 8192)
        body = "$b = New-Object byte[] %d; (New-Object Random(%d)).NextBytes($b);" % (size, seed)
    elif mode == "garbage":
        size = rng.randint(1, 512)
        body = "$b = New-Object byte[] %d; (New-Object Random(%d)).NextBytes($b);" % (size, seed)
    elif mode == "newline":
        # ASCII garbage WITH a trailing newline: the daemon parses and rejects it.
        body = "$b = New-Object byte[] 300; (New-Object Random(%d)).NextBytes($b); $b[299] = 10;" % seed
    elif mode == "malformed":
        verb = rng.choice(FUZZ_MALFORMED)
        body = "$b = [System.Text.Encoding]::ASCII.GetBytes('%s' + \"`n\");" % verb
    else:  # fragment: a valid PING split across two writes on one connection
        body = ("$p.Write([byte[]]@(0x50,0x49),0,2); "
                "Start-Sleep -Milliseconds 50; "
                "$p.Write([byte[]]@(0x4E,0x47,0x0A),0,3);")

    script = (
        "$p = new-object System.IO.Pipes.NamedPipeClientStream('.', '%s', "
        "[System.IO.Pipes.PipeDirection]::InOut); "
        "$p.Connect(2000); "
        "%s "
        "$p.Flush(); $p.Close();"
    ) % (cfg["pipe_name"], body)
    subprocess.run(["powershell", "-NoProfile", "-Command", script],
                   capture_output=True, timeout=cfg["command_timeout_s"])


def run_fuzz(cfg, rounds, rng):
    """Runs the fuzz rounds; returns a list of failure strings."""
    failures = []
    for round_index in range(rounds):
        mode = FUZZ_MODES[round_index % len(FUZZ_MODES)]
        fuzz_round(cfg, mode, rng)
        if not daemon_alive(cfg):
            # The symptom alone ("stopped serving") does not say why; the last
            # command against the daemon usually does.
            failures.append("fuzz round %d (%s): daemon stopped serving; %s"
                            % (round_index, mode,
                               smoketest.describe_last_command()))
            print("[fuzz] FAIL in round %d (%s)" % (round_index, mode))
            break
        print("[fuzz] round %d/%d (%s): daemon alive" % (round_index + 1, rounds, mode))
    return failures


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Concurrency storm and pipe fuzzing against the running daemon.")
    parser.add_argument("--config", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "smoketest.ini"))
    parser.add_argument("--processes", type=int, default=8)
    parser.add_argument("--requests", type=int, default=25)
    parser.add_argument("--fuzz-rounds", type=int, default=50)
    parser.add_argument("--skip-storm", action="store_true")
    parser.add_argument("--skip-fuzz", action="store_true")
    args = parser.parse_args()

    cfg = smoketest.load_config(args.config)
    if not os.path.isfile(cfg["client_path"]):
        raise SystemExit("ERROR: executables not found in %s - build first."
                         % cfg["release_dir"])

    failures = []

    code, _ = smoketest.run_client(cfg, ["daemon", "on"])
    if code != 0:
        raise SystemExit("ERROR: the daemon did not start.")

    rng = random.Random()
    if not args.skip_storm:
        stormFailures, total = run_storm(cfg, args.processes, args.requests)
        failures.extend(stormFailures)
        label = "storm: %d concurrent requests served" % total
        print("[PASS] %s" % label if not stormFailures else "[FAIL] %s" % label)

    if not args.skip_fuzz:
        fuzzFailures = run_fuzz(cfg, args.fuzz_rounds, rng)
        failures.extend(fuzzFailures)
        label = "fuzz: %d rounds survived" % args.fuzz_rounds
        print("[PASS] %s" % label if not fuzzFailures else "[FAIL] %s" % label)

    # Final sanity: the daemon must still be fully functional.
    code, out = smoketest.run_client(cfg, ["st"])
    if code != 0 or "Daemon:" not in out:
        # This is the failure a reader is most likely to hit, so it carries
        # the command output rather than just announcing that it failed.
        failures.append("final status check failed: awake st -> exit %d; output: %s"
                        % (code, (" ".join(out.split())[:200] or "(empty)")))

    print("")
    print("stress summary: %d failure(s)" % len(failures))
    for failure in failures:
        print("  - %s" % failure)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
