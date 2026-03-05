# SSH Connection Management Skill / SSH 连接管理 Skill

[English](#english) | [中文](#中文)

---

# English

Manage SSH connections to OpenSync DUT devices through Jumphost or direct connection.

## Network Topology

```
Your Machine                 Jumphost                    DUT Device
┌─────────────┐             ┌─────────────┐            ┌─────────────┐
│             │             │             │            │             │
│  Developer  │────SSH─────▶│  172.16.   │────SSH────▶│  192.168.  │
│   Machine   │             │   10.81    │            │   40.1     │
│             │             │             │            │             │
│  (Local)    │             │ (Jumphost) │            │   (DUT)    │
└─────────────┘             └─────────────┘            └─────────────┘
      │                                                       │
      │                                                       │
      └───────────────────────Direct SSH (Optional)──────────┘
                           (When DUT is reachable)
```

### Connection Modes

**Mode 1: Through Jumphost (Recommended)**
- Your Machine → Jumphost (172.16.10.81) → DUT (192.168.40.1)
- Most secure and reliable
- Works even when DUT is not directly accessible
- Default configuration

**Mode 2: Direct Connection (Optional)**
- Your Machine → DUT (192.168.40.1)
- Requires DUT to be on the same network
- Faster but less flexible
- Set `USE_JUMPHOST=no` in config

### Network Details

| Component | IP Address | Port | User | Description |
|-----------|------------|------|------|-------------|
| **Jumphost** | 172.16.10.81 | 22 | plume/actiontec | SSH relay server |
| **DUT** | 192.168.40.1 | 22 | root/osync | OpenSync device under test |
| **Alternative DUT** | 192.168.50.1 | 22 | root | Different board configuration |

## Quick Start

### 1. Configure Connection Settings

Edit `ssh_dut.conf` to set your connection parameters:

```bash
# Connection Mode
CONNECTION_MODE="jumphost"  # or "direct"

# Jumphost Configuration
JUMPHOST_HOST="172.16.10.81"
JUMPHOST_USER="plume"
JUMPHOST_PASS="plume"
JUMPHOST_AUTH_METHOD="password"  # or "key"

# DUT Configuration
DUT_HOST="192.168.40.1"
DUT_USER="root"
DUT_PASS="OpenSync@#!2022"

# Network Configuration
DUT_INTERFACE="eth1"
JUMPHOST_INTERFACE="enp5s0"
```

### 2. Test Connection

```bash
# Test Jumphost connection
./ssh2jumphost.sh "hostname"

# Test DUT connection through Jumphost
./ssh2jumphost2dut.sh "hostname"

# Check DUT network interface
./ssh2jumphost2dut.sh "ifconfig"
```

### 3. Common Operations

```bash
# System Information
./ssh2jumphost2dut.sh "uname -a"
./ssh2jumphost2dut.sh "uptime"
./ssh2jumphost2dut.sh "free -m"
./ssh2jumphost2dut.sh "df -h"

# OpenSync Commands
./ssh2jumphost2dut.sh "ovsh s Wifi_Radio_State"
./ssh2jumphost2dut.sh "ovsh s Wifi_VIF_State"
./ssh2jumphost2dut.sh "ovsh s Connection_Manager_Uplink"

# Logs
./ssh2jumphost2dut.sh "logread | grep -i opensync | tail -20"
./ssh2jumphost2dut.sh "tail -50 /var/log/messages"

# Network Diagnostics
./ssh2jumphost2dut.sh "ping -c 3 8.8.8.8"
./ssh2jumphost2dut.sh "iwconfig"
```

## Files

- **README.md** - This file
- **SKILL.md** - Complete documentation for GitHub Copilot
- **ssh2jumphost.sh** - Connect to Jumphost and execute commands
- **ssh2jumphost2dut.sh** - Connect to DUT through Jumphost or directly
- **ssh_dut.conf** - Connection configuration file

## Usage with GitHub Copilot

In Copilot Chat, you can use natural language:

```
"Connect to DUT and check system status"
"View DUT OpenSync logs"
"Execute ovsh s Wifi_Radio_Config on DUT"
"Check DUT network connectivity"
```

## Troubleshooting

### Cannot Connect to Jumphost

```bash
# Test connectivity
ping -c 3 172.16.10.81

# Check credentials in ssh_dut.conf
cat ssh_dut.conf | grep JUMPHOST

# Test manual SSH
ssh plume@172.16.10.81
```

### Cannot Connect to DUT from Jumphost

```bash
# From Jumphost, test DUT connectivity
./ssh2jumphost.sh "ping -c 3 192.168.40.1"

# Check DUT interface on Jumphost
./ssh2jumphost.sh "ifconfig enp5s0"

# Try direct connection (if on same network)
ssh root@192.168.40.1
```

### Network Configuration Issues

```bash
# Configure Jumphost network interface
./ssh2jumphost.sh "sudo ifconfig enp5s0 192.168.40.100 netmask 255.255.255.0 up"

# Check routing
./ssh2jumphost.sh "route -n"
```

## Security Warning

⚠️ The configuration file contains sensitive credentials. Keep it secure:
- Restrict file permissions: `chmod 600 ssh_dut.conf`
- Do not commit to public repositories
- Use SSH keys instead of passwords (recommended):

```bash
# Generate SSH key
ssh-keygen -t rsa -b 4096 -C "your_email@example.com"

# Copy to Jumphost
ssh-copy-id plume@172.16.10.81

# Update config
# Set JUMPHOST_AUTH_METHOD="key"
```

## Support

For issues or questions, contact Hugh Cheng.

---

# 中文

通过 Jumphost 或直接连接管理到 OpenSync DUT 设备的 SSH 连接。

## 网络拓扑

```
你的电脑                    跳板机                     DUT 设备
┌─────────────┐             ┌─────────────┐            ┌─────────────┐
│             │             │             │            │             │
│   开发者    │────SSH─────▶│  172.16.   │────SSH────▶│  192.168.  │
│   机器      │             │   10.81    │            │   40.1     │
│             │             │             │            │             │
│   (本地)    │             │  (跳板机)  │            │   (DUT)    │
└─────────────┘             └─────────────┘            └─────────────┘
      │                                                       │
      │                                                       │
      └───────────────────────直接 SSH (可选)─────────────────┘
                           (当 DUT 可达时)
```

### 连接模式

**模式 1: 通过跳板机（推荐）**
- 你的电脑 → 跳板机 (172.16.10.81) → DUT (192.168.40.1)
- 最安全可靠
- 即使 DUT 不可直接访问也能工作
- 默认配置

**模式 2: 直接连接（可选）**
- 你的电脑 → DUT (192.168.40.1)
- 需要 DUT 在同一网络
- 更快但灵活性较低
- 在配置中设置 `USE_JUMPHOST=no`

### 网络详情

| 组件 | IP 地址 | 端口 | 用户 | 说明 |
|------|---------|------|------|------|
| **跳板机** | 172.16.10.81 | 22 | plume/actiontec | SSH 中继服务器 |
| **DUT** | 192.168.40.1 | 22 | root/osync | 待测试的 OpenSync 设备 |
| **备用 DUT** | 192.168.50.1 | 22 | root | 不同板子配置 |

## 快速开始

### 1. 配置连接设置

编辑 `ssh_dut.conf` 设置连接参数：

```bash
# 连接模式
CONNECTION_MODE="jumphost"  # 或 "direct"

# 跳板机配置
JUMPHOST_HOST="172.16.10.81"
JUMPHOST_USER="plume"
JUMPHOST_PASS="plume"
JUMPHOST_AUTH_METHOD="password"  # 或 "key"

# DUT 配置
DUT_HOST="192.168.40.1"
DUT_USER="root"
DUT_PASS="OpenSync@#!2022"

# 网络配置
DUT_INTERFACE="eth1"
JUMPHOST_INTERFACE="enp5s0"
```

### 2. 测试连接

```bash
# 测试跳板机连接
./ssh2jumphost.sh "hostname"

# 测试通过跳板机连接 DUT
./ssh2jumphost2dut.sh "hostname"

# 检查 DUT 网络接口
./ssh2jumphost2dut.sh "ifconfig"
```

### 3. 常用操作

```bash
# 系统信息
./ssh2jumphost2dut.sh "uname -a"
./ssh2jumphost2dut.sh "uptime"
./ssh2jumphost2dut.sh "free -m"
./ssh2jumphost2dut.sh "df -h"

# OpenSync 命令
./ssh2jumphost2dut.sh "ovsh s Wifi_Radio_State"
./ssh2jumphost2dut.sh "ovsh s Wifi_VIF_State"
./ssh2jumphost2dut.sh "ovsh s Connection_Manager_Uplink"

# 日志
./ssh2jumphost2dut.sh "logread | grep -i opensync | tail -20"
./ssh2jumphost2dut.sh "tail -50 /var/log/messages"

# 网络诊断
./ssh2jumphost2dut.sh "ping -c 3 8.8.8.8"
./ssh2jumphost2dut.sh "iwconfig"
```

## 文件说明

- **README.md** - 本文件
- **SKILL.md** - GitHub Copilot 完整文档
- **ssh2jumphost.sh** - 连接跳板机并执行命令
- **ssh2jumphost2dut.sh** - 通过跳板机或直接连接 DUT
- **ssh_dut.conf** - 连接配置文件

## 在 GitHub Copilot 中使用

在 Copilot Chat 中，可以使用自然语言：

```
"连接到 DUT 并检查系统状态"
"查看 DUT 的 OpenSync 日志"
"在 DUT 上执行 ovsh s Wifi_Radio_Config"
"检查 DUT 网络连接"
```

## 故障排除

### 无法连接到跳板机

```bash
# 测试连通性
ping -c 3 172.16.10.81

# 检查 ssh_dut.conf 中的凭证
cat ssh_dut.conf | grep JUMPHOST

# 测试手动 SSH
ssh plume@172.16.10.81
```

### 从跳板机无法连接到 DUT

```bash
# 从跳板机测试 DUT 连通性
./ssh2jumphost.sh "ping -c 3 192.168.40.1"

# 检查跳板机上的 DUT 接口
./ssh2jumphost.sh "ifconfig enp5s0"

# 尝试直接连接（如果在同一网络）
ssh root@192.168.40.1
```

### 网络配置问题

```bash
# 配置跳板机网络接口
./ssh2jumphost.sh "sudo ifconfig enp5s0 192.168.40.100 netmask 255.255.255.0 up"

# 检查路由
./ssh2jumphost.sh "route -n"
```

## 安全警告

⚠️ 配置文件包含敏感凭证，请妥善保管：
- 限制文件权限：`chmod 600 ssh_dut.conf`
- 不要提交到公共仓库
- 使用 SSH 密钥代替密码（推荐）：

```bash
# 生成 SSH 密钥
ssh-keygen -t rsa -b 4096 -C "your_email@example.com"

# 复制到跳板机
ssh-copy-id plume@172.16.10.81

# 更新配置
# 设置 JUMPHOST_AUTH_METHOD="key"
```

## 技术支持

如有问题或疑问，请联系 Hugh Cheng。
