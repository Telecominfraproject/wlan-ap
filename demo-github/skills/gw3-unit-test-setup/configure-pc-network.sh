#!/bin/bash
#
# Configure PC network to match DUT subnet
# Usage: ./configure-pc-network.sh [DUT_IP]
#

DUT_IP="${1:-192.168.1.1}"

# Extract subnet from DUT IP (e.g., 192.168.1.1 -> 192.168.1)
DUT_SUBNET=$(echo "$DUT_IP" | cut -d. -f1-3)
PC_IP="${DUT_SUBNET}.2"

echo "===================================="
echo "PC Network Configuration"
echo "===================================="
echo "DUT IP: $DUT_IP"
echo "Target PC IP: $PC_IP/24"
echo ""

# Find Ethernet interface
IFACE=""
for candidate in eth0 enp* ens* eno*; do
    if [ -e "/sys/class/net/$candidate" ]; then
        # Check if it's an Ethernet device (not wireless)
        if [ -e "/sys/class/net/$candidate/device" ] && ! [ -e "/sys/class/net/$candidate/phy80211" ]; then
            IFACE="$candidate"
            break
        fi
    fi
done

if [ -z "$IFACE" ]; then
    echo "✗ No Ethernet interface found"
    echo ""
    echo "Available interfaces:"
    ls /sys/class/net/ | grep -v lo
    exit 1
fi

echo "Found Ethernet interface: $IFACE"
echo ""

# Check current IP
CURRENT_IP=$(ip addr show $IFACE | grep "inet " | grep "$DUT_SUBNET" | awk '{print $2}' | cut -d/ -f1)

if [ -n "$CURRENT_IP" ]; then
    echo "✓ Interface already configured with IP in DUT subnet: $CURRENT_IP"
else
    echo "Configuring $IFACE with $PC_IP/24..."
    
    # Disable NetworkManager management if needed
    if command -v nmcli >/dev/null 2>&1; then
        sudo nmcli device set $IFACE managed no 2>/dev/null || true
    fi
    
    # Flush existing IPs in different subnets
    sudo ip addr flush dev $IFACE
    
    # Add new IP
    sudo ip addr add $PC_IP/24 dev $IFACE
    
    # Bring interface up
    sudo ip link set $IFACE up
    
    sleep 1
    
    # Verify
    NEW_IP=$(ip addr show $IFACE | grep "inet " | awk '{print $2}')
    if [ -n "$NEW_IP" ]; then
        echo "✓ Interface configured: $NEW_IP"
    else
        echo "✗ Failed to configure interface"
        exit 1
    fi
fi

echo ""
echo "Current configuration:"
ip addr show $IFACE | grep -E "state |inet "
echo ""

# Test connectivity
echo "Testing DUT connectivity..."
if ping -c 2 -W 2 "$DUT_IP" >/dev/null 2>&1; then
    echo "✓ DUT reachable at $DUT_IP"
else
    echo "✗ DUT not reachable at $DUT_IP"
    echo ""
    echo "Troubleshooting:"
    echo "  - Check DUT power and boot status"
    echo "  - Verify Ethernet cable connection"
    echo "  - Confirm DUT LAN IP via console"
fi

echo ""
echo "===================================="
