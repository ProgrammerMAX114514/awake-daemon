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
# smoketest.py - semi-automated smoke test for awake (client + daemon).
#
# Usage (from anywhere):
#   python utils/test/smoketest.py
#   python utils/test/smoketest.py --config path/to/smoketest.ini
#
# What it does:
#   Runs every command of awake.exe against the built Release executables
#   and checks the observable behavior (exit codes and console output,
#   including the command aliases). It also touches real side effects -
#   the watch list in awake.ini, the registry autostart entry and a real
#   test application - so run it on a machine where that is acceptable.
#
# What stays manual (printed as a checklist at the end):
#   - the actual on-screen colors (script output is captured, not displayed),
#   - powercfg /requests cross-verification (needs an elevated terminal),
#   - whether the daemon really starts at logon (needs a logoff/logon).
#
# The script is intentionally dependency-free (Python 3.8+ stdlib only) and
# driven by the INI file next to it, so test data and locations can be
# adapted without touching the code.
# =============================================================================

import argparse
import configparser
import os
import subprocess
import sys
import time
import winreg

# ---------------------------------------------------------------------------
# Locations and configuration
# ---------------------------------------------------------------------------

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def load_config(path):
    """Loads the INI configuration and returns a dict of flattened options."""
    parser = configparser.ConfigParser()
    if not parser.read(path, encoding="ascii"):
        raise SystemExit("ERROR: config file not found: %s" % path)
    cfg = {
        "release_dir": os.path.abspath(os.path.join(REPO_ROOT, parser.get("paths", "release_dir"))),
        "client_exe": parser.get("paths", "client_exe"),
        "daemon_exe": parser.get("paths", "daemon_exe"),
        "test_app": parser.get("watch", "test_app"),
        "keep_window_open": parser.getboolean("watch", "keep_window_open"),
        "registry_key": parser.get("autostart", "registry_key"),
        "value_name": parser.get("autostart", "value_name"),
        "restore_state": parser.getboolean("autostart", "restore_state"),
        "command_timeout_s": parser.getint("timeouts", "command_timeout_s"),
    }
    cfg["client_path"] = os.path.join(cfg["release_dir"], cfg["client_exe"])
    cfg["daemon_path"] = os.path.join(cfg["release_dir"], cfg["daemon_exe"])
    return cfg


# ---------------------------------------------------------------------------
# Test runner helpers
# ---------------------------------------------------------------------------

PASS = 0
FAIL = 0


def run_client(cfg, args, stdin_text=None):
    """Runs awake.exe with the given arguments and returns (exit_code, output)."""
    result = subprocess.run(
        [cfg["client_path"]] + args,
        input=stdin_text,
        capture_output=True,
        text=True,
        timeout=cfg["command_timeout_s"],
    )
    return result.returncode, (result.stdout or "") + (result.stderr or "")


def check(name, condition, detail=""):
    """Records and prints one test verdict."""
    global PASS, FAIL
    if condition:
        PASS += 1
        print("[PASS] %s" % name)
    else:
        FAIL += 1
        print("[FAIL] %s%s" % (name, (" - " + detail) if detail else ""))


def autostart_value_exists(cfg):
    """Returns True when the autostart registry value currently exists."""
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, cfg["registry_key"], 0, winreg.KEY_QUERY_VALUE) as key:
            winreg.QueryValueEx(key, cfg["value_name"])
            return True
    except OSError:
        return False


def kill_test_app(cfg):
    """Kills the test application (best effort)."""
    subprocess.run(["taskkill", "/IM", cfg["test_app"], "/F"],
                   capture_output=True, timeout=cfg["command_timeout_s"])


# ---------------------------------------------------------------------------
# Test steps (each roughly maps to one user-visible behavior)
# ---------------------------------------------------------------------------

def test_version_and_help(cfg):
    code, out = run_client(cfg, ["version"])
    check("awake version prints the version", code == 0 and "awake 0." in out, out.strip())

    code, out = run_client(cfg, ["help"])
    check("awake help shows the version and usage",
          code == 0 and "keep the system awake" in out and "Usage:" in out, out.strip())

    code, out = run_client(cfg, ["h"])
    check("alias 'h' shows help", code == 0 and "Usage:" in out)
    code, out = run_client(cfg, ["?"])
    check("alias '?' shows help", code == 0 and "Usage:" in out)

    code, out = run_client(cfg, ["definitely-not-a-command"])
    check("unknown command fails with a hint",
          code == 1 and "unknown command" in out.lower())


def test_status_without_daemon(cfg):
    # The daemon is expected to be stopped here (the script stops it first).
    code, out = run_client(cfg, ["status"])
    check("status without daemon reports not running",
          code == 0 and "not running" in out and "INACTIVE" in out and "Autostart:" in out, out.strip())


def test_daemon_lifecycle(cfg):
    code, out = run_client(cfg, ["daemon", "off"])
    check("daemon off is idempotent when not running",
          code == 0 and "not running" in out.lower(), out.strip())

    code, out = run_client(cfg, ["daemon", "on"])
    check("daemon on starts the daemon", code == 0 and "now running" in out, out.strip())

    code, out = run_client(cfg, ["daemon", "on"])
    check("daemon on is idempotent when running",
          code == 0 and "already running" in out.lower(), out.strip())

    code, out = run_client(cfg, ["st"])
    check("status shows the daemon running", code == 0 and "Daemon:" in out and "running" in out)


def test_keep_awake(cfg):
    code, out = run_client(cfg, ["1"])
    check("awake 1 enables keep-awake", code == 0 and "ENABLED" in out, out.strip())

    code, out = run_client(cfg, ["st"])
    check("status reports keep-awake ENABLED (manually set)",
          code == 0 and "ENABLED" in out and "manually set" in out, out.strip())

    code, out = run_client(cfg, ["0"])
    check("awake 0 disables keep-awake", code == 0 and "DISABLED" in out, out.strip())

    # Aliases must reach the same functionality.
    code, out = run_client(cfg, ["on"])
    check("alias 'on' enables keep-awake", code == 0 and "ENABLED" in out)
    code, out = run_client(cfg, ["disable"])
    check("alias 'disable' disables keep-awake", code == 0 and "DISABLED" in out)
    code, out = run_client(cfg, ["stop"])
    check("alias 'stop' disables keep-awake", code == 0 and "DISABLED" in out)


def test_screen_keep_awake(cfg):
    code, out = run_client(cfg, ["scr", "1"])
    check("alias 'scr 1' enables screen keep-awake",
          code == 0 and "Screen keep-awake ENABLED" in out, out.strip())

    code, out = run_client(cfg, ["st"])
    check("status reports screen ENABLED",
          code == 0 and "Screen:" in out and "ENABLED" in out)

    code, out = run_client(cfg, ["scr", "0"])
    check("alias 'scr 0' disables screen keep-awake",
          code == 0 and "Screen keep-awake DISABLED" in out)


def test_watch_list(cfg):
    app = cfg["test_app"]

    code, out = run_client(cfg, ["add", app])
    check("add registers the test app", code == 0 and "Added" in out, out.strip())

    code, out = run_client(cfg, ["add", app])
    check("add is idempotent (duplicate rejected)",
          code == 0 and "already" in out.lower(), out.strip())

    code, out = run_client(cfg, ["st"])
    check("status shows the entry as not running",
          code == 0 and app in out and "not running" in out, out.strip())

    # Start the real test app: a window may flash - the semi-automatic part.
    print("       starting %s (a window may appear briefly)" % app)
    proc = subprocess.Popen(["cmd", "/c", "start", "", app],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(2)  # give the process a moment to appear in the process list

    code, out = run_client(cfg, ["st"])
    check("status shows the entry as running", code == 0 and "(running)" in out, out.strip())

    if not cfg["keep_window_open"]:
        kill_test_app(cfg)
        time.sleep(1)
    else:
        print("       keep_window_open=true: leave %s running for manual checks" % app)

    code, out = run_client(cfg, ["del", app])
    check("del removes the entry", code == 0 and "Removed" in out, out.strip())

    code, out = run_client(cfg, ["del", app])
    check("del is idempotent (missing entry reported)",
          code == 0 and "not found" in out.lower(), out.strip())


def test_reload_and_reset(cfg):
    code, out = run_client(cfg, ["reload"])
    check("reload is acknowledged by the running daemon",
          code == 0 and "reloaded" in out.lower(), out.strip())

    code, out = run_client(cfg, ["add", "smoketest-placeholder.exe"])
    code, out = run_client(cfg, ["reset"], stdin_text="n\n")
    check("reset aborts on 'n'", code == 0 and "Aborted" in out, out.strip())

    code, out = run_client(cfg, ["reset"], stdin_text="y\n")
    check("reset proceeds on 'y'", code == 0 and "reset to its initial state" in out, out.strip())

    code, out = run_client(cfg, ["st"])
    check("watch list is empty after reset", "(empty" in out)


def test_autostart(cfg):
    code, out = run_client(cfg, ["daemon", "enable"])
    check("daemon enable registers autostart",
          code == 0 and "ENABLED" in out, out.strip())
    check("registry value exists after enable", autostart_value_exists(cfg))

    code, out = run_client(cfg, ["daemon", "enable"])
    check("daemon enable is idempotent",
          code == 0 and "already enabled" in out.lower(), out.strip())

    code, out = run_client(cfg, ["st"])
    check("status reports autostart ENABLED",
          code == 0 and "Autostart:" in out and "ENABLED" in out, out.strip())

    code, out = run_client(cfg, ["daemon", "disable"])
    check("daemon disable removes autostart",
          code == 0 and "DISABLED" in out, out.strip())
    check("registry value gone after disable", not autostart_value_exists(cfg))

    code, out = run_client(cfg, ["daemon", "disable"])
    check("daemon disable is idempotent",
          code == 0 and "already disabled" in out.lower(), out.strip())


def test_daemon_shutdown(cfg):
    code, out = run_client(cfg, ["d", "0"])  # alias: daemon off
    check("alias 'd 0' stops the daemon", code == 0 and "stopped" in out.lower(), out.strip())

    code, out = run_client(cfg, ["status"])
    check("status reports the daemon not running again",
          code == 0 and "not running" in out, out.strip())


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    arg_parser = argparse.ArgumentParser(description="Semi-automated smoke test for awake.")
    default_ini = os.path.join(os.path.dirname(os.path.abspath(__file__)), "smoketest.ini")
    arg_parser.add_argument("--config", default=default_ini, help="path to the INI config file")
    args = arg_parser.parse_args()

    cfg = load_config(args.config)

    if not os.path.isfile(cfg["client_path"]) or not os.path.isfile(cfg["daemon_path"]):
        raise SystemExit("ERROR: executables not found in %s - run utils\\build.bat first."
                         % cfg["release_dir"])

    print("awake smoke test")
    print("  release dir : %s" % cfg["release_dir"])
    print("  config      : %s" % args.config)
    print("")

    # Save and clear the environment the tests are about to touch.
    autostart_before = autostart_value_exists(cfg)
    run_client(cfg, ["daemon", "off"])  # start from a stopped daemon

    test_version_and_help(cfg)
    test_status_without_daemon(cfg)
    test_daemon_lifecycle(cfg)
    test_keep_awake(cfg)
    test_screen_keep_awake(cfg)
    test_watch_list(cfg)
    test_reload_and_reset(cfg)
    test_autostart(cfg)
    test_daemon_shutdown(cfg)

    # Restore the pre-test autostart state if requested.
    if cfg["restore_state"] and autostart_before and not autostart_value_exists(cfg):
        run_client(cfg, ["daemon", "enable"])
        print("[INFO] restored the pre-test autostart state")
    if not cfg["keep_window_open"]:
        kill_test_app(cfg)  # best-effort cleanup of any leftover test app

    print("")
    print("Summary: %d passed, %d failed" % (PASS, FAIL))

    print("")
    print("Manual checks (cannot be automated from a normal shell):")
    print("  1. Colors: labels are default-colored, only values are colored")
    print("     (run 'awake status' by hand to eyeball it).")
    print("  2. 'powercfg /requests' in an ELEVATED terminal shows awake.daemon.exe")
    print("     under DISPLAY/SYSTEM while 'awake 1' / 'awake screen on' is active.")
    print("  3. Logon autostart: run 'awake daemon enable', sign out/in and verify")
    print("     the daemon process is running afterwards ('awake status').")
    print("  4. Manual sleep (power button / Start menu) still works while keep-awake")
    print("     is enabled - idle sleep is blocked, manual sleep is not.")

    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
