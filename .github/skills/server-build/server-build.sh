#!/usr/bin/env bash
# .github/skills/server-build/server-build.sh
# Remote build script for OpenWiFi AP NOS on the shared build server.
#
# Usage:
#   ./server-build.sh <profile> [--host <ip>] [--user <username>] [--docker-env <env>]
#
# Defaults:
#   --host       192.168.20.30
#   --user       ruanyaoyu
#   --docker-env wf188_tip
#
# Exit codes:
#   0  Build succeeded
#   1  Profile validation error
#   2  SSH connection error
#   3  Build (make) failure

set -euo pipefail

# ---------------------------------------------------------------------------
# Defaults (credentials hardcoded for this project)
# ---------------------------------------------------------------------------
DEFAULT_HOST="192.168.20.30"
DEFAULT_USER="ruanyaoyu"
DEFAULT_PASS="openwifi"
DEFAULT_DOCKER_ENV="wf188_tip"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
PROFILE=""
HOST="$DEFAULT_HOST"
REMOTE_USER="$DEFAULT_USER"
REMOTE_PASS="$DEFAULT_PASS"
DOCKER_ENV="$DEFAULT_DOCKER_ENV"

usage() {
    echo "Usage: $0 <profile> [--host <ip>] [--user <username>] [--docker-env <env>]"
    echo "  <profile>      Profile name (matching profiles/<profile>.yml)"
    echo "  --host         Build server IP/hostname (default: $DEFAULT_HOST)"
    echo "  --user         SSH username (default: $DEFAULT_USER)"
    echo "  --docker-env   Docker environment argument (default: $DEFAULT_DOCKER_ENV)"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --host)        HOST="$2";          shift 2 ;;
        --user)        REMOTE_USER="$2";   shift 2 ;;
        --docker-env)  DOCKER_ENV="$2";    shift 2 ;;
        --help|-h)     usage ;;
        -*)            echo "Unknown option: $1"; usage ;;
        *)
            if [[ -z "$PROFILE" ]]; then
                PROFILE="$1"
                shift
            else
                echo "Unexpected argument: $1"; usage
            fi
            ;;
    esac
done

[[ -n "$PROFILE" ]] || { echo "ERROR: profile is required"; usage; }

# Find repo root (script may be invoked from any directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

# ---------------------------------------------------------------------------
# Step 1: Validate profile
# ---------------------------------------------------------------------------
PROFILE_FILE="${REPO_ROOT}/profiles/${PROFILE}.yml"
if [[ ! -f "$PROFILE_FILE" ]]; then
    echo "ERROR: Profile '${PROFILE}' not found in profiles/"
    echo "Available profiles:"
    for p in "${REPO_ROOT}/profiles/"*.yml; do
        echo "  $(basename "$p" .yml)"
    done
    exit 1
fi

# ---------------------------------------------------------------------------
# Step 2: SSH reachability check
# ---------------------------------------------------------------------------
if ! command -v sshpass >/dev/null 2>&1; then
    echo "ERROR: sshpass is not installed. Install it with: sudo apt install sshpass"
    exit 2
fi

if ! sshpass -p "${REMOTE_PASS}" ssh \
        -o ConnectTimeout=5 -o StrictHostKeyChecking=no \
        -o BatchMode=no \
        "${REMOTE_USER}@${HOST}" "exit 0" 2>/dev/null; then
    echo "ERROR: Cannot reach build server ${HOST}. Is it online?"
    echo "Suggestion: Try /local-build ${PROFILE} for a local build instead."
    exit 2
fi

# Helper: run SSH command with hardcoded credentials via sshpass
ssh_cmd() {
    sshpass -p "${REMOTE_PASS}" ssh -o StrictHostKeyChecking=no \
        "${REMOTE_USER}@${HOST}" "$@"
}

# ---------------------------------------------------------------------------
# Step 3: Start Docker + Step 4: Configure + Step 5: Build
# ---------------------------------------------------------------------------
echo "Connecting to ${REMOTE_USER}@${HOST}..."
echo "Starting build for profile: ${PROFILE}"

BUILD_LOG="/tmp/build-${PROFILE}.log"

ssh_cmd bash -s -- "$DOCKER_ENV" "$PROFILE" "$BUILD_LOG" <<'REMOTE_SCRIPT'
DOCKER_ENV="$1"
PROFILE="$2"
BUILD_LOG="$3"

set -e

# Start docker build environment
run_build_docker "$DOCKER_ENV"

# Configure
cd openwrt
./scripts/gen_config.py "$PROFILE"

# Build
make -j"$(nproc)" 2>&1 | tee "$BUILD_LOG"
REMOTE_SCRIPT

BUILD_EXIT=${PIPESTATUS[0]:-$?}

# ---------------------------------------------------------------------------
# Step 6: Report
# ---------------------------------------------------------------------------
if [[ $BUILD_EXIT -eq 0 ]]; then
    echo ""
    echo "Build succeeded: ${PROFILE}"
    # Collect image paths from remote
    ssh_cmd bash -c "find openwrt/bin/ -type f \( -name '*.bin' -o -name '*.img' \) 2>/dev/null" | \
        while IFS= read -r img; do
            echo "Image: ${img}"
        done
    exit 0
else
    echo ""
    echo "Build failed: ${PROFILE} (exit code ${BUILD_EXIT})"
    echo "Last 50 lines of build log:"
    ssh_cmd "tail -50 ${BUILD_LOG}" 2>/dev/null || echo "(log not available)"
    exit 3
fi
