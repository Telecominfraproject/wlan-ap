#!/bin/bash
# ============================================================================
# Universal Build and Upgrade Skill / 通用构建和升级 Skill
# ============================================================================
# 功能 / Function:
#   1. 更新版本号 / Update version number
#   2. 构建固件 / Build firmware (调用 universal-build skill)
#   3. 查找镜像文件 / Locate image file
#   4. 升级固件 / Upgrade firmware (调用 firmware-upgrade skill)
#   5. 等待设备重启 / Wait for device reboot
#   6. 验证版本 / Verify version
#
# 用法 / Usage:
#   ./universal_build_and_upgrade.sh [PROJECT] [VERSION]
#   ./universal_build_and_upgrade.sh help
#
# 示例 / Example:
#   ./universal_build_and_upgrade.sh WF710G 12.2.6.25-WF710X-T26011002
#   ./universal_build_and_upgrade.sh WF710G
# ============================================================================

set -e

# ========== 脚本路径和相对路径 / Script Paths and Relative Paths ==========
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${SCRIPT_DIR}/build_upgrade.conf"

# 相对路径引用其他 skills / Relative paths to other skills
BUILD_SKILL_DIR="${SCRIPT_DIR}/../universal-build"
UPGRADE_SKILL_DIR="${SCRIPT_DIR}/../firmware-upgrade"
SSH_SKILL_DIR="${SCRIPT_DIR}/../ssh-connection"
UPGRADE_CONFIG="${UPGRADE_SKILL_DIR}/firmware_upgrade.conf"
BUILD_SCRIPT="${BUILD_SKILL_DIR}/universal_build_skill.sh"
UPGRADE_SCRIPT="${UPGRADE_SKILL_DIR}/universal_firmware_upgrade.sh"
SSH_SCRIPT="${SSH_SKILL_DIR}/ssh2jumphost2dut.sh"
SSH_CONFIG="${SSH_SKILL_DIR}/ssh_dut.conf"

# ========== 颜色定义 / Color Definitions ==========
COLOR_RESET="\033[0m"
COLOR_RED="\033[31m"
COLOR_GREEN="\033[32m"
COLOR_YELLOW="\033[33m"
COLOR_BLUE="\033[34m"
COLOR_CYAN="\033[36m"
COLOR_MAGENTA="\033[35m"

# ========== 加载配置 / Load Configuration ==========
load_config() {
    if [[ ! -f "$CONFIG_FILE" ]]; then
        echo -e "${COLOR_RED}✗ 错误 / Error: 配置文件不存在 / Config file not found${COLOR_RESET}"
        echo "  文件 / File: $CONFIG_FILE"
        exit 1
    fi
    
    # shellcheck source=/dev/null
    source "$CONFIG_FILE"
    source "$UPGRADE_CONFIG"
    
    # 加载 SSH 配置以获取 DUT 信息
    # Load SSH config to get DUT info
    if [[ -f "$SSH_CONFIG" ]]; then
        # 保护变量，防止被覆盖
        # Protect variables from being overwritten
        local saved_workspace_default="${WORKSPACE_DEFAULT}"
        
        # shellcheck source=/dev/null
        source "$SSH_CONFIG"
        
        # 恢复被覆盖的变量
        # Restore overwritten variables
        WORKSPACE_DEFAULT="${saved_workspace_default}"
    fi
}

# ========== 加载项目配置 / Load Project Configuration ==========
load_project_config() {
    local project=$1
    
    # 自动检测 WORKSPACE（脚本在 .github/skills/build-and-upgrade/）
    local workspace_default="${WORKSPACE_DEFAULT:-$(dirname $(dirname $(dirname $SCRIPT_DIR)))}"
    
    # 动态读取项目特定的 WORKSPACE
    local workspace_var="${project}_WORKSPACE"
    WORKSPACE=$(eval echo "\${${workspace_var}:-${workspace_default}}")
    
    # 读取构建相关配置（来自 build_config.env）
    local model_name_var="${project}_MODEL_NAME"
    local container_dir_var="${project}_CONTAINER_DIR"
    local make_cmd_var="${project}_MAKE_CMD"
    local docker_image_var="${project}_DOCKER_IMAGE"
    local dut_version_var="${project}_DUT_VERSION_FILE"
    
    MODEL_NAME=$(eval echo "\${${model_name_var}}")
    CONTAINER_DIR=$(eval echo "\${${container_dir_var}}")
    MAKE_CMD=$(eval echo "\${${make_cmd_var}}")
    DOCKER_IMAGE=$(eval echo "\${${docker_image_var}}")
    DUT_VERSION_FILE=$(eval echo "\${${dut_version_var}}")
    
    # 读取固件升级配置（来自 firmware_upgrade.conf）
    local version_file_var="${project}_VERSION_FILE"
    local firmware_prefix_var="${project}_FIRMWARE_PREFIX"
    local firmware_suffix_var="${project}_FIRMWARE_SUFFIX"
    local upgrade_cmd_var="${project}_UPGRADE_CMD"
    local platform_var="${project}_PLATFORM"
    local src_base_path_var="${project}_SRC_BASE_PATH"
    echo "###version_file_var=$version_file_var"
    PLATFORM=$(eval echo "\${${platform_var}}")
    VERSION_FILE=$(eval echo "\${${version_file_var}}")
    echo "###version_file=$VERSION_FILE"
    FIRMWARE_PREFIX=$(eval echo "\${${firmware_prefix_var}}")
    FIRMWARE_SUFFIX=$(eval echo "\${${firmware_suffix_var}}")
    UPGRADE_CMD=$(eval echo "\${${upgrade_cmd_var}}")
    #PLATFORM=$(eval echo "\${${platform_var}}")
    TT_CMD=$(eval echo "\${${platform_var}}")
    SRC_BASE_PATH=$(eval echo "\${${src_base_path_var}}")
    # 读取 DUT/Jumphost 配置（来自 ssh_dut.conf）
    local dut_ssh_user_var="${project}_DUT_SSH_USER"
    local dut_ssh_host_var="${project}_DUT_SSH_HOST"
    local jumphost_ssh_user_var="${project}_JUMPHOST_SSH_USER"
    local jumphost_ssh_host_var="${project}_JUMPHOST_SSH_HOST"
    
    DUT_SSH_USER=$(eval echo "\${${dut_ssh_user_var}}")
    DUT_SSH_HOST=$(eval echo "\${${dut_ssh_host_var}}")
    JUMPHOST_SSH_USER=$(eval echo "\${${jumphost_ssh_user_var}}")
    JUMPHOST_SSH_HOST=$(eval echo "\${${jumphost_ssh_host_var}}")
    
    # 验证必需配置
    if [[ -z "$MODEL_NAME" ]] || [[ -z "$VERSION_FILE" ]] || [[ -z "$DUT_SSH_HOST" ]]; then
        echo -e "${COLOR_RED}✗ 错误 / Error: 项目 $project 配置不完整 / Project $project configuration incomplete${COLOR_RESET}"
        echo "  缺少 / Missing: MODEL_NAME, VERSION_FILE, or DUT_SSH_HOST"
        exit 1
    fi
    
    echo -e "${COLOR_GREEN}✓ 项目 $project 配置加载成功 / Project $project configuration loaded${COLOR_RESET}"
    echo -e "${COLOR_BLUE}>>> 工作区 / Workspace: $WORKSPACE${COLOR_RESET}"
    echo -e "${COLOR_BLUE}>>> DUT: ${DUT_SSH_USER}@${DUT_SSH_HOST}${COLOR_RESET}"
    echo -e "${COLOR_BLUE}>>> Jumphost: ${JUMPHOST_SSH_USER}@${JUMPHOST_SSH_HOST}${COLOR_RESET}"
}

# ========== 帮助信息 / Help Information ==========
show_help() {
    cat << 'HELPEOF'
========================================
通用构建和升级脚本 / Universal Build and Upgrade Script
========================================

功能 / Function:
  完整的固件开发流程：版本更新 → 构建 → 升级 → 验证
  Complete firmware development workflow: Version Update → Build → Upgrade → Verify

用法 / Usage:
  ./universal_build_and_upgrade.sh [PROJECT] [VERSION]
  ./universal_build_and_upgrade.sh help

参数 / Parameters:
  PROJECT      项目名称 / Project name: WF710G, WF810, WF710
               默认: WF710G / Default: WF710G
  
  VERSION      版本号 / Version number
               默认从配置文件读取 / Default from config file
               格式 / Format: 12.2.6.25-WF710X-T26011002

命令 / Commands:
  help         显示此帮助信息 / Show this help message

示例 / Examples:
  # 使用默认项目和版本
  # Use default project and version
  ./universal_build_and_upgrade.sh

  # 指定项目
  # Specify project
  ./universal_build_and_upgrade.sh WF710G

  # 指定项目和版本
  # Specify project and version
  ./universal_build_and_upgrade.sh WF710G 12.2.6.25-WF710X-T26011002

工作流程 / Workflow:
  [1/7] 更新版本号 / Update version number
  [2/7] 构建固件 / Build firmware (30-60 分钟 / minutes)
  [3/7] 查找镜像文件 / Locate image file
  [4/7] 升级固件 / Upgrade firmware
  [5/7] 等待设备断开 / Wait for device disconnect
  [6/7] 等待 SSH 就绪 / Wait for SSH ready
  [7/7] 验证版本 / Verify version

依赖 Skills / Dependencies:
  - universal-build     : 固件构建 / Firmware build
  - firmware-upgrade   : 固件升级 / Firmware upgrade
  - ssh-connection     : SSH 连接 / SSH connection

配置文件 / Configuration:
  - build_upgrade.conf : 本 skill 配置 / This skill configuration
  - 自动复用其他 skills 的配置 / Auto-reuse other skills' configurations

WORKSPACE 机制 / WORKSPACE Mechanism:
  自动检测项目根目录（.github/skills 的上上上级）
  Auto-detect project root directory (3 levels up from .github/skills)
  
  可通过配置文件覆盖：
  Can be overridden in config file:
    WORKSPACE_DEFAULT="/your/project/path"
    {PROJECT}_WORKSPACE="/specific/path"

========================================
HELPEOF
}

# ========== 打印步骤标题 / Print Step Title ==========
print_step() {
    local step_num=$1
    local total_steps=$2
    local step_title_cn=$3
    local step_title_en=$4
    
    echo ""
    echo -e "${COLOR_CYAN}========================================${COLOR_RESET}"
    echo -e "${COLOR_CYAN}[步骤 $step_num/$total_steps] $step_title_cn${COLOR_RESET}"
    echo -e "${COLOR_CYAN}[Step $step_num/$total_steps] $step_title_en${COLOR_RESET}"
    echo -e "${COLOR_CYAN}========================================${COLOR_RESET}"
}

# ========== 打印成功信息 / Print Success Message ==========
print_success() {
    echo -e "${COLOR_GREEN}✓ $1${COLOR_RESET}"
}

# ========== 打印错误信息 / Print Error Message ==========
print_error() {
    echo -e "${COLOR_RED}✗ $1${COLOR_RESET}"
}

# ========== 打印警告信息 / Print Warning Message ==========
print_warning() {
    echo -e "${COLOR_YELLOW}⚠️  $1${COLOR_RESET}"
}

# ========== 打印信息 / Print Info ==========
print_info() {
    echo -e "${COLOR_BLUE}>>> $1${COLOR_RESET}"
}

# ========== 更新版本号 / Update Version Number ==========
update_version() {
    local version=$1
    local version_file_path="$WORKSPACE/$VERSION_FILE"
    echo "##version_file=$version_file_path"	
    print_step 1 7 "更新版本号" "Update Version Number"
    print_info "版本文件路径 / Version file path: $version_file_path"

    # 检查版本文件是否存在
    if [[ ! -f "$version_file_path" ]]; then
        print_error "版本文件不存在 / Version file not found: $version_file_path"
        exit 1
    fi

    # 备份原版本文件
    local backup_file="${version_file_path}.bak.$(date +%Y%m%d-%H%M%S)"
    cp "$version_file_path" "$backup_file"
    print_info "已备份原版本文件 / Original version file backed up"
    print_info "  备份文件 / Backup: $backup_file"

    if [[ "$PLATFORM" == "opensync" ]]; then
        # opensync: 直接覆盖
        printf "%s\n" "$version" > "$version_file_path"
    elif [[ "$PLATFORM" == "prplos" ]]; then
        # prplos: 替换 CONFIG_VERSION_CODE 行
        if grep -q 'CONFIG_VERSION_CODE=' "$version_file_path"; then
            sed -i "s/CONFIG_VERSION_CODE=.*/CONFIG_VERSION_CODE=\"$version\"/" "$version_file_path"
	    echo "###version file ($version_file_path)"
	    #cat $version_file_path
        else
            # 如果没有该行则追加
            echo "CONFIG_VERSION_CODE=\"$version\"" >> "$version_file_path"
        fi
    else
        print_error "未知平台 / Unknown PLATFORM: $PLATFORM"
        exit 1
    fi

    local current_version
    current_version=$(cat "$version_file_path")
    print_success "版本号已更新 / Version number updated: $current_version"
}

# ========== 构建固件 / Build Firmware ==========
build_firmware() {
    local project=$1
    local version=$2
    
    print_step 2 7 "构建固件（预计 30-60 分钟）" "Build Firmware (Estimated 30-60 minutes)"
    
    if [[ ! -f "$BUILD_SCRIPT" ]]; then
        print_error "构建脚本不存在 / Build script not found: $BUILD_SCRIPT"
        exit 1
    fi
    
    print_info "开始构建 / Starting build..."
    print_info "  项目 / Project: $project"
    print_info "  版本 / Version: $version"
    print_info "  脚本 / Script: $BUILD_SCRIPT"
    
    # 执行构建 / Execute build
    version1=""
    if [[ "$PLATFORM" == "opensync" ]]; then 
	    version1=$version
    fi  
    if bash "$BUILD_SCRIPT" build "$project" "$version1"; then
        print_success "固件构建完成 / Firmware build completed"
    else
        print_error "固件构建失败 / Firmware build failed"
        exit 1
    fi
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


# ========== 查找镜像文件 / Locate Image File ==========
locate_image() {
    local version=$1
    
    print_step 3 7 "查找生成的镜像文件" "Locate Generated Image File"
    
    local image_dir="$WORKSPACE/$SRC_BASE_PATH"
    
    # 转换版本号为固件文件名
    # Convert version to firmware filename
    IMAGE_NAME=$(convert_version_to_firmware "$version")
    IMAGE_PATH="$image_dir/$IMAGE_NAME"
    
    print_info "查找镜像文件 / Searching for image file..."
    print_info "  预期文件名 / Expected filename: $IMAGE_NAME"
    print_info "  预期路径 / Expected path: $IMAGE_PATH"
    
    if [[ -f "$IMAGE_PATH" ]]; then
        print_success "找到镜像文件 / Image file found"
        local file_size
        file_size=$(ls -lh "$IMAGE_PATH" | awk '{print $5}')
        print_info "  文件大小 / File size: $file_size"
        return 0
    else
        print_error "未找到镜像文件 / Image file not found"
        print_info "  预期路径 / Expected path: $IMAGE_PATH"
        print_warning "最近生成的镜像文件 / Recent image files:"
        ls -lht "$image_dir"/*.img 2>/dev/null | head -5 || echo "  无 .img 文件 / No .img files"
        exit 1
    fi
}

# ========== 升级固件 / Upgrade Firmware ==========
upgrade_firmware() {
    local project=$1
    local version=$2
    
    print_step 4 7 "升级固件到设备" "Upgrade Firmware to Device"
    
    if [[ ! -f "$UPGRADE_SCRIPT" ]]; then
        print_error "升级脚本不存在 / Upgrade script not found: $UPGRADE_SCRIPT"
        exit 1
    fi
    
    print_info "开始升级 / Starting upgrade..."
    print_info "  项目 / Project: $project"
    print_info "  版本 / Version: $version"
    print_info "  脚本 / Script: $UPGRADE_SCRIPT"
    
    # 执行升级 / Execute upgrade
    if bash "$UPGRADE_SCRIPT" "$project" "$version"; then
        print_success "固件升级完成 / Firmware upgrade completed"
    else
        print_error "固件升级失败 / Firmware upgrade failed"
        exit 1
    fi
}

# ========== 等待设备断开 / Wait for Device Disconnect ==========
wait_device_disconnect() {
    print_step 5 7 "等待设备断开连接（确认重启开始）" "Wait for Device Disconnect (Confirm Reboot Start)"
    
    local dut_ip="${DUT_SSH_HOST:-192.168.40.1}"
    local max_wait=${MAX_WAIT_DISCONNECT:-60}
    local elapsed=0
    
    print_info "检测设备连接状态 / Checking device connection status..."
    print_info "  目标 IP / Target IP: $dut_ip"
    print_info "  最大等待时间 / Max wait time: ${max_wait}秒 / seconds"
    
    # 等待一小段时间让 reboot 命令执行
    # Wait a bit for reboot command to execute
    sleep 5
    
    echo -n "等待断开 / Waiting for disconnect: "
    while [ $elapsed -lt $max_wait ]; do
        if ! ping -c 1 -W 1 "$dut_ip" &>/dev/null; then
            echo ""
            print_success "设备已断开，正在重启 / Device disconnected, rebooting (${elapsed}秒 / seconds)"
            return 0
        fi
        echo -n "."
        sleep 2
        elapsed=$((elapsed + 2))
    done
    
    echo ""
    print_warning "设备未断开，可能已经重启完成或网络异常 / Device not disconnected, may already rebooted or network issue"
}

# ========== 等待 SSH 服务就绪 / Wait for SSH Service Ready ==========
wait_ssh_ready() {
    print_step 6 7 "等待 SSH 服务就绪" "Wait for SSH Service Ready"
    
    local max_wait=${MAX_WAIT_SSH_READY:-300}
    local check_interval=${CHECK_INTERVAL:-5}
    local elapsed=0
    
    if [[ ! -f "$SSH_SCRIPT" ]]; then
        print_error "SSH 脚本不存在 / SSH script not found: $SSH_SCRIPT"
        exit 1
    fi
    
    print_info "等待 SSH 连接恢复 / Waiting for SSH connection recovery..."
    print_info "  最大等待时间 / Max wait time: ${max_wait}秒 / seconds"
    print_info "  检测间隔 / Check interval: ${check_interval}秒 / seconds"
    
    echo -n "检测中 / Checking: "
    echo "###($SSH_SCRIPT  $PROJECT uptime) "
    while [ $elapsed -lt $max_wait ]; do
        if bash "$SSH_SCRIPT" "$PROJECT" "uptime" &>/dev/null; then
            echo ""
            print_success "SSH 服务已就绪 / SSH service ready (等待时间 / Wait time: ${elapsed}秒 / seconds)"
            return 0
        fi
        echo -n "."
        sleep "$check_interval"
        elapsed=$((elapsed + check_interval))
    done
    
    echo ""
    print_error "超时：SSH 服务未在 ${max_wait} 秒内就绪 / Timeout: SSH service not ready within ${max_wait} seconds"
    print_warning "请手动检查设备状态 / Please manually check device status"
    exit 1
}


# ========== 验证版本 / Verify Version ==========
verify_version() {
    local expected_version=$1

    print_step 7 7 "验证固件版本" "Verify Firmware Version"

    if [[ ! -f "$SSH_SCRIPT" ]]; then
        print_error "SSH 脚本不存在 / SSH script not found: $SSH_SCRIPT"
        exit 1
    fi

    print_info "读取设备版本信息 / Reading device version info..."

    local dut_version_file="${DUT_VERSION_FILE:-/sbin/version}"
    local current_version
    if [[ "$PLATFORM" == "opensync" ]]; then
    	current_version=$(bash "$SSH_SCRIPT" "$PROJECT"  "cat $dut_version_file" 2>/dev/null | tail -1 | tr -d '\r\n')
    elif [[ "$PLATFORM" == "prplos" ]]; then
	current_version=$(bash "$SSH_SCRIPT" "$PROJECT"  "cat $dut_version_file" 2>/dev/null | grep "^VERSION_ID="| cut -d'=' -f2 | tr -d '"')
	if echo "$current_version" | grep -q "$expected_version"; then
    		echo "匹配: $expected_version 是 $current_version 的子集"
		version_matched=true
	else
    		echo "不匹配"
		version_matched=false
	fi
    else
	    version_matched=false
    fi
    print_info "预期版本 / Expected version: $expected_version"
    print_info "实际版本 / Actual version: $current_version"

    # 验证版本是否匹配（支持 -dev 标识）
    # Verify version match (support -dev identifier)
    # 例如 / Example:
    #   输入 / Input: 12.2.6.25-WF710X-T26011502
    #   实际 / Actual: 12.2.6.25-WF710X-dev-T26011502 (正确 / Correct)

    local version_matched=false
    if [[ "$PLATFORM" == "opensync" ]]; then	
    	if [[ "$expected_version" =~ ^(.+)-(T[0-9]+)$ ]]; then
        	# 提取基础版本和时间戳 / Extract base version and timestamp
        	local base_version="${BASH_REMATCH[1]}"
        	local timestamp="${BASH_REMATCH[2]}"

        	# 检查实际版本是否匹配以下任一格式：
        	# Check if actual version matches any of the following formats:
        	# 1. base-dev-timestamp (例如 / e.g.: 12.2.6.25-WF710X-dev-T26011502)
        	# 2. base-timestamp (例如 / e.g.: 12.2.6.25-WF710X-T26011502)
        	if [[ "$current_version" == *"${base_version}-dev-${timestamp}"* ]] || \
           	[[ "$current_version" == *"${base_version}-${timestamp}"* ]]; then
            		version_matched=true
        	fi
    	else
        	# 没有时间戳格式，直接匹配（允许 -dev）
        	# No timestamp format, direct match (allow -dev)
        	if [[ "$current_version" == *"$expected_version"* ]] || \
           	[[ "$current_version" == *"${expected_version}-dev"* ]]; then
            		version_matched=true
        	fi
    	fi
    else
            		version_matched=true
    fi	    
    # 根据匹配结果显示信息 / Display information based on match result
    if [[ "$version_matched" == true ]]; then
        echo ""
        echo -e "${COLOR_GREEN}========================================${COLOR_RESET}"
        echo -e "${COLOR_GREEN}✅ 升级成功！ / Upgrade Successful!${COLOR_RESET}"
        echo -e "${COLOR_GREEN}========================================${COLOR_RESET}"
        echo -e "${COLOR_GREEN}预期版本 / Expected: $expected_version${COLOR_RESET}"
        echo -e "${COLOR_GREEN}实际版本 / Actual: $current_version${COLOR_RESET}"
        echo -e "${COLOR_GREEN}========================================${COLOR_RESET}"
        return 0
    else
        echo ""
        echo -e "${COLOR_YELLOW}========================================${COLOR_RESET}"
        echo -e "${COLOR_YELLOW}⚠️  版本不匹配 / Version Mismatch${COLOR_RESET}"
        echo -e "${COLOR_YELLOW}========================================${COLOR_RESET}"
        echo -e "${COLOR_YELLOW}预期版本 / Expected: $expected_version${COLOR_RESET}"
        echo -e "${COLOR_YELLOW}实际版本 / Actual: $current_version${COLOR_RESET}"
        echo -e "${COLOR_YELLOW}请手动检查设备 / Please manually check device${COLOR_RESET}"
        echo -e "${COLOR_YELLOW}========================================${COLOR_RESET}"
        return 1
    fi
}

# ========== 主流程 / Main Process ==========
main() {
    # 显示帮助 / Show help
    if [[ "$1" == "help" ]] || [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
        show_help
        exit 0
    fi
    
    # 加载配置 / Load configuration
    load_config
    
    # 获取项目和版本 / Get project and version
    PROJECT="${1:-${DEFAULT_PROJECT}}"
    VERSION="${2:-${DEFAULT_VERSION}}"
    
    # 加载项目配置 / Load project configuration
    load_project_config "$PROJECT"
    
    # 打印开始信息 / Print start information
    echo ""
    echo -e "${COLOR_MAGENTA}========================================${COLOR_RESET}"
    echo -e "${COLOR_MAGENTA}完整构建和升级流程 / Complete Build and Upgrade Process${COLOR_RESET}"
    echo -e "${COLOR_MAGENTA}========================================${COLOR_RESET}"
    echo -e "${COLOR_BLUE}项目 / Project: $PROJECT${COLOR_RESET}"
    echo -e "${COLOR_BLUE}版本 / Version: $VERSION${COLOR_RESET}"
    echo -e "${COLOR_BLUE}工作区 / Workspace: $WORKSPACE${COLOR_RESET}"
    echo -e "${COLOR_BLUE}目标设备 / Target Device: ${DUT_SSH_HOST:-192.168.40.1}${COLOR_RESET}"
    echo -e "${COLOR_MAGENTA}========================================${COLOR_RESET}"
    
    # 执行各步骤 / Execute steps
    update_version "$VERSION"
    build_firmware "$PROJECT" "$VERSION"
    locate_image "$VERSION"
    upgrade_firmware "$PROJECT" "$VERSION"
    wait_device_disconnect
    wait_ssh_ready
    verify_version "$VERSION"
    
    # 打印完成信息 / Print completion information
    echo ""
    echo -e "${COLOR_MAGENTA}========================================${COLOR_RESET}"
    echo -e "${COLOR_MAGENTA}🎉 流程完成 / Process Completed${COLOR_RESET}"
    echo -e "${COLOR_MAGENTA}========================================${COLOR_RESET}"
    echo -e "${COLOR_GREEN}项目 / Project: $PROJECT${COLOR_RESET}"
    echo -e "${COLOR_GREEN}版本 / Version: $VERSION${COLOR_RESET}"
    echo -e "${COLOR_GREEN}工作区 / Workspace: $WORKSPACE${COLOR_RESET}"
    echo -e "${COLOR_MAGENTA}========================================${COLOR_RESET}"
}

# ========== 执行主流程 / Execute Main Process ==========
main "$@"
