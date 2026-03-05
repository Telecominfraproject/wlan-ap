#!/bin/bash
# ============================================
# Jumphost 连接脚本 (支持多项目)
# Jumphost Connection Script (Multi-Project Support)
# ============================================
# 功能 / Function: 
#   SSH 连接到 Jumphost 并执行命令
#   SSH to Jumphost and execute commands
#   支持从配置文件读取多项目配置
#   Support loading multi-project configuration from config file
# ============================================

set -e  # 遇到错误立即退出 / Exit on error

# ============================================
# 配置 / Configuration
# ============================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${SCRIPT_DIR}/ssh_dut.conf"

# 默认配置 / Default configuration
CURRENT_PROJECT=""
SSH_USER="actiontec"
SSH_PASSWORD="Hugh1234@AEI"
SSH_HOST="172.16.10.81"

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

# 打印信息 / Print info
print_info() {
    echo -e "${BLUE}>>> $1${NC}"
}

# 打印成功 / Print success
print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# 打印错误 / Print error
print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# 显示帮助信息 / Show help
show_help() {
    cat << HELPEOF
========================================
Jumphost 连接脚本 / Jumphost Connection Script
========================================

用法 / Usage:
    $0 [project] <command>

参数 / Parameters:
    project    (可选) 项目名称，从配置文件读取对应项目配置
               (Optional) Project name, load corresponding project config
               不指定则使用 CURRENT_PROJECT 配置
               If not specified, use CURRENT_PROJECT setting
               
    command    要在 Jumphost 上执行的命令
               Command to execute on Jumphost

示例 / Examples:
    # 使用当前项目配置
    # Use current project configuration
    $0 "pwd"
    $0 "ifconfig"
    
    # 指定项目配置
    # Specify project configuration
    $0 WF710G "pwd"
    $0 WF728 "uname -a"

配置文件 / Configuration:
    ${CONFIG_FILE}
    
    设置 CURRENT_PROJECT 变量指定默认项目
    Set CURRENT_PROJECT variable to specify default project

支持的项目 / Supported Projects:
    - WF710G
    - WF728
    - WF630
    - (更多项目可在配置文件中添加 / More can be added in config)

========================================
HELPEOF
}

# 加载配置文件 / Load configuration
load_config() {
    local project="$1"
    
    if [[ ! -f "${CONFIG_FILE}" ]]; then
        print_error "配置文件不存在 / Configuration file not found: ${CONFIG_FILE}"
        exit 1
    fi
    
    # 读取 CURRENT_PROJECT / Read CURRENT_PROJECT
    while IFS='=' read -r key value; do
        [[ "$key" =~ ^#.*$ ]] && continue
        [[ -z "$key" ]] && continue
        key=$(echo "$key" | xargs)
        value=$(echo "$value" | xargs)
        
        if [[ "$key" == "CURRENT_PROJECT" ]]; then
            CURRENT_PROJECT="$value"
        fi
    done < "${CONFIG_FILE}"
    
    # 如果指定了项目参数，使用指定的项目
    # If project parameter is specified, use it
    if [[ -n "$project" ]]; then
        CURRENT_PROJECT="$project"
    fi
    
    if [[ -z "$CURRENT_PROJECT" ]]; then
        print_error "未指定项目 / Project not specified"
        print_error "请在配置文件中设置 CURRENT_PROJECT 或通过参数指定项目"
        print_error "Please set CURRENT_PROJECT in config file or specify via parameter"
        exit 1
    fi
    
    print_info "加载项目配置 / Loading project configuration: ${CURRENT_PROJECT}"
    
    # 读取项目配置 / Load project configuration
    local prefix="${CURRENT_PROJECT}_JUMPHOST_"
    while IFS='=' read -r key value; do
        [[ "$key" =~ ^#.*$ ]] && continue
        [[ -z "$key" ]] && continue
        key=$(echo "$key" | xargs)
        value=$(echo "$value" | xargs)
        
        case "$key" in
            ${prefix}SSH_USER)
                SSH_USER="$value"
                ;;
            ${prefix}SSH_PASSWORD)
                SSH_PASSWORD="$value"
                ;;
            ${prefix}SSH_HOST)
                SSH_HOST="$value"
                ;;
        esac
    done < "${CONFIG_FILE}"
    
    # 验证配置 / Validate configuration
    if [[ -z "${SSH_USER}" || -z "${SSH_PASSWORD}" || -z "${SSH_HOST}" ]]; then
        print_error "项目配置不完整 / Project configuration incomplete: ${CURRENT_PROJECT}"
        print_error "请检查配置文件 / Please check configuration file"
        exit 1
    fi
}

# 执行远程命令 / Execute remote command
execute_command() {
    local command="$1"
    
    echo "========================================"
    echo "Jumphost 连接脚本 / Jumphost Connection Script"
    echo "========================================"
    print_info "连接到 Jumphost / Connecting to Jumphost"
    echo "    项目 / Project: ${CURRENT_PROJECT}"
    echo "    主机 / Host: ${SSH_USER}@${SSH_HOST}"
    echo "    执行命令 / Executing command: ${command}"
    echo "========================================"
    echo ""
    
    # 使用 sshpass 执行命令 / Execute command using sshpass
    if ! command -v sshpass &> /dev/null; then
        print_error "sshpass 未安装 / sshpass not installed"
        print_error "请安装: sudo apt-get install sshpass"
        print_error "Please install: sudo apt-get install sshpass"
        exit 1
    fi
    
    # 执行 SSH 命令 / Execute SSH command
    sshpass -p "${SSH_PASSWORD}" ssh \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        -o HostKeyAlgorithms=+ssh-dss \
        -o KexAlgorithms=+diffie-hellman-group1-sha1 \
        "${SSH_USER}@${SSH_HOST}" \
        "${command}"
    
    local exit_code=$?
    echo ""
    
    if [[ $exit_code -eq 0 ]]; then
        print_success "命令执行成功 / Command executed successfully"
    else
        print_error "命令执行失败 / Command execution failed (exit code: ${exit_code})"
    fi
    
    return $exit_code
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

    local project=""
    local command=""

    if [[ $# -eq 1 ]]; then
        command="$1"
    elif [[ $# -ge 2 ]]; then
        project="$1"
        shift
        command="$*"
    else
        print_error "参数错误 / Invalid parameters"
        show_help
        exit 1
    fi

    load_config "$project"
    execute_command "$command"
    exit $?
}


# 执行主程序 / Execute main program
main "$@"
