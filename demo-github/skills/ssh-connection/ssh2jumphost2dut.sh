#!/bin/bash

# skill Shell Script
# Description: 连接到 DUT 并执行命令，支持多项目配置

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG_FILE="${SCRIPT_DIR}/ssh_dut.conf"

# 参数解析
if [ $# -lt 2 ]; then
    echo "Usage: $0 <project> <command>"
    exit 1
fi
PROJECT="$1"
shift
CMD="$*"

# 加载配置文件
if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
else
    echo "警告 / Warning: 配置文件不存在 / Config file not found: $CONFIG_FILE"
    exit 1
fi

# 动态加载项目相关变量
DUT_SSH_USER_VAR="${PROJECT}_DUT_SSH_USER"
DUT_SSH_PASSWORD_VAR="${PROJECT}_DUT_SSH_PASSWORD"
DUT_SSH_HOST_VAR="${PROJECT}_DUT_SSH_HOST"
JUMPHOST_SSH_USER_VAR="${PROJECT}_JUMPHOST_SSH_USER"
JUMPHOST_SSH_PASSWORD_VAR="${PROJECT}_JUMPHOST_SSH_PASSWORD"
JUMPHOST_SSH_HOST_VAR="${PROJECT}_JUMPHOST_SSH_HOST"
JUMPHOST_DUT_INTERFACE_VAR="${PROJECT}_JUMPHOST_DUT_INTERFACE"
JUMPHOST_DUT_IP_VAR="${PROJECT}_JUMPHOST_DUT_IP"
JUMPHOST_SUDO_PASSWORD_VAR="${PROJECT}_JUMPHOST_SUDO_PASSWORD"

DUT_SSH_USER="${!DUT_SSH_USER_VAR}"
DUT_SSH_PASSWORD="${!DUT_SSH_PASSWORD_VAR}"
DUT_SSH_HOST="${!DUT_SSH_HOST_VAR}"
JUMPHOST_SSH_USER="${!JUMPHOST_SSH_USER_VAR}"
JUMPHOST_SSH_PASSWORD="${!JUMPHOST_SSH_PASSWORD_VAR}"
JUMPHOST_SSH_HOST="${!JUMPHOST_SSH_HOST_VAR}"
JUMPHOST_DUT_INTERFACE="${!JUMPHOST_DUT_INTERFACE_VAR}"
JUMPHOST_DUT_IP="${!JUMPHOST_DUT_IP_VAR}"
JUMPHOST_SUDO_PASSWORD="${!JUMPHOST_SUDO_PASSWORD_VAR}"

# 检查变量
if [[ -z "$DUT_SSH_USER" || -z "$DUT_SSH_HOST" || -z "$JUMPHOST_SSH_USER" || -z "$JUMPHOST_SSH_HOST" ]]; then
    echo "项目配置不完整，请检查 $CONFIG_FILE 中 $PROJECT 相关变量"
    exit 1
fi

echo "========================================"
echo "DUT 连接脚本 / DUT Connection Script"
echo "========================================"
echo ">>> 使用 Jumphost 模式连接 DUT / Using Jumphost mode to connect DUT"
echo "    Jumphost: ${JUMPHOST_SSH_USER}@${JUMPHOST_SSH_HOST}"
echo "    DUT: ${DUT_SSH_USER}@${DUT_SSH_HOST}"
echo ""

# 步骤1: 配置 Jumphost 上连接 DUT 的网络接口
JUMPHOST_SCRIPT_PATH="${SCRIPT_DIR}/ssh2jumphost.sh"
if [ ! -f "$JUMPHOST_SCRIPT_PATH" ]; then
    echo "错误 / Error: Jumphost 脚本不存在 / Jumphost script not found: $JUMPHOST_SCRIPT_PATH"
    exit 1
fi

echo ">>> [步骤1/2] 配置 Jumphost 网络接口 / [Step 1/2] Configure Jumphost network interface"
echo "    接口 / Interface: ${JUMPHOST_DUT_INTERFACE}"
echo "    IP 地址 / IP Address: ${JUMPHOST_DUT_IP}"
"${JUMPHOST_SCRIPT_PATH}" "echo \"${JUMPHOST_SUDO_PASSWORD}\" | sudo -S ifconfig ${JUMPHOST_DUT_INTERFACE} ${JUMPHOST_DUT_IP}" > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "    警告 / Warning: 配置网络接口失败，继续尝试连接 / Failed to configure interface, continue..."
else
    echo "    成功 / Success: 网络接口配置完成 / Network interface configured"
fi
echo ""

# 步骤2: 通过 Jumphost SSH 到 DUT 执行命令
echo ">>> [步骤2/2] 通过 Jumphost 连接到 DUT / [Step 2/2] Connect to DUT through Jumphost"
if [ -z "$DUT_SSH_PASSWORD" ]; then
    echo "    认证方式 / Authentication: SSH Key"
    DUT_SSH_CMD="ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o HostKeyAlgorithms=ssh-dss,ssh-rsa,ssh-ed25519 -o KexAlgorithms=+diffie-hellman-group1-sha1 -o ControlMaster=no -o LogLevel=quiet -A ${DUT_SSH_USER}@${DUT_SSH_HOST} \"$CMD\""
else
    echo "    认证方式 / Authentication: Password"
    if ! command -v sshpass >/dev/null 2>&1; then
        echo "错误 / Error: DUT 需要密码但 sshpass 未安装 / DUT requires password but sshpass not installed"
        exit 1
    fi
    DUT_SSH_CMD="sshpass -p ${DUT_SSH_PASSWORD} ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o HostKeyAlgorithms=ssh-dss,ssh-rsa,ssh-ed25519 -o KexAlgorithms=+diffie-hellman-group1-sha1 -o ControlMaster=no -o LogLevel=quiet -A ${DUT_SSH_USER}@${DUT_SSH_HOST} \"$CMD\""
fi

echo "    执行命令 / Executing command: $CMD"
echo "========================================"
echo ""

sshpass -p "${JUMPHOST_SSH_PASSWORD}" ssh \
    -o UserKnownHostsFile=/dev/null \
    -o StrictHostKeyChecking=no \
    -o HostKeyAlgorithms=ssh-dss,ssh-rsa,ssh-ed25519 \
    -o KexAlgorithms=+diffie-hellman-group1-sha1 \
    -o ControlMaster=no \
    -o LogLevel=quiet \
    -A \
    "${JUMPHOST_SSH_USER}@${JUMPHOST_SSH_HOST}" \
    "$DUT_SSH_CMD"

exit $?
