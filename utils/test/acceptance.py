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
# acceptance.py - R16 release acceptance orchestrator (three tiers).
#
# Usage:
#   python utils/test/acceptance.py --tier auto|semi|manual|all
#                                   [--output build/verification] [--config ...]
#
#   auto    everything that runs unattended: version consistency, unit tests,
#           the smoke test, stress (storm + fuzz), F1 expiry precision, watch
#           list churn, log rotation pruning and the --json schema.
#   semi    script-driven checks that need the environment: autostart round
#           trip, log export zip validity, powercfg cross-check (admin only)
#           and the interactive sleep/resume check.
#   manual  prints utils/test/manual-checklist.md - the items that only a
#           human can verify.
#   all     runs auto, then semi, then prints the manual checklist.
#
# Results are written to <output>/results.json and <output>/report.md.
# The R16 gate for 1.0.0: auto and semi fully green plus every item of the
# manual checklist ticked (see manual-checklist.md).
# =============================================================================

import argparse
import ctypes
import datetime
import glob
import json
import os
import platform
import re
import subprocess
import sys
import tempfile
import time
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoketest  # noqa: E402
import stress  # noqa: E402

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

class Report:
    def __init__(self, outputDir, tier):
        self.outputDir = outputDir
        self.tier = tier
        self.checks = []  # {tier, name, status, detail}

    def record(self, tier, name, status, detail=""):
        self.checks.append({"tier": tier, "name": name,
                            "status": status, "detail": detail})
        marker = {"PASS": "[PASS]", "FAIL": "[FAIL]", "SKIP": "[SKIP]",
                  "WARN": "[WARN]"}[status]
        print("%s [%s] %s%s" % (marker, tier, name,
                                (" - " + detail) if detail else ""))

    def passed(self, tier, name, detail=""):
        self.record(tier, name, "PASS", detail)

    def failed(self, tier, name, detail=""):
        self.record(tier, name, "FAIL", detail)

    def skipped(self, tier, name, detail=""):
        self.record(tier, name, "SKIP", detail)

    def anomaly(self, text):
        self.checks.append({"tier": "-", "name": text, "status": "WARN", "detail": ""})

    def failCount(self):
        return sum(1 for c in self.checks if c["status"] == "FAIL")

    def write(self, cfg, version):
        os.makedirs(self.outputDir, exist_ok=True)
        results = {
            "generatedEpoch": time.time(),
            "generated": datetime.datetime.now().isoformat(),
            "host": platform.node(),
            "os": platform.platform(),
            "version": version,
            "tier": self.tier,
            "gate": "1.0.0 requires: auto + semi green, manual checklist signed off",
            "checks": self.checks,
        }
        jsonPath = os.path.join(self.outputDir, "results.json")
        with open(jsonPath, "w", encoding="ascii") as file:
            json.dump(results, file, indent=2)
            file.write("\n")

        lines = ["# awake R16 acceptance report",
                 "",
                 "- Generated: %s" % results["generated"],
                 "- Version: %s" % version,
                 "- Tier: %s" % self.tier,
                 "- Gate: %s" % results["gate"],
                 "",
                 "| Tier | Check | Result | Detail |",
                 "|---|---|---|---|"]
        for check in self.checks:
            lines.append("| %s | %s | %s | %s |"
                         % (check["tier"], check["name"], check["status"],
                            check["detail"].replace("|", "\\|")))
        lines.append("")
        reportPath = os.path.join(self.outputDir, "report.md")
        with open(reportPath, "w", encoding="ascii") as file:
            file.write("\n".join(lines))
        return jsonPath, reportPath


def read_declared_version():
    """The version declared in CMakeLists.txt (R8 single source)."""
    with open(os.path.join(REPO_ROOT, "CMakeLists.txt"), encoding="ascii") as file:
        content = file.read()
    version = re.search(r"project\(awake VERSION (\d+\.\d+\.\d+)", content).group(1)
    suffix = re.search(r'set\(AWAKE_VERSION_SUFFIX "([^"]*)"\)', content).group(1)
    return version + suffix


def is_elevated():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin() != 0
    except Exception:
        return False


# ---------------------------------------------------------------------------
# Tier: auto
# ---------------------------------------------------------------------------

def tier_auto(report, cfg):
    tier = "auto"

    # 1. Executables exist.
    if not (os.path.isfile(cfg["client_path"]) and os.path.isfile(cfg["daemon_path"])):
        report.failed(tier, "executables exist", cfg["release_dir"])
        return
    report.passed(tier, "executables exist")

    # 2. Version consistency (R8 guard): binary vs CMakeLists.txt.
    declared = read_declared_version()
    code, out = smoketest.run_client(cfg, ["version"])
    reported = out.replace("\n", " ").strip()
    if code == 0 and declared in reported:
        report.passed(tier, "version consistency", reported)
    else:
        report.failed(tier, "version consistency",
                      "declared %s, reported %s" % (declared, reported))

    # 3. Unit tests (locate the Catch2 binary anywhere under build/).
    testExe = None
    buildDir = os.path.join(REPO_ROOT, "build")
    for root, _, files in os.walk(buildDir):
        if "awake_tests.exe" in files:
            testExe = os.path.join(root, "awake_tests.exe")
            break
    if testExe is None:
        report.skipped(tier, "unit tests", "awake_tests.exe not built")
    else:
        result = subprocess.run([testExe], capture_output=True, text=True)
        summary = result.stdout.strip().splitlines()[-1] if result.stdout else ""
        if result.returncode == 0:
            report.passed(tier, "unit tests", summary)
        else:
            report.failed(tier, "unit tests", summary)

    # 4. Smoke test (as a subprocess - it mutates state heavily). Its own
    # helpers are timeout-resilient, so a wedged daemon degrades to FAILs
    # instead of a crash; the detail carries stderr for the traceback.
    result = subprocess.run([sys.executable, os.path.join(SCRIPT_DIR, "smoketest.py")],
                            capture_output=True, text=True)
    match = re.search(r"Summary: (\d+) passed, (\d+) failed", result.stdout)
    if match and int(match.group(2)) == 0:
        report.passed(tier, "smoke test",
                      "%s passed, %s failed" % (match.group(1), match.group(2)))
    else:
        detail = result.stderr.strip()[-200:] or result.stdout.strip()[-200:]
        report.failed(tier, "smoke test",
                      (match.group(0) if match else detail))

    # 5. Stress (storm + fuzz) as a subprocess - isolated crash domain.
    result = subprocess.run([sys.executable, os.path.join(SCRIPT_DIR, "stress.py"),
                             "--processes", "8", "--requests", "25",
                             "--fuzz-rounds", "25"],
                            capture_output=True, text=True)
    if result.returncode == 0:
        report.passed(tier, "stress (storm + fuzz)")
    else:
        report.failed(tier, "stress (storm + fuzz)", result.stdout[-300:])

    # 6. F1 expiry precision: 10 timed cycles, measured externally.
    latencies = []
    precisionFailures = 0
    for cycle in range(10):
        try:
            started = time.time()
            code, _ = smoketest.run_client(cfg, ["1", "for", "3s"])
            if code != 0:
                precisionFailures += 1
                continue
            releasedAt = None
            deadline = started + 3 + 2.5 + 5
            while time.time() < deadline:
                code, out = smoketest.run_client_stdout(cfg, ["--json", "st"])
                try:
                    if not json.loads(out)["keepAwake"]["active"]:
                        releasedAt = time.time()
                        break
                except (ValueError, KeyError):
                    pass
                time.sleep(0.2)
            if releasedAt is None:
                precisionFailures += 1
                continue
            latencies.append(releasedAt - started)
        except subprocess.TimeoutExpired:
            # A wedged daemon must not crash the acceptance run (R16 lesson).
            precisionFailures += 1
            report.anomaly("F1 cycle %d: client timed out (daemon wedged?)" % cycle)
    if latencies and precisionFailures == 0 and min(latencies) >= 2.9 \
            and max(latencies) <= 5.5:
        report.passed(tier, "F1 expiry precision (10 cycles)",
                      "min %.2fs / avg %.2fs / max %.2fs"
                      % (min(latencies), sum(latencies) / len(latencies), max(latencies)))
    else:
        report.failed(tier, "F1 expiry precision (10 cycles)",
                      "failures %d, latencies %s" % (precisionFailures, latencies))

    # 7. Watch list churn: 100 add/del round trips with case variants.
    churnFailures = 0
    for index in range(100):
        name = "churn-%d.exe" % index
        if not smoketest.run_client(cfg, ["add", name])[0] == 0:
            churnFailures += 1
        if not smoketest.run_client(cfg, ["del", "CHURN-%d.EXE" % index])[0] == 0:
            churnFailures += 1
    entries = smoketest.run_client_stdout(cfg, ["--json", "list"])[1]
    if churnFailures == 0 and "churn-" not in entries:
        report.passed(tier, "watch list churn (100 add/del round trips)")
    else:
        report.failed(tier, "watch list churn (100 add/del round trips)",
                      "%d failures" % churnFailures)

    # 8. Log rotation pruning: fake old logs + max-files 2 -> prune to 2.
    logDir = os.path.join(os.environ.get("LOCALAPPDATA",
                                         os.path.join(tempfile.gettempdir(), "fallback")),
                          "awake", "logs", "daemon")
    os.makedirs(logDir, exist_ok=True)
    for old in glob.glob(os.path.join(logDir, "daemon-*.log")):
        pass  # real logs stay; the fake ones use a distinct old-date name
    fakeCount = 4
    for day in range(1, fakeCount + 1):
        with open(os.path.join(logDir, "daemon-2020010%d.log" % day), "w",
                  encoding="ascii") as file:
            file.write("fake old log for the pruning test\n")
    smoketest.run_client(cfg, ["log", "on"])
    smoketest.run_client(cfg, ["log", "max-files", "2"])
    smoketest.run_client(cfg, ["reload"])
    remaining = len(glob.glob(os.path.join(logDir, "daemon-*.log")))
    smoketest.run_client(cfg, ["log", "max-files", "10"])
    smoketest.run_client(cfg, ["log", "off"])
    if remaining <= 2:
        report.passed(tier, "log rotation pruning (max-files 2)",
                      "%d files remain" % remaining)
    else:
        report.failed(tier, "log rotation pruning (max-files 2)",
                      "%d files remain" % remaining)

    # 9. --json schema: every known key with the right type.
    code, out = smoketest.run_client_stdout(cfg, ["--json", "st"])
    try:
        parsed = json.loads(out)
        required = {
            "daemon": str, "elevated": bool,
            "keepAwake": dict, "screen": dict, "resumeAgoSeconds": int,
            "autostart": str, "powerPlan": dict, "requests": str, "watch": list,
        }
        missing = [key for key in required if key not in parsed]
        wrong = [key for key, kind in required.items()
                 if key in parsed and not isinstance(parsed[key], kind)]
        if missing or wrong:
            report.failed(tier, "--json schema", "missing %s wrong %s" % (missing, wrong))
        else:
            report.passed(tier, "--json schema")
    except ValueError as exc:
        report.failed(tier, "--json schema", repr(exc))


# ---------------------------------------------------------------------------
# Tier: semi (script-driven, may need user interaction)
# ---------------------------------------------------------------------------

def tier_semi(report, cfg):
    tier = "semi"

    # 1. Autostart round trip through the real registry.
    codeOn, outOn = smoketest.run_client(cfg, ["daemon", "enable"],
                                         timeout_s=cfg["autostart_timeout_s"])
    registered = smoketest.autostart_value_exists(cfg)
    codeOff, _ = smoketest.run_client(cfg, ["daemon", "disable"],
                                      timeout_s=cfg["autostart_timeout_s"])
    gone = not smoketest.autostart_value_exists(cfg)
    if codeOn == 0 and codeOff == 0 and registered and gone:
        report.passed(tier, "autostart registry round trip")
    else:
        report.failed(tier, "autostart registry round trip",
                      "on=%d reg=%d off=%d gone=%d" % (codeOn, registered, codeOff, gone))

    # 2. Log export zip validity.
    smoketest.run_client(cfg, ["log", "on"])
    smoketest.run_client(cfg, ["daemon", "on"])
    zipPath = os.path.join(tempfile.gettempdir(), "awake-acceptance-logs.zip")
    if os.path.isfile(zipPath):
        os.remove(zipPath)
    code, _ = smoketest.run_client(cfg, ["log", "export", zipPath])
    valid = os.path.isfile(zipPath) and zipfile.is_zipfile(zipPath)
    entries = 0
    if valid:
        with zipfile.ZipFile(zipPath) as archive:
            entries = len(archive.namelist())
        os.remove(zipPath)
    smoketest.run_client(cfg, ["log", "off"])
    if code == 0 and valid and entries >= 1:
        report.passed(tier, "log export zip validity", "%d entries" % entries)
    else:
        report.failed(tier, "log export zip validity", "code %d, valid %s" % (code, valid))

    # 3. powercfg cross-check (needs admin; the runner/terminal decides).
    smoketest.run_client(cfg, ["daemon", "on"])
    smoketest.run_client(cfg, ["1"])
    time.sleep(1)
    if is_elevated():
        result = subprocess.run(["powercfg", "/requests"], capture_output=True,
                                text=True)
        output = result.stdout or ""
        if "awake.daemon.exe" in output:
            report.passed(tier, "powercfg /requests shows the daemon")
        else:
            report.failed(tier, "powercfg /requests shows the daemon",
                          output[:200])
    else:
        report.skipped(tier, "powercfg /requests cross-check",
                       "needs an elevated terminal (covered by the manual checklist)")
    smoketest.run_client(cfg, ["0"])

    # 4. Interactive sleep/resume check.
    smoketest.run_client(cfg, ["daemon", "on"])
    smoketest.run_client(cfg, ["1"])
    print("")
    print("[semi] Please put the machine to sleep and wake it up again.")
    try:
        input("[semi] After the wake-up, press Enter to verify ... ")
        verified = True
    except EOFError:
        verified = False
    code, out = smoketest.run_client_stdout(cfg, ["--json", "st"])
    try:
        parsed = json.loads(out)
        resumeOk = parsed.get("resumeAgoSeconds", -1) >= 0
        stateOk = parsed["keepAwake"]["active"] is True
    except (ValueError, KeyError):
        resumeOk = stateOk = False
    if verified and resumeOk and stateOk:
        report.passed(tier, "sleep/resume re-apply (interactive)")
    else:
        report.failed(tier, "sleep/resume re-apply (interactive)",
                      "verified %s resumeOk %s stateOk %s" % (verified, resumeOk, stateOk))
    smoketest.run_client(cfg, ["0"])


# ---------------------------------------------------------------------------
# Tier: manual
# ---------------------------------------------------------------------------

def tier_manual(report):
    checklist = os.path.join(SCRIPT_DIR, "manual-checklist.md")
    if os.path.isfile(checklist):
        report.passed("manual", "checklist available", checklist)
        with open(checklist, encoding="ascii") as file:
            print("")
            print(file.read())
    else:
        report.failed("manual", "checklist available", checklist + " is missing")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="R16 release acceptance orchestrator (three tiers).")
    parser.add_argument("--tier", choices=["auto", "semi", "manual", "all"],
                        default="all")
    parser.add_argument("--config", default=os.path.join(SCRIPT_DIR, "smoketest.ini"))
    parser.add_argument("--output", default=os.path.join(REPO_ROOT, "build", "verification"))
    args = parser.parse_args()

    cfg = smoketest.load_config(args.config)
    report = Report(args.output, args.tier)

    # The acceptance run needs the license gate out of the way.
    smoketest.ensure_license_accepted(cfg)

    if args.tier in ("auto", "all"):
        tier_auto(report, cfg)
    if args.tier in ("semi", "all"):
        tier_semi(report, cfg)
    if args.tier in ("manual", "all"):
        tier_manual(report)

    version = read_declared_version()
    jsonPath, reportPath = report.write(cfg, version)

    failures = report.failCount()
    print("")
    print("acceptance: %d check(s), %d failure(s)" % (len(report.checks), failures))
    print("results : %s" % jsonPath)
    print("report  : %s" % reportPath)
    print("R16 gate: auto + semi green and the manual checklist signed off "
          "are required for 1.0.0.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
