# Security Policy

## Supported versions

Only the latest release line receives security fixes. This project is in
beta; older beta tags are not supported.

| Version    | Supported |
|------------|-----------|
| 0.3.0-beta | yes       |
| < 0.3.0    | no        |

## Reporting a vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

Use GitHub's "Report a vulnerability" feature (Security tab of the
repository) to submit a private security advisory. Include:

- the affected version/commit,
- steps to reproduce or a proof of concept,
- the impact you estimate.

You will get a response as soon as possible. Once a fix is available, the
advisory will be published and the fix released in the next version.

## Known design boundaries

- The named pipe `\\.\pipe\awake_daemon` uses default (creator-user) ACLs
  and is limited to one instance; requests larger than 1 KB without a
  newline are dropped. The pipe only accepts well-formed ASCII commands.
- The daemon launch token (`--internal-daemon`) is a convenience guard
  against accidental direct invocation, **not** a security boundary - it is
  compiled into the publicly available client.
- The autostart feature only writes `HKCU\...\Run`; it never elevates.

## AI-generated code notice

This codebase was generated with AI assistance and may contain unnoticed
flaws. Extra care when reviewing security-relevant code (named pipe server,
process scanning, registry access) is strongly recommended - see `NOTICE`.
