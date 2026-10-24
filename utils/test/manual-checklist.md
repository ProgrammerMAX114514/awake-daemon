# awake R16 Manual Checklist

> **Release gate for 1.0.0**: every item below ticked AND
> `python utils\test\acceptance.py --tier all` fully green
> (see `build\verification\report.md`).
>
> Fill in the sign-off block at the bottom when done.

Environment used for this pass:

- awake version: ______________
- Date: ______________
- Machine / Windows version: ______________

## 1. Deployment

- [ ] Clean-machine deployment: copy only `awake.exe` + `awake.daemon.exe`
      into an empty folder; `awake daemon on` works; `awake.ini` is created
      with the comment header; the first command shows the license prompt
      and `awake version` works before acceptance.

## 2. Elevation matrix (a daemon and a client in every combination)

| Client \ Daemon | Non-elevated | Elevated |
|---|---|---|
| **Non-elevated** | [ ] works | [ ] client warns, cannot control daemon |
| **Elevated** | [ ] works | [ ] works, status shows `(elevated)` |

- [ ] `awake status` shows the `(elevated)` marker for an elevated daemon.

## 3. Power behavior

- [ ] `awake 1` blocks idle sleep for at least one full idle period
      (verified against the configured power plan timeout).
- [ ] `awake 0` releases it; the system sleeps again on idle.
- [ ] `awake screen on` blocks screen-off, screensaver and idle lock;
      `awake screen off` releases them.
- [ ] Manual sleep (power button / Start menu) is NEVER blocked while
      keep-awake or screen keep-awake is on.
- [ ] Elevated terminal: `powercfg /requests` shows `awake.daemon.exe`
      under SYSTEM (keep-awake on) and DISPLAY (screen keep-awake on).

## 4. Real sleep / resume

- [ ] Put the machine to sleep and wake it while `awake 1` is on:
      `awake status` shows a `Last resume` line, keep-awake is still on and
      `powercfg /requests` still shows the request.
- [ ] Repeat with `awake screen on`: the screen turns back on after the
      wake and the screensaver stays blocked.

## 5. Logon autostart

- [ ] `awake daemon enable`, then sign out and back in: the daemon process
      is running (`awake status` shows running) with keep-awake inactive.
- [ ] `awake daemon disable` removes the autostart; after another
      sign-out/in the daemon is not running.

## 6. Daemon logging

- [ ] `awake log on`, run a few commands, `awake log export`: the zip opens,
      contains the `daemon-*.log` files and the lines have plausible
      timestamps.
- [ ] Log rotation: with `awake log max-files 2` the number of
      `daemon-*.log` files never exceeds 2 over several rotations.
- [ ] `awake log off` stops the file growth.

## 7. Long-run report review

- [ ] `utils\test\soak.py --minutes 1440` completed; review
      `soak-report.json`: no anomalies, no memory growth trend
      (> 50 % flagged), F1 expiry statistics inside the tolerance.

## 8. Output modes

- [ ] `awake status` renders colors correctly (labels default, values
      colored) in Windows Terminal and in the classic console.
- [ ] `awake --no-color st` and `NO_COLOR=1` produce plain text.
- [ ] `awake --json st` / `awake list --json` output parses as JSON.

## Sign-off

- [ ] `python utils\test\acceptance.py --tier all` fully green
      (attach `build\verification\report.md`).
- [ ] All items above ticked.

Reviewed by: ______________  Date: ______________
