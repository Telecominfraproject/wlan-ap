# detect-wsl Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | grep, mount, uname, (optional) powershell.exe |

## Overview

The `detect-wsl` skill determines whether the current Linux environment is running inside Windows Subsystem for Linux. It distinguishes WSL1 from WSL2, supports three output modes (silent, verbose, JSON), and uses seven independent detection methods for reliability. The script requires no root privileges and performs only read-only, non-invasive checks.

**Primary users**: Build scripts, CI pipelines, and other skills that need to branch behavior based on whether the host is WSL or native Linux (for example, skipping hardware tests, adjusting PATH filtering, or prompting for `usbipd` device attachment).

**When to use**: At the start of any workflow that has WSL-specific behavior, or as a guard condition in shell scripts.

## Architecture

### Directory Layout

```
.github/skills/detect-wsl/
  SKILL.md         # Usage documentation
  spec.md          # This specification
  detect-wsl.sh    # Detection script
```

### Detection Model

The script runs seven independent boolean checks sequentially. Each method that succeeds sets `IS_WSL=1`. The `DETECTION_METHOD` variable records which method first confirmed WSL (the primary detection method). WSL version is determined by specific markers: "WSL2" in the kernel string or the presence of `WSLInterop` indicates version 2; "Microsoft" without "WSL2" indicates version 1.

## Interface / Subcommands

### detect-wsl.sh

```
Usage: .github/skills/detect-wsl/detect-wsl.sh [OPTIONS]

Options:
  --verbose, -v   Show detailed detection report with all check results
  --json, -j      Output results as JSON object
  --help, -h      Show usage help and exit

Exit codes:
  0   Running in WSL (WSL1 or WSL2)
  1   Not running in WSL (native Linux)
```

### Output Modes

**Silent** (default): No stdout. Detection result conveyed entirely via exit code. Designed for use in `if` conditions.

**Verbose** (`--verbose`): Prints a formatted report including the overall result, WSL version, primary detection method, kernel version, Windows version (if retrievable), and a per-check pass/fail table.

**JSON** (`--json`): Prints a single JSON object:

```json
{
  "is_wsl": true,
  "wsl_version": "2",
  "detection_method": "proc_version",
  "kernel_version": "6.6.87.2-microsoft-standard-WSL2",
  "windows_version": "Microsoft Windows 11"
}
```

## Logic / Workflow

### Detection Methods (evaluated in order)

| # | Method | Identifier | Check | WSL Version Signal |
|---|--------|------------|-------|--------------------|
| 1 | proc_version | `proc_version` | `grep -qEi "(Microsoft\|WSL)" /proc/version` | "WSL2" in string -> v2; "Microsoft" only -> v1 |
| 2 | WSLInterop | `wslinterop` | `-f /proc/sys/fs/binfmt_misc/WSLInterop` | Always v2 |
| 3 | Windows mount | `windows_mount` | `-d /mnt/c/Windows` | None (no version info) |
| 4 | osrelease | `osrelease` | `grep -qi "WSL" /proc/sys/kernel/osrelease` | None |
| 5 | drvfs mount | `drvfs_mount` | `mount \| grep -qi "drvfs"` | None |
| 6 | Windows executables | `windows_exe` | `command -v powershell.exe` or `command -v cmd.exe` | None |
| 7 | Drive mounts | `mnt_drives` | `ls /mnt/` contains single-letter directories (a-z) | None |

- Each method is independent and fault-tolerant (errors suppressed with `2>/dev/null`).
- `DETECTION_METHOD` is set to the identifier of the first method that triggers (earlier methods have higher priority).
- Methods 3-7 only set `DETECTION_METHOD` if no prior method already did, but they always set `IS_WSL=1`.
- Method 3 has an additional guard: it only fires if `IS_WSL` is still 0, preventing `/mnt/c/Windows` from being the sole indicator when stronger methods already confirmed.

### Windows Version Retrieval

If WSL is detected and `/mnt/c/Windows/System32/cmd.exe` exists, the script calls `powershell.exe -Command "(Get-WmiObject -class Win32_OperatingSystem).Caption"` to retrieve the Windows edition string (e.g., "Microsoft Windows 11 Pro"). Errors are suppressed; the field is left empty if the call fails.

### Exit Code Calculation

The final exit code is `1 - IS_WSL`:
- `IS_WSL=1` -> exit 0 (success = is WSL)
- `IS_WSL=0` -> exit 1 (failure = not WSL)

This follows the Unix convention where exit 0 indicates the positive/true condition.

## Safety Rules

- **All checks are read-only**. No files are created, modified, or deleted.
- **No root privileges required**. All probed paths and commands are accessible to unprivileged users.
- **All external command failures are suppressed** with `2>/dev/null` to prevent noisy errors on systems where certain paths or commands do not exist.
- **`powershell.exe` invocation is guarded** behind a file existence check (`-f /mnt/c/Windows/System32/cmd.exe`) and `2>/dev/null`, so it never runs on native Linux or if Windows interop is disabled.
- **No network access**. All detection is local.

## Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| `grep` | System | Pattern matching in `/proc/version`, `/proc/sys/kernel/osrelease` |
| `mount` | System | Listing mounted filesystems for drvfs check |
| `uname` | System | Retrieving kernel version string |
| `ls` | System | Scanning `/mnt` for drive letter directories |
| `command` | Built-in | Checking for `powershell.exe` / `cmd.exe` in PATH |
| `powershell.exe` | Optional (WSL) | Retrieving Windows edition name |
