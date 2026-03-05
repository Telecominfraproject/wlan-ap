# gw3-console Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | screen, bash, usbipd-win (WSL only) |

## 1. Overview

The `gw3-console` skill provides serial console access to Gateway 3 DUT (Device Under Test)
hardware via UART. It manages `screen` sessions for persistent serial connections, supports
non-interactive command execution and output capture for AI-assisted testing, handles WSL
USB passthrough, and logs all console output for post-mortem analysis.

**Users:** Developers and AI assistants performing device debugging, testing, and development.

**When to use:**
- Connecting to a GW3 device's serial console for interactive debugging
- Running commands on the DUT from the HOST without SSH (pre-network or recovery scenarios)
- Capturing DUT output for automated test workflows
- Setting up USB serial passthrough in WSL environments

## 2. Architecture

### 2.1 Directory Layout

```
.github/skills/gw3-console/
  SKILL.md                  # Skill documentation
  spec.md                   # This specification
  attach_uart_wsl.sh        # WSL USB passthrough (usbipd-win)
  auto_console.sh           # Auto-detect port + start/status
  start_console.sh          # Start named screen session with logging
  console_exec.sh           # Send command to running session
  console_capture.sh        # Execute command + capture screen output
  list_consoles.sh          # List active sessions and available ports
  stop_all_consoles.sh      # Terminate all gw3-console sessions
```

### 2.2 Key Components

| Component | Role |
|-----------|------|
| `screen` | Terminal multiplexer for persistent serial sessions |
| `usbipd-win` | USB/IP passthrough from Windows host to WSL (WSL only) |
| `/dev/ttyUSB*`, `/dev/ttyACM*` | Linux serial device nodes |
| `local/records/` | Log storage directory (gitignored) |

### 2.3 Session Naming Convention

All screen sessions follow the pattern: `gw3-console-<port_name>`

Examples: `gw3-console-ttyUSB0`, `gw3-console-ttyACM0`, `gw3-console-ttyUSB1`

### 2.4 Data Flow

```
HOST terminal
  -> screen session (gw3-console-ttyUSB0)
    -> /dev/ttyUSB0 (115200 8N1)
      -> UART cable (3.3V TTL)
        -> DUT serial port

Logging: screen -L -> local/records/console_ttyUSB0_YYYYMMDD_HHMMSS.log
Capture: screen hardcopy -> local/records/console_ttyUSB0_cap_<epoch>.txt
```

## 3. Interface / Subcommands

### 3.1 attach_uart_wsl.sh

Attach USB serial device from Windows host to WSL via `usbipd-win`.

```
attach_uart_wsl.sh [busid]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `busid` | *(auto-detect)* | USB bus ID (e.g., `2-3`). Auto-detects UART devices if omitted. |

**Output:** Device attachment confirmation with `/dev/tty*` verification.

**Auto-detection patterns:** `serial`, `uart`, `ftdi`, `cp210`, `ch340`, `prolific`

### 3.2 auto_console.sh

Auto-detect serial port and start console session if not already running.

```
auto_console.sh [port]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `port` | First `/dev/ttyUSB*` or `/dev/ttyACM*` found | Full device path |

**Behavior:**
- If no port found and running in WSL, automatically calls `attach_uart_wsl.sh`
- If session already exists, prints status (port, log path, attach command)
- If no session exists, starts one via `start_console.sh`

### 3.3 start_console.sh

Start a new screen session with automatic logging.

```
start_console.sh [port] [baud]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `port` | `/dev/ttyUSB0` | Serial device path |
| `baud` | `115200` | Baud rate |

**Creates:** `local/records/console_<port_name>_<timestamp>.log`

**Idempotent:** If session already exists, prints message and exits with 0.

### 3.4 console_exec.sh

Send a command to a running console session without capturing output.

```
console_exec.sh [port_name] 'command' [wait]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `port_name` | `ttyUSB0` | Port name (not full path, e.g., `ttyUSB0`) |
| `command` | *(required)* | Command string to execute on DUT |
| `wait` | `1` | Seconds to wait after sending command |

**Port detection:** If first argument matches `^tty`, it is treated as port name;
otherwise `ttyUSB0` is assumed and the first argument is the command.

**Mechanism:** Uses `screen -S <session> -X stuff "$CMD\r"` to inject keystrokes.

### 3.5 console_capture.sh

Execute a command and capture the screen output to a file.

```
console_capture.sh [port_name] 'command' [wait]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `port_name` | `ttyUSB0` | Port name |
| `command` | *(required)* | Command string to execute on DUT |
| `wait` | `2` | Seconds to wait before capturing (longer default than exec) |

**Output:** Captured text printed to stdout. Also saved to
`local/records/console_<port>_cap_<epoch>.txt`.

**Mechanism:** Sends command via `screen stuff`, waits, then uses `screen hardcopy`
to dump the visible terminal buffer to a file.

### 3.6 list_consoles.sh

List all active `gw3-console-*` screen sessions and available serial ports.

```
list_consoles.sh
```

**Output sections:**
1. **Active Sessions** -- session name, port, log file path
2. **Available Ports** -- each `/dev/ttyUSB*` and `/dev/ttyACM*` with active/idle status

### 3.7 stop_all_consoles.sh

Terminate all `gw3-console-*` screen sessions.

```
stop_all_consoles.sh
```

**Mechanism:** Finds all screen sessions matching `gw3-console` and sends `quit` command.

## 4. Logic / Workflow

### 4.1 WSL USB Attachment

1. Verify running in WSL (check `/proc/version` for `WSL|Microsoft`)
2. Verify `usbipd.exe` is available
3. If no busid provided, list USB devices matching UART patterns
4. If single device found, auto-select; if multiple, prompt user to choose
5. Bind device if not already shared: `usbipd.exe bind --busid <busid>`
6. Attach to WSL: `usbipd.exe attach --wsl --busid <busid>`
7. Wait 2 seconds, verify `/dev/ttyUSB*` or `/dev/ttyACM*` appears

### 4.2 Console Session Startup

1. Validate port device exists
2. Check for existing session (`screen -ls | grep gw3-console-<port>`)
3. Create log directory `local/records/` if needed
4. Start detached screen session with logging:
   `screen -L -Logfile <log> -dmS gw3-console-<port> <port> <baud>`
5. Verify session started successfully after 0.5s delay

### 4.3 Non-Interactive Command Execution

1. Determine port name (from argument or default `ttyUSB0`)
2. Verify target screen session exists
3. Inject command via `screen -S <session> -X stuff "$CMD\r"`
4. Wait specified duration for command to complete
5. (capture only) Dump terminal buffer via `screen -S <session> -X hardcopy <file>`
6. (capture only) Print captured output to stdout

### 4.4 Automated Testing Pattern

```bash
auto_console.sh                        # Ensure console running
console_exec.sh 'ubus call system board'
console_capture.sh 'logread -l 50' 3   # Capture last 50 log lines
tail -100 local/records/console_ttyUSB0_*.log | grep -i error
```

### 4.5 Multi-DUT Testing Pattern

```bash
for p in /dev/ttyUSB*; do start_console.sh $p; done
for p in ttyUSB{0..3}; do console_exec.sh $p 'hostname'; done
```

## 5. Safety Rules

### 5.1 Port Validation

- All scripts that access serial ports validate device existence before proceeding
- `auto_console.sh` falls back to WSL USB attachment if no port found
- Missing port results in error message listing available devices

### 5.2 Session Management

- `start_console.sh` is idempotent: refuses to create duplicate sessions
- `stop_all_consoles.sh` only targets `gw3-console-*` sessions (does not affect
  other screen sessions)
- Sessions are started in detached mode (`-dm`) to avoid blocking the calling terminal

### 5.3 Logging

- All console sessions automatically log to `local/records/` (gitignored)
- Log filenames include port name and timestamp for uniqueness
- Capture files use epoch timestamps to avoid collisions during rapid execution
- The `local/` directory must never be committed to git

### 5.4 WSL-Specific

- WSL detection uses `/proc/version` grep, not environment variables
- `usbipd.exe` availability is checked before attempting USB operations
- Device binding (`usbipd bind`) is a prerequisite for attachment; script handles
  this automatically if not already done
- Post-attachment verification waits 2 seconds for driver initialization

### 5.5 UART Hardware

- **Voltage:** 3.3V TTL only. Never connect 5V adapters to the DUT.
- **Parameters:** 115200 baud, 8N1 (8 data bits, no parity, 1 stop bit)
- **Compatible adapters:** FTDI FT232RL, CP2102, CH340G (3.3V models only)

## 6. Dependencies

### 6.1 Required

| Dependency | Purpose |
|------------|---------|
| `screen` | Terminal multiplexer for serial sessions |
| `bash` | Script runtime |
| Serial device (`/dev/ttyUSB*` or `/dev/ttyACM*`) | Physical UART connection |

### 6.2 WSL Only

| Dependency | Purpose |
|------------|---------|
| `usbipd-win` | USB/IP passthrough from Windows to WSL |
| WSL2 kernel with USB serial drivers | Device node creation |

### 6.3 Hardware

| Component | Specification |
|-----------|---------------|
| UART adapter | 3.3V TTL, FTDI/CP2102/CH340G |
| Serial cable | TX, RX, GND (3 wires minimum) |
| DUT serial header | Board-specific pinout (see hardware docs) |

### 6.4 Integration

| Component | Purpose |
|-----------|---------|
| `local/ut-setup.sh` | Unit test environment configuration (cross-agent console discovery) |
| `local/records/` | Shared log directory with other GW3 skills |

## 7. Screen Session Quick Reference

| Action | Command |
|--------|---------|
| List sessions | `screen -ls` |
| Attach to session | `screen -r gw3-console-ttyUSB0` |
| Detach (inside screen) | `Ctrl-A` then `D` |
| Kill specific session | `screen -S gw3-console-ttyUSB0 -X quit` |
| Scroll mode (inside screen) | `Ctrl-A` then `[`, arrow keys to scroll, `q` to exit |

## 8. Future Work

- Automatic reconnection on USB disconnect/reconnect
- Output parsing and structured result return for AI-driven testing
- Support for multiple baud rates per session (e.g., bootloader vs OS)
- Integration with `gw3-upgrade` for serial-based recovery flashing
- Timeout and retry logic for `console_capture.sh` when DUT is unresponsive
