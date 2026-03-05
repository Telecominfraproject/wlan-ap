# gw3-unit-test-setup Specification

| Field | Value |
|-------|-------|
| Status | Active |
| Version | 1.0.0 |
| Created | 2026-02-20 |
| Depends | gw3-console, ip, ping, ssh, screen |

## Overview

The `gw3-unit-test-setup` skill generates and manages a unit test environment configuration file (`local/ut-setup.sh`) for Gateway 3 DUT testing. It auto-detects the network topology between a development PC and a GW3 device (WF-728N or WF-728A), configures the PC ethernet interface to match the DUT subnet, and exports shell variables and helper functions consumed by other skills and test scripts.

**Primary users**: Developers and AI assistants preparing a test bench before deploying or testing firmware on a physical DUT.

**When to use**: At the start of any test session, after powering on a DUT, or after network changes.

## Architecture

### Directory Layout

```
.github/skills/gw3-unit-test-setup/
  SKILL.md                  # Detailed usage documentation
  spec.md                   # This specification
  generate-ut-setup.sh      # Main generator script
  configure-pc-network.sh   # PC network configuration helper
  verify-setup.sh           # Environment verification checker
```

### Generated Artifact

```
local/ut-setup.sh           # Sourceable shell config (gitignored)
```

### Data Flow

```
generate-ut-setup.sh
  |-- detect console (screen -ls, /dev/ttyUSB*, auto_console.sh)
  |-- detect PC IP (ip addr show, 192.168.x.x scan)
  |-- detect DUT IP (ping .254, .1, .100 in discovered subnet)
  |-- detect PC ethernet interface (ip addr / ip route)
  |-- check status flags (ping, ssh)
  '--> write local/ut-setup.sh (template + sed placeholder replacement)

configure-pc-network.sh [DUT_IP]
  |-- find ethernet interface (/sys/class/net)
  |-- compute PC IP (DUT subnet + .2)
  |-- flush + assign IP (sudo ip addr)
  '--> test connectivity (ping)

verify-setup.sh
  |-- check console session
  |-- show PC network config
  |-- ping DUT (192.168.1.1, then 192.168.254.254)
  |-- verify PC in DUT subnet
  |-- test SSH access
  |-- test internet (8.8.8.8)
  '--> print summary + exit code
```

## Interface / Subcommands

### generate-ut-setup.sh

Auto-detects environment and writes `local/ut-setup.sh`.

```
Usage: .github/skills/gw3-unit-test-setup/generate-ut-setup.sh
Arguments: none
Output:    local/ut-setup.sh
Exit code: 0 (always; best-effort detection)
```

**Exported variables** (written into `local/ut-setup.sh`):

| Variable | Example | Description |
|----------|---------|-------------|
| `DUT_MODEL` | `WF-728N` | Gateway model (default) |
| `DUT_PLATFORM` | `Qualcomm IPQ5424` | SoC platform |
| `DUT_LAN_IP` | `192.168.1.1` | Detected DUT LAN address |
| `DUT_LAN_SUBNET` | `192.168.1.0/24` | DUT LAN subnet |
| `DUT_LAN_FALLBACK_IP` | `192.168.254.254` | U-Boot / no-DHCP address |
| `PC_ETH_IF` | `eth0` | PC ethernet interface name |
| `PC_ETH_IP` | `192.168.1.2` | PC IP in DUT subnet |
| `CONSOLE_PORT` | `/dev/ttyUSB0` | Serial device path |
| `CONSOLE_SESSION` | `gw3-console-ttyUSB0` | Screen session name |
| `CONSOLE_STATUS` | `active` / `inactive` | Console liveness |
| `DUT_SSH_USER` | `root` | SSH login user |
| `DUT_SSH_HOST` | `192.168.1.1` | SSH target (same as DUT_LAN_IP) |
| `SETUP_READY` | `true` / `false` | Overall readiness flag |

**Exported helper functions**:

| Function | Description |
|----------|-------------|
| `dut_ssh [cmd]` | SSH to DUT with stored credentials |
| `dut_console <cmd>` | Execute command via serial console |
| `dut_ping` | Ping DUT (3 packets) |
| `verify_setup` | Run verification script |
| `show_setup` | Print formatted configuration summary |

### configure-pc-network.sh

Configures the PC ethernet interface to be in the same subnet as the DUT.

```
Usage: .github/skills/gw3-unit-test-setup/configure-pc-network.sh [DUT_IP]
  DUT_IP   Target DUT IP address (default: 192.168.1.1)
Exit code: 0 = success, 1 = no interface found or config failed
Requires:  sudo (for ip addr flush/add, ip link set)
```

**Behavior**:
1. Extracts subnet prefix from `DUT_IP` (first 3 octets).
2. Sets `PC_IP` to `<subnet>.2`.
3. Searches `/sys/class/net` for first wired interface matching `eth0`, `enp*`, `ens*`, or `eno*` (skips wireless by checking for `phy80211` sysfs entry).
4. If interface already has an IP in the DUT subnet, reports success and skips.
5. Otherwise: disables NetworkManager management, flushes existing IPs, assigns `PC_IP/24`, brings interface up.
6. Pings DUT to confirm connectivity.

### verify-setup.sh

Runs a 5-step verification of the test environment.

```
Usage: .github/skills/gw3-unit-test-setup/verify-setup.sh
Exit code: 0 = all critical checks pass, 1 = one or more failures
```

**Verification steps**:

| Step | Check | Pass Criteria |
|------|-------|---------------|
| 1 | Console session | `screen -ls` shows `gw3-console-*` |
| 2 | PC network | Lists interfaces and IP addresses |
| 3 | DUT connectivity | Ping 192.168.1.1 or 192.168.254.254; verify PC in DUT subnet |
| 4 | SSH access | `ssh root@DUT 'echo ok'` succeeds (3 s timeout) |
| 5 | Internet | Ping 8.8.8.8 succeeds |

**Overall READY**: Console OK AND DUT ping OK AND SSH OK.

## Logic / Workflow

### DUT IP Detection (generate-ut-setup.sh)

1. Scan `ip addr show` for any `192.168.x.y` address on the PC.
2. If found, extract subnet number `x` and try pinging gateway candidates in order: `.254`, `.1`, `.100`.
3. First responding IP becomes `DUT_IP`. If none respond, default to `.254`.
4. If no PC IP exists in `192.168.x.x`, fall back to pinging well-known addresses: `192.168.254.254` then `192.168.1.1`.
5. Ultimate fallback: `DUT_IP=192.168.254.254`, `PC_IP=192.168.254.2`.

### Console Detection (generate-ut-setup.sh)

1. Check `screen -ls` for session matching `gw3-console-tty*`.
2. If found, extract session name and derive device path.
3. If not found, scan `/dev/ttyUSB*` and `/dev/ttyACM*`.
4. If a serial device exists, attempt to start a console via `gw3-console/auto_console.sh`.
5. Fallback defaults: `CONSOLE_PORT=/dev/ttyUSB0`, session name derived from port.

### Status Flag Evaluation (generate-ut-setup.sh)

1. `SETUP_CONSOLE_ACTIVE` -- `screen -ls` matches `gw3-console`.
2. `SETUP_DUT_DISCOVERED` / `SETUP_DUT_REACHABLE` -- single ping to `DUT_IP` succeeds.
3. `SETUP_PC_CONFIGURED` -- `ip addr show` contains `PC_IP`.
4. `SETUP_SSH_VERIFIED` -- `ssh root@DUT 'echo ok'` returns `ok` within 3 s.
5. `SETUP_READY` -- `DUT_REACHABLE` AND `SSH_VERIFIED` both true.

### Template Generation (generate-ut-setup.sh)

1. Write a heredoc template with `__PLACEHOLDER__` tokens into `local/ut-setup.sh`.
2. Run `sed -i` to replace each placeholder with the detected value.
3. `chmod +x` the output file.

## Safety Rules

- **Never modify files outside `local/`**. The generated config is always `local/ut-setup.sh` (gitignored).
- **`configure-pc-network.sh` requires sudo** for IP assignment. It will not silently escalate; `sudo` prompts apply.
- **NetworkManager is told to ignore the interface** (`nmcli device set ... managed no`) to prevent it from overriding the static IP. The `2>/dev/null || true` guard ensures non-fatal failure if nmcli is unavailable.
- **Ping and SSH timeouts are bounded**: ping uses `-W 1` (1 s), SSH uses `-o ConnectTimeout=2` or `ConnectTimeout=3` with a `timeout 3` wrapper. The script never blocks indefinitely.
- **`StrictHostKeyChecking=no`** is used for SSH to avoid interactive prompts; acceptable because DUT is on a private test LAN.
- **No destructive DUT operations**. All DUT interactions are read-only probes (ping, `echo ok`).

## Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| `gw3-console` skill | Skill | `auto_console.sh` for starting console sessions; `console_exec.sh` for helper function |
| `ip` (iproute2) | System | Network interface detection and configuration |
| `ping` | System | Connectivity checks |
| `ssh` / `scp` | System | DUT SSH probing |
| `screen` | System | Console session management |
| `sudo` | System | Required by `configure-pc-network.sh` for IP configuration |
| `nmcli` | Optional | Disabling NetworkManager on test interface |
| `ethtool` | Optional | Link detection during troubleshooting |
