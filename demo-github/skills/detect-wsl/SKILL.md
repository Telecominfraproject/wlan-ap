---
name: detect-wsl
description: Detect if the host system is running inside Windows Subsystem for Linux (WSL)
---

## Overview

This skill provides reliable detection of WSL environments, distinguishing between WSL1, WSL2, and native Linux systems. It uses multiple detection methods to ensure accurate identification and provides both programmatic (exit codes) and human-readable output formats.

## Quick Start

```bash
# Check if running in WSL (silent mode)
.github/skills/detect-wsl/detect-wsl.sh
if [ $? -eq 0 ]; then
    echo "Running in WSL"
else
    echo "Native Linux"
fi
```

## Usage

### Basic Detection

**Silent Mode (for scripts):**
```bash
.github/skills/detect-wsl/detect-wsl.sh
# Exit code 0 = WSL, 1 = Native Linux
```

**Verbose Mode (for humans):**
```bash
.github/skills/detect-wsl/detect-wsl.sh --verbose
```

**JSON Output:**
```bash
.github/skills/detect-wsl/detect-wsl.sh --json
```

### Command Options

- `--verbose, -v` - Show detailed detection report with all checks
- `--json, -j` - Output results in JSON format for programmatic processing
- `--help, -h` - Display help message

## Detection Methods

The script uses 7 independent detection methods to reliably identify WSL:

1. **proc_version** - Checks `/proc/version` for Microsoft/WSL keywords
2. **wslinterop** - Detects WSLInterop binary format (WSL2-specific)
3. **osrelease** - Checks `/proc/sys/kernel/osrelease` for WSL marker
4. **drvfs_mount** - Looks for drvfs mounts (WSL filesystem)
5. **windows_mount** - Checks for Windows mount point at `/mnt/c/Windows`
6. **windows_exe** - Verifies accessibility of Windows executables (powershell.exe, cmd.exe)
7. **mnt_drives** - Scans `/mnt` for typical single-letter drive mounts (a-z)

## Exit Codes

- `0` - Running in WSL (includes both WSL1 and WSL2)
- `1` - Not running in WSL (native Linux)

## Example Output

### Verbose Mode
```
===================================
WSL Detection Report
===================================

Result: Running in WSL ✓
WSL Version: 2
Detection Method: proc_version

System Information:
  Kernel: 6.6.87.2-microsoft-standard-WSL2
  Windows: Microsoft Windows 11

Detection Checks:
  ✓ /proc/version contains Microsoft/WSL
  ✗ No WSLInterop (not WSL2)
  ✓ Windows mount point found (/mnt/c)
  ✓ drvfs mounts found (WSL filesystem)
  ✓ Windows executables accessible

===================================
```

### JSON Mode
```json
{
  "is_wsl": true,
  "wsl_version": "2",
  "detection_method": "proc_version",
  "kernel_version": "6.6.87.2-microsoft-standard-WSL2",
  "windows_version": "Microsoft Windows 11"
}
```

## Use Cases

### In Shell Scripts
```bash
# Conditionally load WSL-specific configurations
.github/skills/detect-wsl/detect-wsl.sh
if [ $? -eq 0 ]; then
    # WSL-specific setup
    source .github/skills/wsl-setup.sh
else
    # Native Linux setup
    source .github/skills/native-setup.sh
fi
```

### In Build Systems
```bash
# Skip certain targets on WSL
if .github/skills/detect-wsl/detect-wsl.sh; then
    echo "Skipping hardware-specific tests in WSL"
else
    make test-hardware
fi
```

## WSL Version Determination

- **WSL Version 1**: Legacy WSL using translation layer, shows "Microsoft" in kernel version
- **WSL Version 2**: Hyper-V based, has dedicated kernel with "WSL2" marker
- Both versions are correctly detected, with version info provided in verbose/JSON output

## Reliability Notes

- Works in both WSL1 and WSL2 environments
- No root privileges required
- Safe to use in automated systems and CI/CD pipelines
- All detection methods are non-invasive and read-only
- Handles missing files gracefully with fallback checks
