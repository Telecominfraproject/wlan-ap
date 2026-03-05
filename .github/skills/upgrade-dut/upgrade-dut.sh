#!/usr/bin/env bash
# .github/skills/upgrade-dut/upgrade-dut.sh
# Upload firmware image to a DUT and trigger sysupgrade.
#
# Usage:
#   ./upgrade-dut.sh [<image-path>] [--dut-ip <ip>] [--dut-user <user>]
#
# Defaults:
#   --dut-ip    192.168.1.1
#   --dut-user  root
#
# Exit codes:
#   0  Upgrade command issued (DUT rebooting)
#   1  Image file not found
#   2  DUT unreachable
#   3  SCP transfer failure

set -euo pipefail

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
DEFAULT_DUT_IP="192.168.1.1"
DEFAULT_DUT_USER="root"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
IMAGE_PATH=""
DUT_IP="$DEFAULT_DUT_IP"
DUT_USER="$DEFAULT_DUT_USER"

usage() {
    echo "Usage: $0 [<image-path>] [--dut-ip <ip>] [--dut-user <user>]"
    echo "  <image-path>  Path to firmware image (auto-discovered if omitted)"
    echo "  --dut-ip      DUT management IP (default: $DEFAULT_DUT_IP)"
    echo "  --dut-user    SSH username on DUT (default: $DEFAULT_DUT_USER)"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dut-ip)    DUT_IP="$2";   shift 2 ;;
        --dut-user)  DUT_USER="$2"; shift 2 ;;
        --help|-h)   usage ;;
        -*)          echo "Unknown option: $1"; usage ;;
        *)
            if [[ -z "$IMAGE_PATH" ]]; then
                IMAGE_PATH="$1"
                shift
            else
                echo "Unexpected argument: $1"; usage
            fi
            ;;
    esac
done

# Find repo root (script may be invoked from any directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

# Helper: run SSH/SCP command (uses sshpass if SSH_PASS env var is set)
_ssh() {
    if command -v sshpass >/dev/null 2>&1 && [[ -n "${SSH_PASS:-}" ]]; then
        sshpass -p "$SSH_PASS" ssh -o StrictHostKeyChecking=no \
            -o ConnectTimeout=5 "${DUT_USER}@${DUT_IP}" "$@"
    else
        ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 \
            "${DUT_USER}@${DUT_IP}" "$@"
    fi
}

_scp() {
    local src="$1"
    if command -v sshpass >/dev/null 2>&1 && [[ -n "${SSH_PASS:-}" ]]; then
        sshpass -p "$SSH_PASS" scp -o StrictHostKeyChecking=no "$src" \
            "${DUT_USER}@${DUT_IP}:/tmp/"
    else
        scp -o StrictHostKeyChecking=no "$src" "${DUT_USER}@${DUT_IP}:/tmp/"
    fi
}

# ---------------------------------------------------------------------------
# Step 1: Resolve image
# ---------------------------------------------------------------------------
if [[ -n "$IMAGE_PATH" ]]; then
    # Explicit path provided — verify it exists
    if [[ ! -f "$IMAGE_PATH" ]]; then
        echo "ERROR: Image file not found: ${IMAGE_PATH}"
        exit 1
    fi
else
    # Auto-discover firmware images under openwrt/bin/
    OPENWRT_BIN="${REPO_ROOT}/openwrt/bin"
    if [[ ! -d "$OPENWRT_BIN" ]]; then
        echo "ERROR: No firmware images found in openwrt/bin/."
        echo "Run /server-build <profile> or /local-build <profile> first."
        exit 1
    fi

    mapfile -t IMAGES < <(find "$OPENWRT_BIN" -type f \( -name "*.bin" -o -name "*.img" \) 2>/dev/null | sort)

    if [[ ${#IMAGES[@]} -eq 0 ]]; then
        echo "ERROR: No firmware images found in openwrt/bin/."
        echo "Run /server-build <profile> or /local-build <profile> first."
        exit 1
    elif [[ ${#IMAGES[@]} -eq 1 ]]; then
        IMAGE_PATH="${IMAGES[0]}"
        echo "Auto-selected image: ${IMAGE_PATH}"
    else
        echo "Multiple firmware images found. Select one:"
        for i in "${!IMAGES[@]}"; do
            printf "  [%d] %s\n" $((i + 1)) "${IMAGES[$i]}"
        done
        read -rp "Enter number: " CHOICE
        if ! [[ "$CHOICE" =~ ^[0-9]+$ ]] || \
           [[ "$CHOICE" -lt 1 ]] || [[ "$CHOICE" -gt ${#IMAGES[@]} ]]; then
            echo "ERROR: Invalid selection."
            exit 1
        fi
        IMAGE_PATH="${IMAGES[$((CHOICE - 1))]}"
    fi
fi

FILENAME="$(basename "$IMAGE_PATH")"

# ---------------------------------------------------------------------------
# Step 2: Verify DUT reachability
# ---------------------------------------------------------------------------
echo "Checking DUT reachability (${DUT_USER}@${DUT_IP})..."
if ! _ssh "exit 0" 2>/dev/null; then
    echo "ERROR: Cannot reach DUT at ${DUT_IP}."
    echo "Check that the DUT is powered on and the management interface is up."
    exit 2
fi

# ---------------------------------------------------------------------------
# Step 3: Transfer image via SCP
# ---------------------------------------------------------------------------
echo "Transferring image to DUT (${DUT_IP})..."
echo "  ${FILENAME} → /tmp/${FILENAME}  ..."

if ! _scp "$IMAGE_PATH"; then
    SCP_EXIT=$?
    echo ""
    echo "ERROR: File transfer failed (scp exit ${SCP_EXIT})."
    echo "Possible causes: DUT disk full (/tmp); network interrupted; wrong credentials."
    echo "Sysupgrade NOT executed."
    exit 3
fi
echo "  ${FILENAME} → /tmp/${FILENAME}  [OK]"

# ---------------------------------------------------------------------------
# Step 4: Execute upgrade (|| true because DUT drops connection on sysupgrade)
# ---------------------------------------------------------------------------
echo "Issuing sysupgrade..."
# shellcheck disable=SC2029
_ssh "sysupgrade -n /tmp/${FILENAME}" || true

echo "Upgrade command issued. DUT is rebooting — connection drop is expected."
exit 0
