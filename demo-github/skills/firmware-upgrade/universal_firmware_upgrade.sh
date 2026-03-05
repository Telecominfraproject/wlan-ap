#!/bin/bash
# ============================================
# 通用固件升级脚本 / Universal Firmware Upgrade Script
# ============================================
# 功能 / Function: 
#   支持多项目的自动固件下载和升级
#   Support multi-project automatic firmware download and upgrade
# 
# 用法 / Usage:
#   ./universal_firmware_upgrade.sh <project> [version]
#   如果不指定 version，则从源服务器读取 version 文件
#   If version is not specified, read from version file on source server
# ============================================

set -e  # 遇到错误立即退出 / Exit on error

# ============================================
# 路径配置 / Path Configuration
# ============================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${SCRIPT_DIR}/firmware_upgrade.conf"
SSH_CONFIG_FILE="${SCRIPT_DIR}/../ssh-connection/ssh_dut.conf"
JUMPHOST_SCRIPT="${SCRIPT_DIR}/../ssh-connection/ssh2jumphost.sh"
DUT_SCRIPT="${SCRIPT_DIR}/../ssh-connection/ssh2jumphost2dut.sh"

# ============================================
# 颜色定义 / Color Definitions
# ============================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ============================================
# 辅助函数 / Helper Functions
# ============================================

print_info() { echo -e "${BLUE}>>> $1${NC}"; }
print_success() { echo -e "${GREEN}✓ $1${NC}"; }
print_error() { echo -e "${RED}✗ $1${NC}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${NC}"; }
print_separator() { echo "========================================"; }

# 显示帮助信息 / Show help
show_help() {
    cat << HELPEOF
========================================
通用固件升级脚本 / Universal Firmware Upgrade Script
========================================

用法 / Usage:
    $0 <project> [version]

参数 / Parameters:
    project         项目名称 / Project name
                   支持 / Supported: WF710G, WF810, WF710
    
    version        (可选) 版本号 / (Optional) Version number
                   如不指定，自动从源服务器读取 version 文件
                   If not specified, auto-read from version file on source server
                   
                   支持两种格式 / Supports two formats:
                   1. 不带时间戳 / Without timestamp: 12.2.6.25-WF710X
                   2. 带时间戳 / With timestamp: 12.2.6.25-WF710X-T2601406

示例 / Examples:
    # 使用 version 文件中的版本 (推荐)
    # Use version from version file (recommended)
    $0 WF710G
    
    # 指定版本号 (不带时间戳)
    # Specify version (without timestamp)
    $0 WF710G 12.2.6.25-WF710X
    
    # 指定版本号 (带时间戳)
    # Specify version (with timestamp)
    $0 WF710G 12.2.6.25-WF710X-T2601406

版本转换规则 / Version Conversion Rules:
    12.2.6.25-WF710X           -> nand-ipq5332-single-12.2.6.25-WF710X-dev.img
    12.2.6.25-WF710X-T2601406  -> nand-ipq5332-single-12.2.6.25-WF710X-dev-T2601406.img

流程 / Process:
    1. 读取版本号 (从 version 文件或参数)
       Read version (from version file or parameter)
    2. 生成固件文件名
       Generate firmware filename
    3. 从源服务器下载固件到 Jumphost
       Download firmware from source server to Jumphost
    4. 从 Jumphost 传输固件到 DUT
       Transfer firmware from Jumphost to DUT
    5. 在 DUT 上执行升级并重启
       Execute upgrade on DUT and reboot

配置文件 / Configuration Files:
    固件配置 / Firmware config: ${CONFIG_FILE}
    SSH 配置 / SSH config: ${SSH_CONFIG_FILE}

========================================
HELPEOF
}

# 加载配置文件 / Load configuration
load_config() {
    if [[ ! -f "${CONFIG_FILE}" ]]; then
        print_error "配置文件不存在 / Configuration file not found: ${CONFIG_FILE}"
        exit 1
    fi
    
    if [[ ! -f "${SSH_CONFIG_FILE}" ]]; then
        print_error "SSH 配置文件不存在 / SSH config file not found: ${SSH_CONFIG_FILE}"
        exit 1
    fi
    
    # 保存脚本路径（防止被配置文件覆盖）
    # Save script paths (prevent override by config files)
    local saved_jumphost_script="${JUMPHOST_SCRIPT}"
    local saved_dut_script="${DUT_SCRIPT}"
    
    # 读取配置 / Load configuration
    source "${CONFIG_FILE}"
    source "${SSH_CONFIG_FILE}"
    
    # 恢复脚本路径 / Restore script paths
    JUMPHOST_SCRIPT="${saved_jumphost_script}"
    DUT_SCRIPT="${saved_dut_script}"
    
    print_success "配置文件加载完成 / Configuration loaded"
}

# 加载项目配置 / Load project configuration
load_project_config() {
    local project="$1"
    
    # 计算 WORKSPACE 默认值（脚本目录的上上上级）
    # Calculate WORKSPACE default (3 levels up from script directory)
    local workspace_default="${WORKSPACE_DEFAULT:-$(dirname $(dirname $(dirname $SCRIPT_DIR)))}"
    
    # 尝试读取项目特定的 WORKSPACE，如果为空则使用默认值
    # Try to read project-specific WORKSPACE, use default if empty
    local project_workspace=$(eval echo \$${project}_WORKSPACE)
    WORKSPACE="${project_workspace:-$workspace_default}"
    
    # 读取项目特定配置 / Read project-specific configuration
    VERSION_FILE=$(eval echo \$${project}_VERSION_FILE)
    FIRMWARE_PREFIX=$(eval echo \$${project}_FIRMWARE_PREFIX)
    FIRMWARE_SUFFIX=$(eval echo \$${project}_FIRMWARE_SUFFIX)
    SRC_HOST=$(eval echo \$${project}_SRC_HOST)
    SRC_USER=$(eval echo \$${project}_SRC_USER)
    SRC_PASSWORD=$(eval echo \$${project}_SRC_PASSWORD)
    SRC_BASE_PATH=$(eval echo \$${project}_SRC_BASE_PATH)
    UPGRADE_CMD=$(eval echo \$${project}_UPGRADE_CMD)   
    PLATFORM=$(eval echo \$${project}_PLATFORM)   
    # 验证配置 / Validate configuration
    if [[ -z "${VERSION_FILE}" || -z "${FIRMWARE_PREFIX}" || -z "${SRC_HOST}" ]]; then
        print_error "项目 ${project} 配置不完整 / Project ${project} configuration incomplete"
        return 1
    fi
    
    print_success "项目 ${project} 配置加载成功 / Project ${project} configuration loaded"
    print_info "工作区 / Workspace: ${WORKSPACE}"
    return 0
}

# 从源服务器读取版本文件 / Read version file from source server
read_version_from_source() {
    local version_file_path="${WORKSPACE}/${SRC_BASE_PATH}/${VERSION_FILE}"
    
    print_info "从源服务器读取版本文件 / Reading version file from source server"
    echo "    路径 / Path: ${SRC_USER}@${SRC_HOST}:${version_file_path}"
    
    # 使用 sshpass 读取 version 文件
    # Use sshpass to read version file
    local version=$(sshpass -p "${SRC_PASSWORD}" ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${SRC_USER}@${SRC_HOST} "cat ${version_file_path}" 2>/dev/null | tr -d '\n\r ')
    
    if [[ -z "${version}" ]]; then
        print_error "无法读取版本文件 / Cannot read version file"
        return 1
    fi
    
    echo "${version}"
    return 0
}

# 版本转换为固件文件名 / Convert version to firmware filename
# 规则 / Rules:
#   12.2.6.25-WF710X           -> 12.2.6.25-WF710X-dev
#   12.2.6.25-WF710X-T2601406  -> 12.2.6.25-WF710X-dev-T2601406
convert_version_to_firmware() {
    local version="$1"
    local firmware_name=""
    
    # 检查版本是否包含 -T 时间戳
    # Check if version contains -T timestamp
    if [[ "${version}" =~ -T[0-9]+ ]]; then
        # 带时间戳：在 -T 前插入 -dev
        # With timestamp: insert -dev before -T
        firmware_name=$(echo "${version}" | sed 's/-T/-dev-T/')
    else
        # 不带时间戳：在末尾添加 -dev
        # Without timestamp: append -dev
        firmware_name="${version}-dev"
    fi
    
    # 添加前缀和后缀
    # Add prefix and suffix
    if [ "$PLATFORM" = "opensync" ] ; then
       firmware_name="${FIRMWARE_PREFIX}${firmware_name}${FIRMWARE_SUFFIX}"
    else
       firmware_name="${FIRMWARE_PREFIX}${version}${FIRMWARE_SUFFIX}"
    fi
    echo "${firmware_name}"
}

# 下载固件到 Jumphost / Download firmware to Jumphost
download_firmware() {
    local firmware_file="$1"
    local src_path="${WORKSPACE}/${SRC_BASE_PATH}/${firmware_file}"
    
    print_separator
    print_info "[步骤 1/3] 下载固件到 Jumphost / [Step 1/3] Download firmware to Jumphost"
    echo "    源地址 / Source: ${SRC_USER}@${SRC_HOST}:${src_path}"
    echo "    目标地址 / Destination: ${JUMPHOST_SSH_USER}@${JUMPHOST_SSH_HOST}:~/"
    print_separator
    
    # 使用 sshpass 从源服务器下载固件到 Jumphost
    local download_cmd="sshpass -p '''${SRC_PASSWORD}''' scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${SRC_USER}@${SRC_HOST}:${src_path} ./"
    
    if "${JUMPHOST_SCRIPT}" "${project}" "${download_cmd}"; then
        print_success "固件下载成功 / Firmware downloaded successfully"
        return 0
    else
        print_error "固件下载失败 / Firmware download failed"
        return 1
    fi
}

# 传输固件到 DUT / Transfer firmware to DUT
transfer_firmware() {
    local firmware_file="$1"
    DUT_SSH_USER_VAR="${project}_DUT_SSH_USER"
    DUT_SSH_HOST_VAR="${project}_DUT_SSH_HOST"
    DUT_SSH_PASSWORD_VAR="${project}_DUT_SSH_PASSWORD"
    DUT_SSH_USER="${!DUT_SSH_USER_VAR}"
    DUT_SSH_HOST="${!DUT_SSH_HOST_VAR}"    
    DUT_SSH_PASSWORD="${!DUT_SSH_PASSWORD_VAR}"
    print_separator
    print_info "[步骤 2/3] 传输固件到 DUT / [Step 2/3] Transfer firmware to DUT"
    echo "    源地址 / Source: ${JUMPHOST_SSH_USER}@${JUMPHOST_SSH_HOST}:~/${firmware_file}"
    echo "    目标地址 / Destination: ${DUT_SSH_USER}@${DUT_SSH_HOST}:/var/"
    print_separator
    
    # 使用 sshpass 从 Jumphost 传输固件到 DUT
    if [ -n "$DUT_SSH_PASSWORD" ] ; then
    	local transfer_cmd="sshpass -p '''${DUT_SSH_PASSWORD}''' scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${firmware_file} ${DUT_SSH_USER}@${DUT_SSH_HOST}:/var/"
    else
    	local transfer_cmd="scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ${firmware_file} ${DUT_SSH_USER}@${DUT_SSH_HOST}:/var/"

    fi
    if "${JUMPHOST_SCRIPT}" "${project}" "${transfer_cmd}"; then
        print_success "固件传输成功 / Firmware transferred successfully"
        return 0
    else
        print_error "固件传输失败 / Firmware transfer failed"
        return 1
    fi
}

upgrade_firmware() {
    local project="$1"
    local firmware_name="$2"
    
    print_info "升级固件并重启 / Upgrade firmware and reboot"
    echo "    固件文件 / Firmware: /var/${firmware_name}"
    echo "    项目 / Project: ${project}"
    
    # 动态获取项目的升级命令
    UPGRADE_CMD_VAR="${project}_UPGRADE_CMD"
    UPGRADE_CMD="${!UPGRADE_CMD_VAR}"
    
    if [[ -z "$UPGRADE_CMD" ]]; then
        print_error "未找到项目 ${project} 的升级命令配置 / Upgrade command not found for project ${project}"
        return 1
    fi
    
    # 替换命令模板中的 {firmware_file} 占位符为实际文件名
    local upgrade_cmd="${UPGRADE_CMD//\{firmware_file\}/${firmware_name}}"
    
    print_info "执行升级命令 / Executing upgrade command: ${upgrade_cmd}"
    print_warning "DUT 将在升级后自动重启 / DUT will reboot automatically after upgrade"
    
    # 调用 DUT 脚本执行升级（传递 project 和 command）
    if "${DUT_SCRIPT}" "${project}" "${upgrade_cmd}"; then
        print_success "升级命令执行成功 / Upgrade command executed successfully"
        print_info "DUT 正在重启，请稍候... / DUT is rebooting, please wait..."
        return 0
    else
	if [ "$PLATFORM" = "opensync" ] ; then
        	print_error "升级命令执行失败 / Upgrade command execution failed"
        	return 1
	else
        	print_success "升级命令完成 需要reboot 后检查 / Upgrade command executed successfully"
	 	return 0
	fi
    fi
}


# 显示升级摘要 / Show upgrade summary

show_summary() {
    local project="$1"
    local version="$2"
    local firmware_name="$3"
    local status="$4"
    
    # 动态获取项目的配置
    DUT_SSH_HOST_VAR="${project}_DUT_SSH_HOST"
    DUT_SSH_USER_VAR="${project}_DUT_SSH_USER"
    JUMPHOST_SSH_HOST_VAR="${project}_JUMPHOST_SSH_HOST"
    UPGRADE_CMD_VAR="${project}_UPGRADE_CMD"
    
    DUT_SSH_HOST="${!DUT_SSH_HOST_VAR}"
    DUT_SSH_USER="${!DUT_SSH_USER_VAR}"
    JUMPHOST_SSH_HOST="${!JUMPHOST_SSH_HOST_VAR}"
    UPGRADE_CMD="${!UPGRADE_CMD_VAR}"
    
    print_separator
    echo "升级汇总 / Upgrade Summary"
    print_separator
    echo "项目 / Project:        ${project}"
    echo "版本 / Version:        ${version}"
    echo "固件 / Firmware:       ${firmware_name}"
    echo "DUT 主机 / DUT Host:   ${DUT_SSH_USER}@${DUT_SSH_HOST}"
    echo "Jumphost:              ${JUMPHOST_SSH_HOST}"
    echo "升级命令 / Upgrade:    ${UPGRADE_CMD}"
    echo "状态 / Status:         ${status}"
    print_separator
}

# ============================================
# 主程序 / Main Program
# ============================================

main() {
    # 检查参数 / Check parameters
    if [[ $# -lt 1 ]]; then
        show_help
        exit 1
    fi
    
    # 处理帮助参数 / Handle help parameter
    if [[ "$1" == "help" || "$1" == "-h" || "$1" == "--help" ]]; then
        show_help
        exit 0
    fi
    
    local project="$1"
    local version="$2"
    
    # 显示开始信息 / Show start info
    print_separator
    echo "通用固件升级脚本 / Universal Firmware Upgrade Script"
    echo "项目 / Project: ${project}"
    print_separator
    
    # 加载配置 / Load configuration
    print_info "加载配置文件 / Loading configuration..."
    load_config
    
    # 加载项目配置 / Load project configuration
    print_info "加载项目配置 / Loading project configuration..."
    if ! load_project_config "${project}"; then
        print_error "加载项目配置失败 / Failed to load project configuration"
        exit 1
    fi
    
    # 获取版本号 / Get version
    if [[ -z "${version}" ]]; then
        print_info "未指定版本，从源服务器读取 / Version not specified, reading from source server"
        version=$(read_version_from_source)
        if [[ $? -ne 0 ]]; then
            print_error "读取版本失败 / Failed to read version"
            exit 1
        fi
        print_success "版本号: ${version}"
    else
        print_info "使用指定版本 / Using specified version: ${version}"
    fi
    
    # 生成固件文件名 / Generate firmware filename
    print_info "生成固件文件名 / Generating firmware filename..."
    firmware_file=$(convert_version_to_firmware "${version}")
    print_success "固件文件名: ${firmware_file}"
    
    # 验证脚本存在 / Validate scripts exist
    if [[ ! -x "${JUMPHOST_SCRIPT}" ]]; then
        print_error "Jumphost 脚本不存在或不可执行 / Jumphost script not found or not executable: ${JUMPHOST_SCRIPT}"
        exit 1
    fi
    
    if [[ ! -x "${DUT_SCRIPT}" ]]; then
        print_error "DUT 脚本不存在或不可执行 / DUT script not found or not executable: ${DUT_SCRIPT}"
        exit 1
    fi
    
    # 执行升级流程 / Execute upgrade process
    print_info "开始升级流程 / Starting upgrade process..."
    
    # 步骤 1: 下载固件 / Step 1: Download firmware
    if ! download_firmware "${firmware_file}"; then
        print_error "升级失败：固件下载失败 / Upgrade failed: Firmware download failed"
        exit 1
    fi
    
    # 步骤 2: 传输固件 / Step 2: Transfer firmware
    if ! transfer_firmware "${firmware_file}"; then
        print_error "升级失败：固件传输失败 / Upgrade failed: Firmware transfer failed"
        exit 1
    fi
    
    # 步骤 3: 升级固件 / Step 3: Upgrade firmware
    if ! upgrade_firmware "${project}" "${firmware_file}"; then
        print_error "升级失败：升级命令执行失败 / Upgrade failed: Upgrade command execution failed"
        exit 1
    fi
    
    # 显示摘要 / Show summary
    show_summary "${project}" "${version}" "${firmware_file}"
    
    print_success "升级流程完成 / Upgrade process completed"
    print_info "请等待 DUT 重启完成 / Please wait for DUT to complete reboot"
    
    exit 0
}

# 执行主程序 / Execute main program
main "$@"
