#!/usr/bin/env bash
# .github/skills/local-build/local-build.sh
# Build OpenWiFi firmware locally using the openwrt/ build tree.
#
# Usage:
#   ./local-build.sh <profile> [--openwrt-dir <path>]
#
# Defaults:
#   --openwrt-dir  openwrt/
#
# Exit codes:
#   0  Build succeeded
#   1  openwrt/ directory missing
#   2  Profile validation error
#   3  gen_config.py failure
#   4  Build (make) failure

set -euo pipefail

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
DEFAULT_OPENWRT_DIR="openwrt"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
PROFILE=""
OPENWRT_DIR="$DEFAULT_OPENWRT_DIR"

usage() {
    echo "Usage: $0 <profile> [--openwrt-dir <path>]"
    echo "  <profile>       Profile name (matching profiles/<profile>.yml)"
    echo "  --openwrt-dir   Path to openwrt build tree (default: openwrt/)"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --openwrt-dir)  OPENWRT_DIR="$2"; shift 2 ;;
        --help|-h)      usage ;;
        -*)             echo "Unknown option: $1"; usage ;;
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

# Resolve openwrt dir (support both relative-to-cwd and absolute)
if [[ "$OPENWRT_DIR" != /* ]]; then
    OPENWRT_ABS="${REPO_ROOT}/${OPENWRT_DIR}"
else
    OPENWRT_ABS="$OPENWRT_DIR"
fi

# ---------------------------------------------------------------------------
# Step 1: Validate openwrt directory
# ---------------------------------------------------------------------------
if [[ ! -d "$OPENWRT_ABS" ]]; then
    echo "ERROR: openwrt/ directory not found at ${OPENWRT_ABS}."
    echo "Run './setup.py --setup' first to initialize the build tree."
    echo "See README.md §Building for details."
    exit 1
fi

# ---------------------------------------------------------------------------
# Step 2: Validate profile
# ---------------------------------------------------------------------------
PROFILE_FILE="${REPO_ROOT}/profiles/${PROFILE}.yml"
if [[ ! -f "$PROFILE_FILE" ]]; then
    echo "ERROR: Profile '${PROFILE}' not found in profiles/"
    echo "Available profiles:"
    for p in "${REPO_ROOT}/profiles/"*.yml; do
        echo "  $(basename "$p" .yml)"
    done
    exit 2
fi

# ---------------------------------------------------------------------------
# Step 3: Configure
# ---------------------------------------------------------------------------
echo "Configuring profile: ${PROFILE}"
cd "$OPENWRT_ABS"

if ! ./scripts/gen_config.py "$PROFILE"; then
    echo "ERROR: gen_config.py failed for profile '${PROFILE}'"
    exit 3
fi

# ---------------------------------------------------------------------------
# Step 4: Build
# ---------------------------------------------------------------------------
BUILD_LOG="/tmp/build-${PROFILE}.log"
echo "Building ($(nproc) cores)... Log: ${BUILD_LOG}"

if make -j"$(nproc)" 2>&1 | tee "$BUILD_LOG"; then
    BUILD_EXIT=0
else
    BUILD_EXIT=${PIPESTATUS[0]:-1}
fi

# ---------------------------------------------------------------------------
# Step 5: Report
# ---------------------------------------------------------------------------
if [[ $BUILD_EXIT -eq 0 ]]; then
    echo ""
    echo "Build succeeded: ${PROFILE}"
    find "${OPENWRT_ABS}/bin/" -type f \( -name "*.bin" -o -name "*.img" \) 2>/dev/null | \
        while IFS= read -r img; do
            echo "Image: ${img}"
        done
    exit 0
else
    echo ""
    echo "Build failed: ${PROFILE} (exit code ${BUILD_EXIT})"
    echo "Last 50 lines of build log:"
    tail -50 "$BUILD_LOG" 2>/dev/null || echo "(log not available)"
    exit 4
fi
