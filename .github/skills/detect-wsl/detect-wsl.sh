#!/bin/bash
#
# WSL Detection Script
# Detects if the current system is running inside Windows Subsystem for Linux
#
# Exit codes:
#   0 - Running in WSL
#   1 - Not running in WSL (native Linux)
#
# Usage:
#   ./detect-wsl.sh                    # Silent mode, check exit code
#   ./detect-wsl.sh --verbose          # Show detection details
#   ./detect-wsl.sh --json             # Output JSON format
#

VERBOSE=0
JSON_OUTPUT=0

# Parse arguments
for arg in "$@"; do
    case $arg in
        --verbose|-v)
            VERBOSE=1
            ;;
        --json|-j)
            JSON_OUTPUT=1
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Detect if running inside Windows Subsystem for Linux (WSL)"
            echo ""
            echo "Options:"
            echo "  --verbose, -v     Show detection details"
            echo "  --json, -j        Output result in JSON format"
            echo "  --help, -h        Show this help message"
            echo ""
            echo "Exit codes:"
            echo "  0 - Running in WSL"
            echo "  1 - Not running in WSL (native Linux)"
            exit 0
            ;;
    esac
done

# Detection logic
IS_WSL=0
WSL_VERSION=""
DETECTION_METHOD=""

# Method 1: Check /proc/version for Microsoft or WSL keywords
if grep -qEi "(Microsoft|WSL)" /proc/version 2>/dev/null; then
    IS_WSL=1
    DETECTION_METHOD="proc_version"
    
    # Try to determine WSL version
    if grep -qi "WSL2" /proc/version 2>/dev/null; then
        WSL_VERSION="2"
    elif grep -qi "Microsoft" /proc/version 2>/dev/null; then
        # WSL1 typically shows "Microsoft" in kernel version
        WSL_VERSION="1"
    fi
fi

# Method 2: Check for WSLInterop (WSL2 specific)
if [ -f /proc/sys/fs/binfmt_misc/WSLInterop ]; then
    IS_WSL=1
    WSL_VERSION="2"
    [ -z "$DETECTION_METHOD" ] && DETECTION_METHOD="wslinterop"
fi

# Method 3: Check for Windows mount points
if [ -d /mnt/c/Windows ] && [ "$IS_WSL" = "0" ]; then
    IS_WSL=1
    [ -z "$DETECTION_METHOD" ] && DETECTION_METHOD="windows_mount"
fi

# Method 4: Check /proc/sys/kernel/osrelease
if [ -f /proc/sys/kernel/osrelease ] && grep -qi "WSL" /proc/sys/kernel/osrelease 2>/dev/null; then
    IS_WSL=1
    [ -z "$DETECTION_METHOD" ] && DETECTION_METHOD="osrelease"
fi

# Method 5: Check for drvfs mounts (WSL-specific filesystem)
if mount | grep -qi "drvfs" 2>/dev/null; then
    IS_WSL=1
    [ -z "$DETECTION_METHOD" ] && DETECTION_METHOD="drvfs_mount"
fi

# Method 6: Check for Windows executables accessibility
if command -v powershell.exe >/dev/null 2>&1 || command -v cmd.exe >/dev/null 2>&1; then
    IS_WSL=1
    [ -z "$DETECTION_METHOD" ] && DETECTION_METHOD="windows_exe"
fi

# Method 7: Check /mnt for typical WSL drive mounts
if [ -d /mnt ] && ls /mnt/ 2>/dev/null | grep -qE "^[a-z]$"; then
    IS_WSL=1
    [ -z "$DETECTION_METHOD" ] && DETECTION_METHOD="mnt_drives"
fi

# Get additional info if in WSL
KERNEL_VERSION=$(uname -r)
WINDOWS_VERSION=""
if [ "$IS_WSL" = "1" ] && [ -f /mnt/c/Windows/System32/cmd.exe ]; then
    WINDOWS_VERSION=$(powershell.exe -Command "(Get-WmiObject -class Win32_OperatingSystem).Caption" 2>/dev/null | tr -d '\r')
fi

# Output results
if [ "$JSON_OUTPUT" = "1" ]; then
    # JSON output
    echo "{"
    echo "  \"is_wsl\": $([[ $IS_WSL -eq 1 ]] && echo "true" || echo "false"),"
    echo "  \"wsl_version\": \"$WSL_VERSION\","
    echo "  \"detection_method\": \"$DETECTION_METHOD\","
    echo "  \"kernel_version\": \"$KERNEL_VERSION\","
    echo "  \"windows_version\": \"$WINDOWS_VERSION\""
    echo "}"
elif [ "$VERBOSE" = "1" ]; then
    # Verbose output
    echo "==================================="
    echo "WSL Detection Report"
    echo "==================================="
    echo ""
    
    if [ "$IS_WSL" = "1" ]; then
        echo "Result: Running in WSL ✓"
        [ -n "$WSL_VERSION" ] && echo "WSL Version: $WSL_VERSION"
        echo "Detection Method: $DETECTION_METHOD"
    else
        echo "Result: Native Linux (not WSL) ✗"
    fi
    
    echo ""
    echo "System Information:"
    echo "  Kernel: $KERNEL_VERSION"
    [ -n "$WINDOWS_VERSION" ] && echo "  Windows: $WINDOWS_VERSION"
    
    echo ""
    echo "Detection Checks:"
    if grep -qEi "(Microsoft|WSL)" /proc/version 2>/dev/null; then
        echo "  ✓ /proc/version contains Microsoft/WSL"
    else
        echo "  ✗ /proc/version does not contain Microsoft/WSL"
    fi
    
    if [ -f /proc/sys/fs/binfmt_misc/WSLInterop ]; then
        echo "  ✓ WSLInterop found (WSL2)"
    else
        echo "  ✗ No WSLInterop (not WSL2)"
    fi
    
    if [ -d /mnt/c/Windows ]; then
        echo "  ✓ Windows mount point found (/mnt/c)"
    else
        echo "  ✗ No Windows mount point"
    fi
    
    if mount | grep -qi "drvfs" 2>/dev/null; then
        echo "  ✓ drvfs mounts found (WSL filesystem)"
    else
        echo "  ✗ No drvfs mounts"
    fi
    
    if command -v powershell.exe >/dev/null 2>&1 || command -v cmd.exe >/dev/null 2>&1; then
        echo "  ✓ Windows executables accessible"
    else
        echo "  ✗ No Windows executables"
    fi
    
    echo ""
    echo "==================================="
fi

# Exit with appropriate code
exit $((1 - IS_WSL))
