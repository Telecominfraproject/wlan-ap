---
name: gw3-unit-test-setup
description: Create and confirm unit test setup configuration for Gateway 3 DUT
---

# Gateway 3 Unit Test Setup Skill

This skill helps create and confirm a unit test setup configuration for Gateway 3 Device Under Test (DUT). It generates a configuration file that can be referenced by other skills and test automation.

## Overview

The unit test setup documents the physical and network configuration between:
- **DUT**: Gateway 3 device (WF-728A or WF-728N)
- **PC**: Development workstation running builds and tests
- **Network**: Internet connectivity and local network configuration
- **Console**: Optional UART serial console connection

This skill automatically generates `local/ut-setup.sh` with detected configuration values.

## Quick Start

```bash
# Generate ut-setup.sh with auto-detected values
.github/skills/gw3-unit-test-setup/generate-ut-setup.sh

# Source the configuration
source local/ut-setup.sh

# Verify setup
verify_setup
```

## Scripts

This skill provides the following scripts:

- **generate-ut-setup.sh**: Automatically detects and generates `local/ut-setup.sh` configuration
- **configure-pc-network.sh**: Helper to configure PC network interface
- **verify-setup.sh**: Verifies test environment is properly configured

## Test Setup Components

### 1. Device Under Test (DUT)

**Supported Gateway 3 Models:**
- **WF-728N** (Default) - Qualcomm IPQ5424 platform, 256MB RAM, 32MB Flash
- **WF-728A** - Qualcomm IPQ5424 platform, alternate configuration

**DUT Network Configuration:**
- **WAN Port**: Connected to Internet via DHCP (upstream router/modem)
- **LAN Port**: Connected to PC for testing and management
- **LAN IP**: Defined by DUT software (varies by configuration)
  - Example: 192.168.1.1/24, 192.168.0.1/24, or other subnet
  - **Fallback IP**: 192.168.254.254/24 (when DHCP not available or in U-Boot)
- **DHCP Server**: Typically enabled on LAN interface (when system is fully booted)

### 2. PC (Development Workstation)

**Network Requirements:**
- **LAN Connection**: Ethernet to DUT LAN port
  - **Primary**: DHCP from DUT (obtains IP in DUT's subnet automatically)
  - **Fallback**: Static IP when DHCP unavailable
    - **PC Static IP**: 192.168.254.2/24 (default, user-confirmed)
    - **DUT Static IP**: 192.168.254.254/24 (provisioned in DUT)
    - Use fallback when: DUT in U-Boot, DHCP disabled, or boot incomplete
- **WiFi Connection**: Internet connectivity (independent of DUT)
  - Used for downloads, documentation, and external resources

**Software Requirements:**
- Build tools (see [gw3-build](../gw3-build/SKILL.md) skill)
- Serial terminal tools (optional, see [gw3-console](../gw3-console/SKILL.md) skill)
- Network tools: ping, curl, ssh, netcat

### 3. Network Topology

```
Internet
   |
   | (DHCP)
   |
[WAN Port]--[DUT Gateway 3]--[LAN Port]
                                 |
                                 | (Ethernet)
                                 |
                            [PC Ethernet]
                            
[PC WiFi]----[WiFi Router]----[Internet]
```

### 4. Optional Console Connection

UART serial console for debugging and recovery:
- USB-to-Serial adapter connected to PC
- Serial parameters: 115200 8N1
- See [gw3-console](../gw3-console/SKILL.md) for detailed setup

## Setup Verification Steps

### Step 1: Verify DUT Model

**Check DUT label or documentation:**
```bash
# Common models:
# - WF-728N (default)
# - WF-728A
```

Record the model name for configuration.

### Step 2: Verify Physical Connections

**Check all physical connections:**

1. **DUT WAN Port** → Ethernet cable → **Router/Modem** (Internet)
2. **DUT LAN Port** → Ethernet cable → **PC Ethernet Port**
3. **DUT Power** → Power adapter → Power outlet
4. **(Optional)** **DUT UART** → USB-Serial adapter → **PC USB Port**

**Verify link status:**
```bash
# On PC, check ethernet interface
ip link show

# Look for interface (e.g., eth0, enp0s31f6) with "state UP"
# Example output:
# 2: enp0s31f6: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
```

### Step 3: Verify DUT Power and Boot

**Power on DUT and wait for boot completion (typically 1-2 minutes):**

Indicators:
- Power LED should be solid or blinking (model-dependent)
- LAN port LEDs should show link activity
- Wait for DHCP server to start on DUT

### Step 4: Configure PC Network

**\u26a0\ufe0f CRITICAL**: PC must be in same subnet as DUT.

**Method 1: Automatic Configuration (Recommended)**

Use the helper script to auto-configure PC network:

```bash
# Auto-detect interface and configure for DUT at 192.168.1.1
.github/skills/gw3-unit-test-setup/configure-pc-network.sh 192.168.1.1

# For U-Boot fallback IP
.github/skills/gw3-unit-test-setup/configure-pc-network.sh 192.168.254.254
```

**Method 2: Manual Configuration**

If DHCP from DUT is working:
```bash
# Check if you got IP from DUT
ip addr show
ip route show | grep default
# Gateway IP is the DUT LAN IP
```

If DHCP not available, configure static IP **in same subnet as DUT**:

```bash
# Example 1: DUT at 192.168.1.1 (typical)
sudo ip addr flush dev <interface>
sudo ip addr add 192.168.1.2/24 dev <interface>
sudo ip link set <interface> up

# Example 2: DUT at 192.168.254.254 (U-Boot fallback)
sudo ip addr flush dev <interface>
sudo ip addr add 192.168.254.2/24 dev <interface>
sudo ip link set <interface> up

# Verify
ip addr show <interface>
```

**\u26a0\ufe0f Subnet Matching**: If DUT is at 192.168.1.1, PC must be 192.168.1.x (not 192.168.254.x)

### Step 5: Verify DUT Connectivity

**Step 5a: Detect DUT IP from Console (Recommended)**

```bash
# Get DUT IP from console
.github/skills/gw3-console/console_exec.sh ttyUSB0 'ip addr show br-lan | grep "inet "'

# Wait 2 seconds and check console log
sleep 2
tail -20 local/records/console_ttyUSB0_*.log | grep "inet "
# Look for: inet 192.168.1.1/24 or similar
```

**Step 5b: Ensure PC in Same Subnet**

```bash
# If DUT is at 192.168.1.1, configure PC:
.github/skills/gw3-unit-test-setup/configure-pc-network.sh 192.168.1.1
```

**Step 5c: Test Connectivity**

```bash
# Determine DUT IP address
DUT_IP=$(ip route show | grep default | awk '{print $3}')
if [ -z "$DUT_IP" ]; then
    # Use discovered IP or fallback
    DUT_IP="192.168.1.1"  # Use IP found from console
fi
echo "DUT IP: $DUT_IP"

# Ping DUT at discovered IP
ping -c 4 $DUT_IP
```

**Test connectivity to DUT:**

```bash
# Ping DUT (use IP obtained from DHCP or fallback 192.168.254.254)
ping -c 4 $DUT_IP

# Expected output:
# 64 bytes from <DUT_IP>: icmp_seq=1 ttl=64 time=1.23 ms
# 64 bytes from <DUT_IP>: icmp_seq=2 ttl=64 time=1.45 ms
# ...
# 4 packets transmitted, 4 received, 0% packet loss
```

**Test SSH access to DUT:**

```bash
# Try SSH to DUT (default user: root, password may be empty or configured)
ssh root@$DUT_IP

# If successful, you should see Gateway 3 shell prompt:
# root@prpl-haze:~#

# Exit SSH session
exit
```

### Step 6: Verify DUT Internet Connectivity

**Check if DUT has Internet access via WAN:**

```bash
# SSH into DUT
ssh root@192.168.1.1

# On DUT, check WAN interface IP
ip addr show eth0  # or check interface name: ip link show

# Ping external server
ping -c 4 8.8.8.8

# Check DNS resolution
nslookup google.com

# Exit DUT
exit
```

### Step 7: Verify PC WiFi Internet Connectivity

**Verify PC has separate Internet access via WiFi:**

```bash
# Check WiFi interface status
ip addr show wlan0  # or your WiFi interface name

# Test Internet connectivity via WiFi
# (Disable LAN temporarily to verify WiFi route)
ping -I wlan0 -c 4 8.8.8.8

# Test DNS via WiFi
dig @8.8.8.8 google.com
```

### Step 8: Generate Setup Configuration

**Automatically generate ut-setup.sh with detected values:**

```bash
# Generate configuration file
.github/skills/gw3-unit-test-setup/generate-ut-setup.sh

# This will:
# - Auto-detect DUT IP address (192.168.1.1 or 192.168.254.254)
# - Detect PC ethernet interface
# - Detect console session if active
# - Check setup status (reachability, SSH, etc.)
# - Generate local/ut-setup.sh with all values
```

**Source and use the configuration:**

```bash
# Load test environment
source local/ut-setup.sh

# Show configuration
show_setup

# Use helper functions
dut_ping
dut_ssh 'uname -a'
```

### Step 9: Setup Console Connection (Optional)

**Auto-detect and setup console session:**

```bash
# Auto-detect serial port and start/show console session
.github/skills/gw3-console/auto_console.sh

# This will:
# - Auto-detect USB serial ports
# - Create background console session if not exists
# - Show session info if already running
```

**After console setup, regenerate ut-setup.sh:**

```bash
# Regenerate with console info
.github/skills/gw3-unit-test-setup/generate-ut-setup.sh
```

**If console is available, verify DUT access:**

```bash
# Load environment
source local/ut-setup.sh

# Use console helper
dut_console 'uname -a'
dut_console 'ip addr show br-lan'
```

**If no USB serial device found:**
- Check USB-to-serial adapter connection
- If on WSL, attach device with usbipd (see [gw3-console](../gw3-console/SKILL.md))
- Console is optional - you can proceed without it

See [gw3-console](../gw3-console/SKILL.md) for detailed console setup.

## Generate Setup Configuration File

The skill provides automatic configuration generation with detected values.

### Automatic Generation (Recommended)

```bash
# Generate ut-setup.sh with auto-detected values
.github/skills/gw3-unit-test-setup/generate-ut-setup.sh
```

**What gets detected:**
- DUT IP address (tries 192.168.1.1, then 192.168.254.254)
- PC ethernet interface
- Console session and port (if active)
- Setup status (DUT reachable, SSH working, etc.)

**Example output:**
```
Generating Unit Test Setup Configuration...
✓ Generated: /home/user/work/prplos/local/ut-setup.sh

Configuration Summary:
  DUT IP:      192.168.1.1
  PC IP:       192.168.1.2
  PC IF:       eth0
  Console:     /dev/ttyUSB0 (active)
  Ready:       true

Usage:
  source local/ut-setup.sh
  show_setup
```

### Using the Generated Configuration

```bash
# Load test environment
source local/ut-setup.sh

# Show configuration info
show_setup

# Use helper functions
dut_ping                    # Ping the DUT
dut_ssh 'uname -a'         # SSH to DUT
dut_console 'uptime'       # Execute via console
verify_setup               # Run verification
```

### Manual Customization

Edit the generated file if needed:

```bash
# Open with editor
nano local/ut-setup.sh

# Example customizations:
# - Change DUT_MODEL from "WF-728N" to "WF-728A"
# - Update DUT_LAN_IP if using different subnet
# - Adjust CONSOLE_PORT if using different serial device
```

After editing, reload:

```bash
source local/ut-setup.sh
show_setup
```

```bash
# Open with your preferred editor
nano local/ut-setup.sh
# or
vim local/ut-setup.sh
# or
code local/ut-setup.sh
```

**Update variables for:**
- DUT model (WF-728N or WF-728A)
- Network configuration (IPs, interfaces)
- PC interface names (ethernet, wifi)
- IP addresses and network configurations
- WiFi SSID and connection details
- Any custom passwords or configurations
- Additional setup notes

### Automated Information Gathering

**Use this script to auto-populate some values:**

```bash
# Create a script to gather system information
cat > local/gather-setup-info.sh << 'SCRIPT'
#!/bin/bash

echo "=== Gateway 3 Unit Test Setup Information ==="
echo ""

echo "## PC Information"
echo "- OS: $(uname -s) $(uname -r)"
echo "- Hostname: $(hostname)"
echo "- Date: $(date +"%Y-%m-%d %H:%M:%S")"
echo ""

echo "## Network Interfaces"
echo "### Ethernet Interfaces:"
ip -brief addr show | grep -E "^e" | while read iface state addr rest; do
    echo "- Interface: $iface"
    echo "  State: $state"
    echo "  IP: $addr"
    mac=$(ip link show $iface | grep "link/ether" | awk '{print $2}')
    echo "  MAC: $mac"
    echo ""
done

echo "### WiFi Interfaces:"
ip -brief addr show | grep -E "^w" | while read iface state addr rest; do
    echo "- Interface: $iface"
    echo "  State: $state"
    echo "  IP: $addr"
    mac=$(ip link show $iface | grep "link/ether" | awk '{print $2}')
    echo "  MAC: $mac"
    if command -v iwgetid &> /dev/null; then
        ssid=$(iwgetid -r 2>/dev/null)
        echo "  SSID: ${ssid:-N/A}"
    fi
    echo ""
done

echo "## Console Access"
# Check if console session is active
if .github/skills/gw3-console/list_consoles.sh 2>/dev/null | grep -q "gw3-console"; then
    echo "- Console session: ACTIVE"
    echo "- Run: .github/skills/gw3-console/list_consoles.sh for details"
else
    echo "- Console session: NOT ACTIVE"
    echo "- Run: .github/skills/gw3-console/auto_console.sh to setup"
fi
echo ""

echo "## DUT Connectivity"
if ping -c 1 -W 2 192.168.1.1 &>/dev/null; then
    echo "- DUT Ping (192.168.1.1): SUCCESS"
    
    # Try to get DUT information via SSH (requires passwordless or known_hosts)
    echo ""
    echo "### DUT Information (if SSH accessible):"
    ssh -o StrictHostKeyChecking=no -o ConnectTimeout=3 root@192.168.1.1 '
        echo "- Hostname: $(cat /proc/sys/kernel/hostname)"
        echo "- Kernel: $(uname -r)"
        echo "- Uptime: $(uptime | cut -d" " -f4-5 | sed "s/,$//")"
        echo ""
        echo "WAN Interface:"
        ip -brief addr show eth0 2>/dev/null | head -1
        echo ""
        echo "LAN Interface:"
        ip -brief addr show br-lan 2>/dev/null | head -1
    ' 2>/dev/null || echo "- SSH access not available (password required or not configured)"
else
    echo "- DUT Ping (192.168.1.1): FAILED"
    echo "  Check DUT power and LAN connection"
fi

echo ""
echo "=== End of Setup Information ==="

SCRIPT

chmod +x local/gather-setup-info.sh

# Run the script
./local/gather-setup-info.sh
```

**Save output to setup file:**

```bash
# Append gathered information to setup notes
./local/gather-setup-info.sh >> local/ut-setup-info.txt

# Review the information
cat local/ut-setup-info.txt
```

## Using the Setup Configuration

Once `local/ut-setup.sh` is created, it can be referenced by:

### 1. Source in Scripts

Skills can source the configuration to load environment variables:

```bash
# Check if setup file exists and source it
if [ -f local/ut-setup.sh ]; then
    source local/ut-setup.sh
    echo "Test setup loaded: DUT at ${DUT_LAN_IP}"
else
    echo "Run gw3-unit-test-setup skill first"
fi
```

### 2. Test Automation Scripts

Use exported variables directly:

```bash
# Load test environment
source local/ut-setup.sh

# Use helper functions
dut_ping
dut_ssh 'uname -a'
dut_console 'ip addr show br-lan'

# Use exported variables
echo "Testing DUT at $DUT_LAN_IP"
ssh "${DUT_SSH_USER}@${DUT_SSH_HOST}" 'uptime'
```

### 3. User Reference

View setup information:

```bash
# Source and show info
source local/ut-setup.sh && show_setup

# Update after configuration changes
nano local/ut-setup.sh
```

## Common Setup Scenarios

### Scenario 1: PC with Single Ethernet Interface

When PC has only one Ethernet port:

**Option A**: Use USB Ethernet adapter for DUT connection
- Primary Ethernet: Internet via router
- USB Ethernet: DUT connection

**Option B**: WiFi for Internet, Ethernet for DUT
- WiFi: Internet connectivity
- Ethernet: DUT connection (recommended)

### Scenario 2: DUT DHCP Not Working

If DUT DHCP server is not running or DUT is in U-Boot:

```bash
# Configure PC with fallback static IP
sudo ip addr add 192.168.254.2/24 dev eth0
sudo ip link set eth0 up

# Test connectivity to DUT fallback IP
ping 192.168.254.254

# If DUT responds, SSH and check DHCP server (if system is booted)
ssh root@192.168.254.254
uci show dhcp
/etc/init.d/dnsmasq restart
```

**Note**: In U-Boot, configure DUT IP:
```
# In U-Boot console
setenv ipaddr 192.168.254.254
setenv netmask 255.255.255.0
saveenv
```

### Scenario 3: No Console Access

Console is optional but helpful for debugging:

**Workarounds without console:**
- Use SSH for most operations
- Use network boot for recovery
- Use web interface if available
- Use TFTP for firmware updates

### Scenario 4: Testing Multiple DUTs

For multiple DUT setup:

```bash
# Create separate setup files
local/ut-setup-dut1.md
local/ut-setup-dut2.md

# Use different IP ranges
# DUT1: 192.168.1.x
# DUT2: 192.168.2.x (change DUT LAN IP first)
```

## Troubleshooting

### Cannot Ping DUT

**Check:**
1. Physical cable connection (both ends)
2. DUT power and boot status (wait 2 minutes after power on)
3. PC ethernet interface status (`ip link show`)
4. Firewall rules on PC (`sudo iptables -L`)
5. Network manager interference (disable for test interface)

**Debug:**
```bash
# Check ethernet link
ethtool eth0 | grep "Link detected"

# Check ARP table for DUT (any subnet)
arp -n

# Listen for DHCP offers
sudo tcpdump -i eth0 -n port 67 or port 68

# If no DHCP, use fallback static IP
sudo ip addr add 192.168.254.2/24 dev eth0
ping 192.168.254.254
```

### SSH Connection Refused

**Check:**
1. DUT has fully booted (wait longer)
2. SSH server is running on DUT
3. Correct IP address (192.168.1.1)
4. No firewall blocking SSH on DUT

**Debug via console:**
```bash
# On DUT console
/etc/init.d/dropbear status
/etc/init.d/dropbear restart

# Check if SSH port is listening
netstat -ln | grep :22
```

### Console Not Working

See [gw3-console](../gw3-console/SKILL.md) troubleshooting section for:
- Device detection issues
- Permission problems
- Wrong serial parameters
- TX/RX pin swap

## Best Practices

1. **Keep setup file updated**: Update `local/ut-setup.sh` when configuration changes
2. **Source before testing**: Always `source local/ut-setup.sh` at the start of test scripts
2. **Document changes**: Add notes about any modifications to default setup
3. **Version control**: Consider backing up setup file (but not in git if it contains sensitive info)
4. **Multiple setups**: Create separate setup files for different test environments
5. **Verification script**: Run verification checks before starting tests
6. **Power cycle routine**: Document power cycle procedure for DUT
7. **Console access**: Keep console connected for debugging even if not always used

## Quick Reference Commands

```bash
# Get DUT IP from DHCP gateway
DUT_IP=$(ip route show | grep default | awk '{print $3}')
# Or use fallback
DUT_IP="192.168.254.254"

# Verify DUT connectivity
ping -c 4 $DUT_IP

# SSH to DUT
ssh root@$DUT_IP

# Check console status
.github/skills/gw3-console/auto_console.sh

# Execute command on DUT via console
.github/skills/gw3-console/console_exec.sh ttyUSB0 'ip addr show'

# Check PC network
ip addr show

# Gather setup information
./local/gather-setup-info.sh

# View setup configuration
cat local/ut-setup.md

# Edit setup configuration
nano local/ut-setup.md
```

## Helper Scripts

### configure-pc-network.sh

Automatically configures PC network interface to match DUT subnet.

```bash
# Usage
.github/skills/gw3-unit-test-setup/configure-pc-network.sh [DUT_IP]

# Examples
.github/skills/gw3-unit-test-setup/configure-pc-network.sh 192.168.1.1
.github/skills/gw3-unit-test-setup/configure-pc-network.sh 192.168.254.254
```

**Features:**
- Auto-detects Ethernet interface (eth0, enp*, ens*, eno*)
- Calculates correct PC IP in same subnet (DUT .1 → PC .2)
- Flushes conflicting IPs
- Tests connectivity to DUT

### verify-setup.sh

Verifies complete unit test environment setup.

```bash
# Usage
.github/skills/gw3-unit-test-setup/verify-setup.sh
```

**Checks:**
- Console session status
- PC network configuration and subnet matching
- DUT connectivity (192.168.1.1 and 192.168.254.254)
- SSH access to DUT
- Internet connectivity

### configure-pc-network.sh

Automatically configures PC network interface to match DUT subnet.

```bash
# Usage
.github/skills/gw3-unit-test-setup/configure-pc-network.sh [DUT_IP]

# Examples
.github/skills/gw3-unit-test-setup/configure-pc-network.sh 192.168.1.1
.github/skills/gw3-unit-test-setup/configure-pc-network.sh 192.168.254.254
```

**Features:**
- Auto-detects Ethernet interface (eth0, enp*, ens*, eno*)
- Calculates correct PC IP in same subnet (DUT .1 → PC .2)
- Flushes conflicting IPs
- Tests connectivity to DUT

## Integration with Other Skills

### With gw3-build

After building firmware, use setup file to identify flash target:

```bash
# Read DUT model from setup
DUT_MODEL=$(grep "Model:" local/ut-setup.md | cut -d: -f2 | tr -d ' ')

# Use appropriate firmware image
ls bin/targets/ipq54xx/generic/*$DUT_MODEL*.img
```

### With gw3-console

Use console for DUT command execution:

```bash
# Auto-detect and setup console
.github/skills/gw3-console/auto_console.sh

# Execute commands on DUT
.github/skills/gw3-console/console_exec.sh ttyUSB0 'hostname'
.github/skills/gw3-console/console_capture.sh ttyUSB0 'ip addr' 2

# For interactive access (human only)
screen -r gw3-console-ttyUSB0
```

### With Test Automation

Parse setup file for test parameters:

```python
# Python example
def load_test_setup():
    config = {}
    with open('local/ut-setup.md', 'r') as f:
        for line in f:
            if 'Model:' in line:
                config['dut_model'] = line.split(':')[1].strip()
            if 'LAN IP:' in line:
                config['dut_ip'] = line.split(':')[1].strip().split('/')[0]
    return config

setup = load_test_setup()
print(f"Testing {setup['dut_model']} at {setup['dut_ip']}")
```

## Summary

The gw3-unit-test-setup skill helps establish a documented, reproducible test environment for Gateway 3 development and testing. The generated `local/ut-setup.md` file serves as:

- **Reference documentation** for the test setup
- **Configuration source** for test automation
- **Troubleshooting guide** with recorded working configuration
- **Communication tool** for sharing setup details with team

Always verify the setup before starting tests and keep the configuration file updated as the environment changes.
