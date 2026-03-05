#!/bin/bash
#
# Gateway 3 Unit Test Setup Verification
# Verifies that the test environment is correctly configured
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

echo "===================================="
echo "Gateway 3 Unit Test Setup Verification"
echo "===================================="
echo ""

# Step 1: Check Console
echo "Step 1: Console Status"
echo "----------------------"
if screen -ls 2>&1 | grep -q "gw3-console"; then
    echo "✓ Console session active:"
    screen -ls 2>&1 | grep "gw3-console"
    CONSOLE_OK=1
else
    echo "✗ No console session found"
    echo "  Run: .github/skills/gw3-console/auto_console.sh"
    CONSOLE_OK=0
fi
echo ""

# Step 2: Check PC Network
echo "Step 2: PC Network Configuration"
echo "---------------------------------"
echo "Network interfaces:"
ip a s | grep -E "^[0-9]+: " | grep -v "lo:"
echo ""
echo "IP addresses:"
ip a s | grep "inet " | grep -v "127.0.0.1"
echo ""

# Step 3: Test DUT Connectivity
echo "Step 3: DUT Connectivity Test"
echo "------------------------------"
DUT_OK=0
DUT_IP="unknown"

# Try DUT at 192.168.1.1 (normal operation)
if ping -c 2 -W 1 192.168.1.1 >/dev/null 2>&1; then
    echo "✓ DUT reachable at 192.168.1.1"
    DUT_IP="192.168.1.1"
    DUT_SUBNET="192.168.1"
    PC_IP="192.168.1.2"
    DUT_OK=1
else
    # Try fallback IP for U-Boot
    if ping -c 2 -W 1 192.168.254.254 >/dev/null 2>&1; then
        echo "✓ DUT reachable at 192.168.254.254 (fallback/U-Boot)"
        DUT_IP="192.168.254.254"
        DUT_SUBNET="192.168.254"
        PC_IP="192.168.254.2"
        DUT_OK=1
    else
        echo "✗ DUT not reachable"
        echo "  Tried: 192.168.1.1, 192.168.254.254"
        echo "  Note: PC and DUT must be in same subnet"
        echo "  Check: DUT power, network cables, PC network config"
    fi
fi

# Check if PC is in correct subnet
if [ "$DUT_OK" = "1" ]; then
    PC_CURRENT_IP=$(ip addr show | grep "inet $DUT_SUBNET" | awk '{print $2}' | cut -d'/' -f1 | head -1)
    if [ -z "$PC_CURRENT_IP" ]; then
        echo "⚠ PC not in DUT subnet ($DUT_SUBNET.0/24)"
        echo "  Run: sudo ip addr add $PC_IP/24 dev eth0"
    else
        echo "✓ PC in correct subnet: $PC_CURRENT_IP"
    fi
fi
echo ""

# Step 4: Test SSH Access (if DUT reachable)
if [ "$DUT_OK" = "1" ]; then
    echo "Step 4: SSH Access Test"
    echo "-----------------------"
    if timeout 3 ssh -o ConnectTimeout=3 -o StrictHostKeyChecking=no root@$DUT_IP 'echo ok' 2>/dev/null | grep -q "ok"; then
        echo "✓ SSH access to DUT successful"
        SSH_OK=1
    else
        echo "✗ SSH access failed"
        echo "  Target: root@$DUT_IP"
        SSH_OK=0
    fi
    echo ""
fi

# Step 5: Test Internet
echo "Step 5: Internet Connectivity"
echo "------------------------------"
if ping -c 2 -W 2 8.8.8.8 >/dev/null 2>&1; then
    echo "✓ Internet accessible"
    INET_OK=1
else
    echo "✗ No internet connection"
    INET_OK=0
fi
echo ""

# Summary
echo "===================================="
echo "Setup Verification Summary"
echo "===================================="
echo ""
echo "Console:   $( [ "$CONSOLE_OK" = "1" ] && echo "✓ OK" || echo "✗ FAIL" )"
echo "DUT IP:    $DUT_IP"
echo "DUT Ping:  $( [ "$DUT_OK" = "1" ] && echo "✓ OK" || echo "✗ FAIL" )"
echo "SSH:       $( [ "$SSH_OK" = "1" ] && echo "✓ OK" || echo "✗ FAIL" )"
echo "Internet:  $( [ "$INET_OK" = "1" ] && echo "✓ OK" || echo "✗ FAIL" )"
echo ""

# Overall status
if [ "$CONSOLE_OK" = "1" ] && [ "$DUT_OK" = "1" ] && [ "$SSH_OK" = "1" ]; then
    echo "✓ Unit test environment is READY"
    exit 0
else
    echo "✗ Unit test environment needs attention"
    echo ""
    echo "Troubleshooting:"
    [ "$CONSOLE_OK" = "0" ] && echo "  - Setup console: .github/skills/gw3-console/auto_console.sh"
    [ "$DUT_OK" = "0" ] && echo "  - Check DUT power and network connections"
    [ "$SSH_OK" = "0" ] && echo "  - Verify DUT SSH service is running"
    exit 1
fi
