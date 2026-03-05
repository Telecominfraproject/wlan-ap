---
name: ssh-connection
description: Manage SSH connections to DUT devices through Jumphost or direct connection. Configure and execute remote commands on OpenSync devices. Use when connecting to devices, managing SSH credentials, or executing remote commands.
---

# SSH Connection Management Skill

This skill helps you manage SSH connections to DUT (Device Under Test) devices, with support for both direct connections and connections through a Jumphost server.

## Capabilities

- **Connect to Jumphost** and execute remote commands
- **Connect to DUT** devices through Jumphost or directly
- **Configure connection settings** via configuration file
- **Support multiple authentication methods** (password and SSH key)
- **Manage network interfaces** on Jumphost for DUT connectivity

## When to Use This Skill

Use this skill when you need to:
- Connect to OpenSync DUT devices for testing or debugging
- Execute commands on remote devices through a Jumphost
- Configure SSH connection settings for different devices
- Set up network interfaces on Jumphost for DUT access
- Check device status or retrieve logs remotely

## Available Scripts

### 1. ssh2jumphost.sh - Connect to Jumphost

Connects to the Jumphost server and executes commands.

**Usage:**
```bash
./ssh2jumphost.sh <PROJECT> <command>
```
PROJECT: WF728, WF710G, WF810, WF710, WF630

**Examples:**
```bash
# Check network interfaces
./ssh2jumphost.sh WF728 "ip addr show"

# Check system status
./ssh2jumphost.sh WF728 "uptime"

# Execute multiple commands
./ssh2jumphost.sh WF728 "ls -la /home && pwd"
```

**Configuration:**
- `WF728_JUMPHOST_SSH_USER`: SSH username for Jumphost
- `WF728_JUMPHOST_SSH_PASSWORD`: SSH password for Jumphost
- `WF728_JUMPHOST_SSH_HOST`: Jumphost IP address

### 2. ssh2jumphost2dut.sh - Connect to DUT

Connects to DUT devices either through Jumphost or directly.

**Usage:**
```bash
./ssh2jumphost2dut.sh <PROJECT> <command>
```
PROJECT: WF728, WF710G, WF810, WF710, WF630

**Examples:**
```bash
# Check DUT status
./ssh2jumphost2dut.sh WF728 "uptime"

# View logs
./ssh2jumphost2dut.sh WF728 "tail -f /var/log/messages"

# Get DUT version
./ssh2jumphost2dut.sh WF728 "cat /etc/os-release"

# Check processes
./ssh2jumphost2dut.sh WF728 "ps -xj"
```

**Modes:**
- **Jumphost Mode** (`USE_JUMPHOST=yes`): Connect to DUT through Jumphost
  - Automatically sets up network interface on Jumphost
  - Creates SSH tunnel to DUT
  - Suitable for remote access scenarios

- **Direct Mode** (`USE_JUMPHOST=no`): Direct connection to DUT
  - Connect to DUT without intermediate server
  - Suitable for local network access

**Configuration** (per-project prefix, e.g. `WF728_`):
- `USE_JUMPHOST`: Enable/disable Jumphost mode (global)
- `WF728_DUT_SSH_USER`: SSH username for DUT
- `WF728_DUT_SSH_PASSWORD`: SSH password (empty for SSH key auth)
- `WF728_DUT_SSH_HOST`: DUT LAN IP address
- `WF728_JUMPHOST_DUT_INTERFACE`: Network interface on Jumphost for DUT connection
- `WF728_JUMPHOST_DUT_IP`: Static IP for Jumphost-DUT connection

### 3. ssh_dut.conf - Configuration File

Central configuration file for all SSH connection settings.

**Configuration Sections:**

#### Connection Mode
```properties
USE_JUMPHOST=yes  # Use Jumphost mode (yes/no)
```

#### Jumphost Settings
```properties
JUMPHOST_SSH_USER=actiontec
JUMPHOST_SSH_PASSWORD=Hugh1234@AEI
JUMPHOST_SSH_HOST=172.16.10.81
JUMPHOST_SUDO_PASSWORD=Hugh1234@AEI
JUMPHOST_DUT_INTERFACE=enp1s0f1:1
JUMPHOST_DUT_IP=192.168.40.100
```

#### DUT Settings
```properties
DUT_SSH_USER=osync
DUT_SSH_PASSWORD=osync123  # Leave empty for SSH key auth
DUT_SSH_HOST=192.168.40.1
```

## Workflow Examples

### Example 1: Check DUT Status Through Jumphost

```
User: "连接到 DUT 并检查系统状态"
Agent: 执行命令...
       [./ssh2jumphost2dut.sh WF728 "uptime && cat /proc/version"]
       显示 DUT 的运行时间和系统版本
```

### Example 2: View OpenSync Logs

```
User: "查看 DUT 的 OpenSync 日志"
Agent: [./ssh2jumphost2dut.sh WF728 "tail -100 /var/log/messages | grep -i opensync"]
       显示最近 100 行包含 opensync 的日志
```

### Example 3: Execute ovsh Commands

```
User: "查看 DUT 的 WiFi 配置"
Agent: [./ssh2jumphost2dut.sh WF728 "ovsh s Wifi_Radio_Config"]
       显示 WiFi 射频配置信息
```

### Example 4: Update Configuration

```
User: "更新 DUT 的 SSH 密码为 newpassword"
Agent: 修改 ssh_dut.conf 文件中的 DUT_SSH_PASSWORD=newpassword
       配置已更新
```

## Features

### Automatic Network Setup
When using Jumphost mode, the script automatically:
1. Checks if network interface exists
2. Creates interface if needed
3. Assigns static IP for DUT connection
4. Verifies connectivity before proceeding

### Flexible Authentication
Supports two authentication methods:
- **Password authentication**: Set `DUT_SSH_PASSWORD` in config
- **SSH Key authentication**: Leave `DUT_SSH_PASSWORD` empty

### Error Handling
- Validates configuration file
- Checks SSH connectivity
- Reports detailed error messages
- Automatic cleanup on failure

## Security Notes

⚠️ **Important Security Considerations:**
- Configuration file contains sensitive credentials
- Keep `ssh_dut.conf` secure and restrict access
- Consider using SSH keys instead of passwords
- Do not commit credentials to version control

## Troubleshooting

### Connection Fails
1. Check network connectivity: `ping <DUT_IP>`
2. Verify credentials in `ssh_dut.conf`
3. Test Jumphost connection: `./ssh2jumphost.sh "hostname"`
4. Check interface setup: `./ssh2jumphost.sh "ip addr show"`

### Authentication Errors
1. Verify username and password
2. Check if SSH key is properly configured
3. Ensure DUT allows password/key authentication

### Network Interface Issues
1. Check interface name in config
2. Verify sudo password
3. Test manually: `sudo ip addr add <IP> dev <INTERFACE>`

## Advanced Usage

### Custom Commands with Variables
```bash
# Get specific log entries
./ssh2jumphost2dut.sh WF728 "grep 'ERROR' /var/log/messages | tail -20"

# Check specific process
./ssh2jumphost2dut.sh WF728 "ps aux | grep opensync"

# Copy files (with scp)
# Note: Use scp separately for file transfers
```

### Direct Mode for Local Testing
```bash
# Switch to direct mode in ssh_dut.conf
USE_JUMPHOST=no
DUT_SSH_HOST=192.168.1.100  # Update to direct IP

# Then execute commands
./ssh2jumphost2dut.sh WF728 "your command"
```

## Integration with Other Skills

This SSH skill works well with:
- **universal-build**: Build firmware then deploy and test on DUT
- **log-analysis**: Retrieve and analyze logs from DUT
- **device-testing**: Execute automated tests on DUT

## File Locations

All skill files are located in:
```
.github/skills/ssh-connection/
├── SKILL.md                # This documentation
├── ssh2jumphost.sh         # Jumphost connection script
├── ssh2jumphost2dut.sh     # DUT connection script
└── ssh_dut.conf            # Configuration file
```

## Quick Reference

| Task | Command |
|------|---------|
| Test Jumphost | `./ssh2jumphost.sh WF728 "hostname"` |
| Test DUT | `./ssh2jumphost2dut.sh WF728 "hostname"` |
| View config | `cat ssh_dut.conf` |
| Check processes | `./ssh2jumphost2dut.sh WF728 "ps -xj"` |
| View logs | `./ssh2jumphost2dut.sh WF728 "tail -f /var/log/messages"` |
| Get version | `./ssh2jumphost2dut.sh WF728 "cat /etc/os-release"` |

---

**配置日期**: 2026-01-14  
**维护者**: Hugh Cheng  
**Skill 版本**: 1.0
