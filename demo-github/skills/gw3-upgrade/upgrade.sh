#!/bin/bash
#
# Gateway 3 Firmware Upgrade
# Automates DUT firmware upgrade via sysupgrade
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

DUT_IP="${2:-192.168.1.1}"
IMAGE_PATH="${1}"
DUT_TMP="/tmp/upgrade.img"

# Find image if not specified
[[ -z "$IMAGE_PATH" ]] && {
    IMAGE_PATH=$(find "$PROJECT_ROOT/bin/targets/ipq54xx/generic" -name "*-single-aei.img" -type f 2>/dev/null | head -1)
    [[ -z "$IMAGE_PATH" ]] && { echo "Error: No firmware image found"; exit 1; }
}

# Verify image exists
[[ -f "$IMAGE_PATH" ]] || { echo "Error: Image not found: $IMAGE_PATH"; exit 1; }

IMAGE_NAME=$(basename "$IMAGE_PATH")
IMAGE_SIZE=$(du -h "$IMAGE_PATH" | cut -f1)

echo "======================================"
echo "Gateway 3 Firmware Upgrade"
echo "======================================"
echo "Image:    $IMAGE_NAME"
echo "Size:     $IMAGE_SIZE"
echo "DUT IP:   $DUT_IP"
echo "======================================"
echo ""

# Check DUT connectivity
echo -n "Checking DUT connectivity... "
ping -c 1 -W 2 "$DUT_IP" >/dev/null 2>&1 || { echo "FAILED"; echo "Error: DUT not reachable at $DUT_IP"; exit 1; }
echo "OK"

# Check SSH access
echo -n "Checking SSH access... "
ssh -o ConnectTimeout=3 -o StrictHostKeyChecking=no root@"$DUT_IP" 'echo ok' >/dev/null 2>&1 || { echo "FAILED"; exit 1; }
echo "OK"

# Check /tmp space
echo -n "Checking /tmp space... "
AVAIL=$(ssh -o ConnectTimeout=3 root@"$DUT_IP" 'df /tmp | tail -1 | awk "{print \$4}"')
[[ $AVAIL -gt 51200 ]] || { echo "FAILED (only ${AVAIL}K available)"; exit 1; }
echo "OK (${AVAIL}K available)"

echo ""
echo "Copying image to DUT..."
scp -o StrictHostKeyChecking=no "$IMAGE_PATH" root@"$DUT_IP":"$DUT_TMP" || { echo "Error: SCP failed"; exit 1; }

echo ""
echo "Starting sysupgrade..."
echo "  ⚠️  DUT will reboot and disconnect"
echo "  ⚠️  Monitor via console: screen -r gw3-console-ttyUSB0"
echo ""

# Execute sysupgrade (this will disconnect)
ssh -o ConnectTimeout=3 root@"$DUT_IP" "sysupgrade $DUT_TMP" &
SSH_PID=$!

# Wait a bit for command to start
sleep 2

# Check if SSH process exited (expected due to reboot)
kill -0 $SSH_PID 2>/dev/null && wait $SSH_PID 2>/dev/null

echo "✓ Upgrade initiated"
echo ""
echo "Next steps:"
echo "  1. Wait 60-90 seconds for DUT to reboot"
echo "  2. Verify connectivity: ping $DUT_IP"
echo "  3. Check version: ssh root@$DUT_IP 'cat /etc/prplos_version'"
echo ""
