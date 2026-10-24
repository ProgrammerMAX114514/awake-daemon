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
# soak.py - long-run stability verification for the running daemon (R16).
#
# Usage:
#   python utils/test/soak.py [--minutes 10] [--config ...] [--output ...]
#
# While it runs it continuously verifies that the daemon keeps behaving:
#   - every  5 s: a --json status snapshot must parse, report "running" and
#     carry the known schema keys;
#   - every 30 s: an F1 precision cycle - "awake 1 for 5s" and the wall time
#     until keepAwake.active flips is measured (must be within [5s, 7.5s]);
#   - every 60 s: a watch-list churn (add/del one entry);
#   - every 120 s: a fuzz round from stress.py (raw pipe attacks);
#   - every 30 s: the daemon's working set is sampled (memory growth trend).
#
# Problems are recorded and the run CONTINUES (a soak documents everything).
# At the end a summary is printed and soak-report.json is written next to
# the script's output directory. For the full R16 pass run --minutes 1440
# (24 hours) locally.
# =============================================================================

import argparse
import json
import os
import random
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoketest  # noqa: E402
import stress  # noqa: E402


class SoakReport:
    """Collects every observation of the run."""

    def __init__(self):
        self.startedEpoch = time.time()
        self.snapshots = 0
        self.jsonFailures = 0
        self.expiries = []          # measured F1 release latencies (seconds)
        self.expiryFailures = 0
        self.churnCycles = 0
        self.churnFailures = 0
        self.fuzzRounds = 0
        self.fuzzFailures = 0
        self.memorySamples = []     # working set in bytes
        self.anomalies = []         # human-readable problem list

    def anomaly(self, text):
        self.anomalies.append("[%s] %s" % (time.strftime("%H:%M:%S"), text))
        print("[ANOMALY] %s" % self.anomalies[-1])

    def to_dict(self):
        memory = {
            "first": self.memorySamples[0] if self.memorySamples else None,
            "last": self.memorySamples[-1] if self.memorySamples else None,
            "peak": max(self.memorySamples) if self.memorySamples else None,
            "samples": len(self.memorySamples),
        }
        if self.memorySamples and self.memorySamples[0] > 0:
            memory["growthPercent"] = round(
                100.0 * (self.memorySamples[-1] - self.memorySamples[0])
                / self.memorySamples[0], 1)
        expiry = {
            "cycles": len(self.expiries),
            "minSeconds": round(min(self.expiries), 2) if self.expiries else None,
            "maxSeconds": round(max(self.expiries), 2) if self.expiries else None,
            "avgSeconds": round(sum(self.expiries) / len(self.expiries), 2)
                          if self.expiries else None,
        }
        return {
            "startedEpoch": self.startedEpoch,
            "durationMinutes": round((time.time() - self.startedEpoch) / 60.0, 1),
            "snapshots": self.snapshots,
            "jsonFailures": self.jsonFailures,
            "expiry": expiry,
            "expiryFailures": self.expiryFailures,
            "churnCycles": self.churnCycles,
            "churnFailures": self.churnFailures,
            "fuzzRounds": self.fuzzRounds,
            "fuzzFailures": self.fuzzFailures,
            "memory": memory,
            "anomalies": self.anomalies,
        }


def sample_memory(cfg):
    """Returns the daemon's working set in bytes, or None."""
    script = ("Get-Process awake.daemon -ErrorAction SilentlyContinue | "
              "Select-Object -First 1 -ExpandProperty WorkingSet64")
    result = subprocess.run(["powershell", "-NoProfile", "-Command", script],
                            capture_output=True, text=True,
                            timeout=cfg["command_timeout_s"])
    try:
        return int(result.stdout.strip())
    except ValueError:
        return None


def expiry_cycle(cfg, report, periodSeconds=5, toleranceSeconds=2.5):
    """One F1 precision cycle: request keep-awake for periodSeconds and
    measure the wall time until it releases."""
    started = time.time()
    code, _ = smoketest.run_client(cfg, ["1", "for", "%ds" % periodSeconds])
    if code != 0:
        report.expiryFailures += 1
        report.anomaly("timed keep-awake request failed")
        return
    # Poll the JSON status until the keep-awake is released.
    deadline = time.time() + periodSeconds + toleranceSeconds + 5
    releasedAt = None
    while time.time() < deadline:
        code, out = smoketest.run_client_stdout(cfg, ["--json", "st"])
        try:
            parsed = json.loads(out)
            if not parsed["keepAwake"]["active"]:
                releasedAt = time.time()
                break
        except (ValueError, KeyError):
            pass
        time.sleep(0.25)
    if releasedAt is None:
        report.expiryFailures += 1
        report.anomaly("timed keep-awake did not release within the tolerance")
        return
    elapsed = releasedAt - started
    report.expiries.append(elapsed)
    if elapsed < periodSeconds:
        report.expiryFailures += 1
        report.anomaly("released too early: %.2fs (expected >= %ds)" % (elapsed, periodSeconds))
    elif elapsed > periodSeconds + toleranceSeconds:
        report.expiryFailures += 1
        report.anomaly("released too late: %.2fs (expected <= %.1fs)"
                       % (elapsed, periodSeconds + toleranceSeconds))


def churn_cycle(cfg, report, index):
    """One watch-list add/del round trip."""
    name = "soak-churn-%d.exe" % index
    codeAdd, _ = smoketest.run_client(cfg, ["add", name])
    entries = config_entries(cfg)
    codeDel, _ = smoketest.run_client(cfg, ["del", name])
    if codeAdd != 0 or codeDel != 0 or name not in entries:
        report.churnFailures += 1
        report.anomaly("watch churn round %d misbehaved" % index)
    report.churnCycles += 1


def config_entries(cfg):
    """The current watch list straight from awake.ini."""
    code, out = smoketest.run_client(cfg, ["--json", "list"])
    try:
        return json.loads(out)
    except ValueError:
        return []


def main():
    parser = argparse.ArgumentParser(
        description="Long-run stability verification for the running daemon.")
    parser.add_argument("--minutes", type=int, default=10,
                        help="soak duration (R16 full pass: 1440 = 24 hours)")
    parser.add_argument("--config", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "smoketest.ini"))
    parser.add_argument("--output", default=None,
                        help="directory for soak-report.json (default: build/verification)")
    parser.add_argument("--seed", type=int, default=None)
    args = parser.parse_args()

    cfg = smoketest.load_config(args.config)
    if not os.path.isfile(cfg["client_path"]):
        raise SystemExit("ERROR: executables not found in %s - build first."
                         % cfg["release_dir"])
    rng = random.Random(args.seed)

    outputDir = args.output or os.path.join(cfg["release_dir"], "verification")
    os.makedirs(outputDir, exist_ok=True)

    if smoketest.run_client(cfg, ["daemon", "on"])[0] != 0:
        raise SystemExit("ERROR: the daemon did not start.")

    report = SoakReport()
    firstMemory = sample_memory(cfg)
    if firstMemory:
        report.memorySamples.append(firstMemory)

    started = time.time()
    endAt = started + args.minutes * 60
    nextSnapshot = 0
    nextExpiry = started + 30
    nextChurn = started + 60
    nextFuzz = started + 120
    nextMemory = started + 30
    churnIndex = 0
    fuzzRng = random.Random(args.seed)

    print("[soak] running for %d minute(s); Ctrl+C aborts (partial report is kept)"
          % args.minutes)
    try:
        while time.time() < endAt:
            now = time.time()
            if now >= nextSnapshot:
                code, out = smoketest.run_client_stdout(cfg, ["--json", "st"])
                report.snapshots += 1
                try:
                    parsed = json.loads(out)
                    if parsed.get("daemon") != "running":
                        raise KeyError("daemon not running")
                except (ValueError, KeyError) as exc:
                    report.jsonFailures += 1
                    report.anomaly("status snapshot broken: %r" % exc)
                nextSnapshot = now + 5
                time.sleep(1)  # keep the loop readable
                continue
            if now >= nextExpiry:
                expiry_cycle(cfg, report)
                nextExpiry = time.time() + 30
                continue
            if now >= nextChurn:
                churn_cycle(cfg, report, churnIndex)
                churnIndex += 1
                nextChurn = time.time() + 60
                continue
            if now >= nextFuzz:
                stress.fuzz_round(cfg, rng.choice(stress.FUZZ_MODES), fuzzRng)
                report.fuzzRounds += 1
                if not stress.daemon_alive(cfg):
                    report.fuzzFailures += 1
                    report.anomaly("daemon stopped serving after a fuzz round")
                nextFuzz = time.time() + 120
                continue
            if now >= nextMemory:
                memory = sample_memory(cfg)
                if memory:
                    report.memorySamples.append(memory)
                nextMemory = time.time() + 30
                continue
            time.sleep(1)
    except KeyboardInterrupt:
        print("[soak] interrupted - writing the partial report")

    summary = report.to_dict()
    reportPath = os.path.join(outputDir, "soak-report.json")
    with open(reportPath, "w", encoding="ascii") as file:
        json.dump(summary, file, indent=2)
        file.write("\n")

    print("")
    print("[soak] duration   : %.1f min" % summary["durationMinutes"])
    print("[soak] snapshots  : %d (json failures: %d)"
          % (summary["snapshots"], summary["jsonFailures"]))
    print("[soak] expiry     : %s (failures: %d)"
          % (summary["expiry"], summary["expiryFailures"]))
    print("[soak] churn      : %d cycles (failures: %d)"
          % (summary["churnCycles"], summary["churnFailures"]))
    print("[soak] fuzz       : %d rounds (failures: %d)"
          % (summary["fuzzRounds"], summary["fuzzFailures"]))
    print("[soak] memory     : %s" % summary["memory"])
    print("[soak] anomalies  : %d" % len(summary["anomalies"]))
    print("[soak] report     : %s" % reportPath)

    failed = (summary["jsonFailures"] + summary["expiryFailures"] +
              summary["churnFailures"] + summary["fuzzFailures"] +
              len(summary["anomalies"]))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
