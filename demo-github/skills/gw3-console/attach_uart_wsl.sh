#!/bin/bash
# Attach UART device to WSL via usbipd
# Usage: attach_uart_wsl.sh [busid]

BUSID="$1"

# Check if running in WSL
grep -qEi "WSL|Microsoft" /proc/version 2>/dev/null || { echo "Not running in WSL"; exit 1; }

# Check if usbipd is available
command -v usbipd.exe >/dev/null || { 
    echo "usbipd-win not installed"
    echo "Install: winget.exe install --interactive --exact dorssel.usbipd-win"
    exit 1
}

# Auto-detect UART device if no busid provided
if [ -z "$BUSID" ]; then
    echo "=== Detecting UART devices ==="
    # Look for common UART/Serial device patterns
    DEVICES=$(usbipd.exe list | grep -iE "serial|uart|ftdi|cp210|ch340|prolific" | grep -E "^[0-9]+-[0-9]+")
    
    if [ -z "$DEVICES" ]; then
        echo "No UART devices found"
        echo ""
        echo "All USB devices:"
        usbipd.exe list
        exit 1
    fi
    
    # Count detected devices
    COUNT=$(echo "$DEVICES" | wc -l)
    
    if [ "$COUNT" -eq 1 ]; then
        # Auto-select single device
        BUSID=$(echo "$DEVICES" | awk '{print $1}')
        DEVICE_NAME=$(echo "$DEVICES" | cut -d' ' -f3-)
        echo "✓ Auto-detected: $BUSID - $DEVICE_NAME"
    else
        # Multiple devices - let user choose
        echo "Found $COUNT UART devices:"
        echo "$DEVICES" | nl -w2 -s') '
        echo ""
        read -p "Select device number (1-$COUNT) or enter busid: " CHOICE
        
        if [[ "$CHOICE" =~ ^[0-9]+-[0-9]+$ ]]; then
            BUSID="$CHOICE"
        else
            BUSID=$(echo "$DEVICES" | sed -n "${CHOICE}p" | awk '{print $1}')
        fi
    fi
fi

[ -z "$BUSID" ] && { echo "No device selected"; exit 1; }

# Check if device is bound
if ! usbipd.exe list | grep -q "$BUSID.*Shared"; then
    echo "Binding device $BUSID..."
    usbipd.exe bind --busid "$BUSID" || { echo "Failed to bind"; exit 1; }
fi

# Attach device to WSL
echo "Attaching device $BUSID to WSL..."
usbipd.exe attach --wsl --busid "$BUSID" || { echo "Failed to attach"; exit 1; }

# Wait and verify
sleep 2
if ls /dev/tty{USB,ACM}* 2>/dev/null | grep -q .; then
    echo "✓ Device attached successfully"
    ls -l /dev/tty{USB,ACM}* 2>/dev/null
else
    echo "⚠ Device attached but not detected. Wait a moment or check drivers."
fi
