---
name: gw3-console
description: Connect to Gateway 3 DUT via UART serial console
---

# Gateway 3 Console Access

Quick access to Gateway 3 device serial console for debugging, testing, and development.

## Quick Start

```bash
# On WSL: Attach USB device first (auto-detects UART)
attach_uart_wsl.sh

# Auto-start console (detects port, starts if needed)
auto_console.sh

# Send command (non-interactive)
console_exec.sh 'uname -a'

# Execute and capture output
console_capture.sh 'ip addr show' 2

# Attach interactively
screen -r gw3-console-ttyUSB0

# Detach: Ctrl-A then D
```

## Scripts

All scripts in `.github/skills/gw3-console/`:

### auto_console.sh
Auto-detect port and start console if not running.

```bash
auto_console.sh [port]           # Auto-detects if port not specified
```

### start_console.sh
Manually start console with logging.

```bash
start_console.sh [port] [baud]   # Default: /dev/ttyUSB0 115200
```

### console_exec.sh
Send command without interrupting session.

```bash
console_exec.sh [port] 'cmd' [wait]   # Default port: ttyUSB0, wait: 1s
console_exec.sh 'ip addr'             # Uses default port
console_exec.sh ttyUSB1 'ps' 2        # Specific port, 2s wait
```

### console_capture.sh
Execute command and capture output.

```bash
console_capture.sh [port] 'cmd' [wait]  # Default wait: 2s
console_capture.sh 'uname -a'
console_capture.sh ttyUSB0 'ifconfig' 3
```

### list_consoles.sh
List active sessions and available ports.

```bash
list_consoles.sh
```

### stop_all_consoles.sh
Stop all console sessions.

```bash
stop_all_consoles.sh
```

### attach_uart_wsl.sh
Attach USB serial device to WSL with auto-detection (WSL only).

```bash
attach_uart_wsl.sh           # Auto-detect and attach UART device
attach_uart_wsl.sh <busid>   # Attach specific device (manual override)
```

## Hardware Setup

**UART Parameters:**
- Baud: 115200
- Format: 8N1 (8 data, no parity, 1 stop)
- Voltage: 3.3V TTL

**Adapters:** FTDI FT232RL, CP2102, CH340G (3.3V models)

**Device Detection:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

## WSL Setup

USB serial devices on WSL require `usbipd-win`.

**Quick setup (auto-detection):**
```bash
# Auto-detect and attach UART device
attach_uart_wsl.sh

# Start console
auto_console.sh
```

**With multiple UART devices:**
- Script will prompt you to select device
- Or specify busid manually: `attach_uart_wsl.sh <busid>`

**Manual setup:**
```bash
# Install usbipd-win (if not installed)
winget.exe install --interactive --exact dorssel.usbipd-win

# List devices
usbipd.exe list

# Bind and attach
usbipd.exe bind --busid <busid>
usbipd.exe attach --wsl --busid <busid>
```

## Multi-Port Support

Each port gets independent session:

```bash
# Start multiple consoles
start_console.sh /dev/ttyUSB0
start_console.sh /dev/ttyUSB1

# Use specific console
console_exec.sh ttyUSB0 'hostname'
console_exec.sh ttyUSB1 'ip addr'

# Attach to specific
screen -r gw3-console-ttyUSB0
screen -r gw3-console-ttyUSB1
```

## Log Files

All output logged to `local/records/`:

```bash
# View live
tail -f local/records/console_ttyUSB0_*.log

# Search
grep "error" local/records/console_*.log

# Size
du -h local/records/console_*.log
```

## Session Management

**Screen commands:**
```bash
# List sessions
screen -ls

# Attach
screen -r gw3-console-ttyUSB0

# Detach (inside screen)
Ctrl-A D

# Kill session
screen -S gw3-console-ttyUSB0 -X quit
```

## Integration

Scripts automatically work with `local/ut-setup.sh` for cross-agent console access discovery.

## Common Patterns

**Automated testing:**
```bash
# Start console
auto_console.sh

# Run test commands
console_exec.sh 'ubus call system board'
console_capture.sh 'logread -l 50' 3

# Check results from log
tail -100 local/records/console_ttyUSB0_*.log | grep -i error
```

**Interactive debugging:**
```bash
auto_console.sh              # Ensure console running
screen -r gw3-console-ttyUSB0  # Attach for interactive work
# Ctrl-A D to detach when done
```

**Multi-DUT testing:**
```bash
# Start all
for p in /dev/ttyUSB*; do start_console.sh $p; done

# Execute on all
for p in ttyUSB{0..3}; do
    console_exec.sh $p 'hostname'
done
```
