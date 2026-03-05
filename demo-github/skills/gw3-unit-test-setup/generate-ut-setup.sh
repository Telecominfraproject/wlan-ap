#!/bin/bash
#
# Gateway 3 Unit Test Setup Generator
# Automatically generates ut-setup.sh with detected values
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
SETUP_FILE="$PROJECT_ROOT/local/ut-setup.sh"

echo "Generating Unit Test Setup Configuration..."

# Create local directory
mkdir -p "$PROJECT_ROOT/local"

# Detect Console
CONSOLE_PORT=""
CONSOLE_SESSION=""
if screen -ls 2>&1 | grep -q "gw3-console-tty"; then
    CONSOLE_SESSION=$(screen -ls 2>&1 | grep "gw3-console-tty" | awk '{print $1}' | cut -d. -f2 | head -1)
    CONSOLE_PORT="/dev/${CONSOLE_SESSION##*-}"
else
    # No active console - detect available serial port
    CONSOLE_PORT=$(ls /dev/tty{USB,ACM}* 2>/dev/null | head -1)
    if [[ -n "$CONSOLE_PORT" ]]; then
        echo "No active console detected. Starting console on $CONSOLE_PORT..."
        "$SCRIPT_DIR/../gw3-console/auto_console.sh" "$CONSOLE_PORT" >/dev/null 2>&1
        sleep 2
        # Re-check for session
        if screen -ls 2>&1 | grep -q "gw3-console-tty"; then
            CONSOLE_SESSION=$(screen -ls 2>&1 | grep "gw3-console-tty" | awk '{print $1}' | cut -d. -f2 | head -1)
            CONSOLE_PORT="/dev/${CONSOLE_SESSION##*-}"
            echo "✓ Console started: $CONSOLE_SESSION"
        fi
    fi
fi
# Fallback defaults
[[ -z "$CONSOLE_PORT" ]] && CONSOLE_PORT="/dev/ttyUSB0"
[[ -z "$CONSOLE_SESSION" ]] && CONSOLE_SESSION="gw3-console-${CONSOLE_PORT##*/}"

# Detect PC network configuration first
PC_ETH_IF=""
PC_IP=""
DUT_IP=""
DUT_SUBNET=""

# Find PC IP in 192.168.x.x range
PC_CONFIG=$(ip addr show | grep -oP 'inet 192\.168\.\K[0-9]+\.[0-9]+/[0-9]+' | head -1)
if [[ -n "$PC_CONFIG" ]]; then
    PC_SUBNET_NUM=$(echo "$PC_CONFIG" | cut -d. -f1)
    PC_IP="192.168.${PC_SUBNET_NUM}.$(echo "$PC_CONFIG" | cut -d. -f2 | cut -d/ -f1)"
    DUT_SUBNET="192.168.${PC_SUBNET_NUM}.0/24"
    # Try common gateway IPs in the detected subnet
    for GW_IP in "192.168.${PC_SUBNET_NUM}.254" "192.168.${PC_SUBNET_NUM}.1" "192.168.${PC_SUBNET_NUM}.100"; do
        if ping -c 1 -W 1 "$GW_IP" >/dev/null 2>&1; then
            DUT_IP="$GW_IP"
            break
        fi
    done
    # If no gateway found, use .254 as default
    [[ -z "$DUT_IP" ]] && DUT_IP="192.168.${PC_SUBNET_NUM}.254"
fi

# Fallback: try common DUT IPs if no PC IP configured
if [[ -z "$PC_IP" ]]; then
    if ping -c 1 -W 1 192.168.254.254 >/dev/null 2>&1; then
        DUT_IP="192.168.254.254"
        DUT_SUBNET="192.168.254.0/24"
        PC_IP="192.168.254.2"
    elif ping -c 1 -W 1 192.168.1.1 >/dev/null 2>&1; then
        DUT_IP="192.168.1.1"
        DUT_SUBNET="192.168.1.0/24"
        PC_IP="192.168.1.2"
    else
        # Ultimate fallback
        DUT_IP="192.168.254.254"
        DUT_SUBNET="192.168.254.0/24"
        PC_IP="192.168.254.2"
    fi
fi

# Detect PC Ethernet Interface
if [[ -n "$PC_IP" ]]; then
    PC_ETH_IF=$(ip addr show | grep -B 2 "$PC_IP" | grep -oP '^\d+: \K[^:]+' | head -1)
fi
[[ -z "$PC_ETH_IF" ]] && PC_ETH_IF=$(ip route get 1.1.1.1 2>/dev/null | grep -oP 'dev \K\S+' | head -1)
[[ -z "$PC_ETH_IF" ]] && PC_ETH_IF="eth0"

# Detect Console Status
CONSOLE_STATUS="inactive"
[[ -n "$(screen -ls 2>&1 | grep gw3-console)" ]] && CONSOLE_STATUS="active"

# Check Setup Status
SETUP_CONSOLE_ACTIVE="false"
SETUP_DUT_DISCOVERED="false"
SETUP_PC_CONFIGURED="false"
SETUP_DUT_REACHABLE="false"
SETUP_SSH_VERIFIED="false"
SETUP_READY="false"

[[ "$CONSOLE_STATUS" == "active" ]] && SETUP_CONSOLE_ACTIVE="true"
ping -c 1 -W 1 "$DUT_IP" >/dev/null 2>&1 && SETUP_DUT_REACHABLE="true" && SETUP_DUT_DISCOVERED="true"
ip addr show | grep -q "$PC_IP" && SETUP_PC_CONFIGURED="true"
timeout 3 ssh -o ConnectTimeout=2 -o StrictHostKeyChecking=no root@"$DUT_IP" 'echo ok' 2>/dev/null | grep -q ok && SETUP_SSH_VERIFIED="true"

[[ "$SETUP_DUT_REACHABLE" == "true" ]] && [[ "$SETUP_SSH_VERIFIED" == "true" ]] && SETUP_READY="true"

# Generate ut-setup.sh
cat > "$SETUP_FILE" << 'EOF'
#!/bin/bash
# Gateway 3 Unit Test Setup Configuration
# Auto-generated - Source this file to load test environment variables

# Device Under Test (DUT)
export DUT_MODEL="WF-728N"
export DUT_PLATFORM="Qualcomm IPQ5424"
export DUT_FIRMWARE="prplOS"

# Network Configuration
export DUT_WAN_IF="eth0"
export DUT_WAN_IP="DHCP"
export DUT_LAN_IF="br-lan"
export DUT_LAN_IP="__DUT_IP__"
export DUT_LAN_SUBNET="__DUT_SUBNET__"
export DUT_LAN_FALLBACK_IP="192.168.254.254"

export PC_ETH_IF="__PC_ETH_IF__"
export PC_ETH_IP="__PC_IP__"
export PC_GATEWAY="__DUT_IP__"

# Console Access
export CONSOLE_SESSION="__CONSOLE_SESSION__"
export CONSOLE_PORT="__CONSOLE_PORT__"
export CONSOLE_BAUD="115200"
export CONSOLE_LOG_DIR="local/records"
export CONSOLE_STATUS="__CONSOLE_STATUS__"

# Access Credentials
export DUT_SSH_USER="root"
export DUT_SSH_HOST="__DUT_IP__"
export DUT_SSH_PASS=""  # passwordless

# Skill Paths
export SKILL_CONSOLE_EXEC=".github/skills/gw3-console/console_exec.sh"
export SKILL_CONSOLE_CAPTURE=".github/skills/gw3-console/console_capture.sh"
export SKILL_CONSOLE_LIST=".github/skills/gw3-console/list_consoles.sh"
export SKILL_VERIFY_SETUP=".github/skills/gw3-unit-test-setup/verify-setup.sh"
export SKILL_CONFIG_PC=".github/skills/gw3-unit-test-setup/configure-pc-network.sh"

# Setup Status Flags
export SETUP_CONSOLE_ACTIVE="__SETUP_CONSOLE_ACTIVE__"
export SETUP_DUT_DISCOVERED="__SETUP_DUT_DISCOVERED__"
export SETUP_PC_CONFIGURED="__SETUP_PC_CONFIGURED__"
export SETUP_DUT_REACHABLE="__SETUP_DUT_REACHABLE__"
export SETUP_SSH_VERIFIED="__SETUP_SSH_VERIFIED__"
export SETUP_READY="__SETUP_READY__"

# Helper Functions
dut_ssh() {
    ssh "${DUT_SSH_USER}@${DUT_SSH_HOST}" "$@"
}

dut_console() {
    local port="${CONSOLE_PORT##*/}"
    "${SKILL_CONSOLE_EXEC}" "$port" "$@"
}

dut_ping() {
    ping -c 3 "${DUT_LAN_IP}"
}

verify_setup() {
    "${SKILL_VERIFY_SETUP}"
}

show_setup() {
    cat << END
Gateway 3 Unit Test Setup
=========================
DUT Model:       ${DUT_MODEL}
DUT Platform:    ${DUT_PLATFORM}
DUT LAN IP:      ${DUT_LAN_IP}
PC Ethernet:     ${PC_ETH_IF} (${PC_ETH_IP})
Console Port:    ${CONSOLE_PORT}
Console Session: ${CONSOLE_SESSION}
Console Status:  ${CONSOLE_STATUS}
Setup Ready:     ${SETUP_READY}

Quick Commands:
  dut_ssh [command]     - SSH to DUT
  dut_console <cmd>     - Execute command via console
  dut_ping             - Ping DUT
  verify_setup         - Verify test environment
  show_setup           - Show this information
END
}

# If sourced, show setup info
if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then
    echo "Unit test environment loaded. Type 'show_setup' for info."
fi
EOF

# Replace placeholders with detected values
sed -i "s|__DUT_IP__|$DUT_IP|g" "$SETUP_FILE"
sed -i "s|__DUT_SUBNET__|$DUT_SUBNET|g" "$SETUP_FILE"
sed -i "s|__PC_IP__|$PC_IP|g" "$SETUP_FILE"
sed -i "s|__PC_ETH_IF__|$PC_ETH_IF|g" "$SETUP_FILE"
sed -i "s|__CONSOLE_PORT__|$CONSOLE_PORT|g" "$SETUP_FILE"
sed -i "s|__CONSOLE_SESSION__|$CONSOLE_SESSION|g" "$SETUP_FILE"
sed -i "s|__CONSOLE_STATUS__|$CONSOLE_STATUS|g" "$SETUP_FILE"
sed -i "s|__SETUP_CONSOLE_ACTIVE__|$SETUP_CONSOLE_ACTIVE|g" "$SETUP_FILE"
sed -i "s|__SETUP_DUT_DISCOVERED__|$SETUP_DUT_DISCOVERED|g" "$SETUP_FILE"
sed -i "s|__SETUP_PC_CONFIGURED__|$SETUP_PC_CONFIGURED|g" "$SETUP_FILE"
sed -i "s|__SETUP_DUT_REACHABLE__|$SETUP_DUT_REACHABLE|g" "$SETUP_FILE"
sed -i "s|__SETUP_SSH_VERIFIED__|$SETUP_SSH_VERIFIED|g" "$SETUP_FILE"
sed -i "s|__SETUP_READY__|$SETUP_READY|g" "$SETUP_FILE"

chmod +x "$SETUP_FILE"

echo "✓ Generated: $SETUP_FILE"
echo ""
echo "Configuration Summary:"
echo "  DUT IP:      $DUT_IP"
echo "  PC IP:       $PC_IP"
echo "  PC IF:       $PC_ETH_IF"
echo "  Console:     $CONSOLE_PORT ($CONSOLE_STATUS)"
echo "  Ready:       $SETUP_READY"
echo ""
echo "Usage:"
echo "  source local/ut-setup.sh"
echo "  show_setup"
