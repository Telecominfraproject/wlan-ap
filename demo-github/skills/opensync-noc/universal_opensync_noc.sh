#!/bin/bash
#==============================================================================
# Universal OpenSync NOC Automation / 通用 OpenSync NOC 自动化包装脚本
#==============================================================================
# 用法 / Usage:
#   ./universal_opensync_noc.sh [NODE_ID]
#==============================================================================

set -e

# ========== 颜色定义 / Color Definitions ==========
COLOR_RESET='\033[0m'
COLOR_RED='\033[31m'
COLOR_GREEN='\033[32m'
COLOR_YELLOW='\033[33m'
COLOR_BLUE='\033[34m'
COLOR_CYAN='\033[36m'
COLOR_MAGENTA='\033[35m'

# ========== 读取配置文件 / Load Configuration ==========
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="$SCRIPT_DIR/noc_config.conf"

# 检查配置文件是否存在
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo -e "${COLOR_RED}✗ 配置文件不存在 / Config file not found:${COLOR_RESET}"
    echo -e "  $CONFIG_FILE"
    exit 1
fi

# 从配置文件读取 WORKSPACE
WORKSPACE=$(grep "^WORKSPACE=" "$CONFIG_FILE" | cut -d'=' -f2 | tr -d '"')

# 如果配置文件中 WORKSPACE 为空，则自动检测
if [[ -z "$WORKSPACE" ]]; then
    WORKSPACE="$(dirname "$(dirname "$SCRIPT_DIR")")"
    echo -e "${COLOR_YELLOW}⚠️  WORKSPACE 未在配置文件中设置，使用自动检测:${COLOR_RESET}"
    echo -e "   $WORKSPACE"
fi

# Python 脚本路径
PYTHON_SCRIPT="$SCRIPT_DIR/universal_opensync_noc.py"

# ========== 显示帮助 / Show Help ==========
if [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
    echo -e "${COLOR_CYAN}========================================${COLOR_RESET}"
    echo -e "${COLOR_CYAN}OpenSync NOC 自动化脚本${COLOR_RESET}"
    echo -e "${COLOR_CYAN}OpenSync NOC Automation Script${COLOR_RESET}"
    echo -e "${COLOR_CYAN}========================================${COLOR_RESET}"
    echo -e ""
    echo -e "${COLOR_YELLOW}用法 / Usage:${COLOR_RESET}"
    echo -e "  ./universal_opensync_noc.sh [NODE_ID]"
    echo -e ""
    echo -e "${COLOR_YELLOW}参数 / Arguments:${COLOR_RESET}"
    echo -e "  NODE_ID    - 节点 ID (可选，默认从配置文件读取)"
    echo -e "               Node ID (optional, default from config)"
    echo -e ""
    echo -e "${COLOR_YELLOW}示例 / Examples:${COLOR_RESET}"
    echo -e "  ./universal_opensync_noc.sh                    # 使用默认节点ID"
    echo -e "  ./universal_opensync_noc.sh 1HG231000110       # 指定节点ID"
    echo -e ""
    echo -e "${COLOR_YELLOW}前置条件 / Prerequisites:${COLOR_RESET}"
    echo -e "  1. Selenium Grid 正在运行 / Selenium Grid is running"
    echo -e "     docker run -d -p 4444:4444 -p 7900:7900 \\"
    echo -e "       --name selenium selenium/standalone-chrome:latest"
    echo -e ""
    echo -e "  2. Python 3 和 Selenium 已安装 / Python 3 and Selenium installed"
    echo -e "     pip3 install selenium"
    echo -e ""
    echo -e "${COLOR_YELLOW}输出文件 / Output Files:${COLOR_RESET}"
    echo -e "  截图 / Screenshots: $WORKSPACE/screenshots/opensync-noc/"
    echo -e "  结果 / Results:     $WORKSPACE/results/opensync-noc/"
    echo -e ""
    echo -e "${COLOR_YELLOW}VNC 查看 / VNC Preview:${COLOR_RESET}"
    echo -e "  浏览器访问 / Open browser: http://localhost:7900"
    echo -e "  密码 / Password: secret"
    echo -e "${COLOR_CYAN}========================================${COLOR_RESET}"
    exit 0
fi

# ========== 检查 Python 脚本 ==========
if [[ ! -f "$PYTHON_SCRIPT" ]]; then
    echo -e "${COLOR_RED}✗ Python 脚本不存在 / Python script not found:${COLOR_RESET}"
    echo -e "  $PYTHON_SCRIPT"
    exit 1
fi

# ========== 检查 Python 环境 ==========
if ! command -v python3 &> /dev/null; then
    echo -e "${COLOR_RED}✗ Python 3 未安装 / Python 3 not installed${COLOR_RESET}"
    exit 1
fi

# ========== 检查 Selenium ==========
if ! python3 -c "import selenium" 2>/dev/null; then
    echo -e "${COLOR_YELLOW}⚠️  Selenium 未安装 / Selenium not installed${COLOR_RESET}"
    echo -e "${COLOR_BLUE}尝试安装 / Trying to install...${COLOR_RESET}"
    pip3 install selenium || {
        echo -e "${COLOR_RED}✗ 安装失败 / Installation failed${COLOR_RESET}"
        echo -e "${COLOR_YELLOW}请手动安装 / Please install manually:${COLOR_RESET}"
        echo -e "  pip3 install selenium"
        exit 1
    }
fi

# ========== 检查 Selenium Grid ==========
SELENIUM_URL=$(grep "^SELENIUM_URL=" "$SCRIPT_DIR/noc_config.conf" | cut -d'=' -f2 | tr -d '"')
if ! curl -s "$SELENIUM_URL/status" > /dev/null 2>&1; then
    echo -e "${COLOR_YELLOW}========================================${COLOR_RESET}"
    echo -e "${COLOR_YELLOW}⚠️  Selenium Grid 未运行${COLOR_RESET}"
    echo -e "${COLOR_YELLOW}⚠️  Selenium Grid not running${COLOR_RESET}"
    echo -e "${COLOR_YELLOW}========================================${COLOR_RESET}"
    echo -e ""
    echo -e "${COLOR_BLUE}启动 Selenium Grid / Start Selenium Grid:${COLOR_RESET}"
    echo -e "  docker run -d -p 4444:4444 -p 7900:7900 \\"
    echo -e "    --name selenium \\"
    echo -e "    selenium/standalone-chrome:latest"
    echo -e ""
    echo -e "${COLOR_BLUE}检查状态 / Check status:${COLOR_RESET}"
    echo -e "  curl ${SELENIUM_URL}/status"
    echo -e ""
    echo -e "${COLOR_BLUE}VNC 预览 / VNC Preview:${COLOR_RESET}"
    echo -e "  http://localhost:7900 (密码 / Password: secret)"
    echo -e "${COLOR_YELLOW}========================================${COLOR_RESET}"
    exit 1
fi

# ========== 运行 Python 脚本 ==========
echo -e "${COLOR_CYAN}启动 OpenSync NOC 自动化 / Starting OpenSync NOC Automation...${COLOR_RESET}"
python3 "$PYTHON_SCRIPT" "$@"

# ========== 退出状态 / Exit Status ==========
EXIT_CODE=$?
if [[ $EXIT_CODE -eq 0 ]]; then
    echo -e "\n${COLOR_GREEN}✅ 成功完成！ / Successfully completed!${COLOR_RESET}"
else
    echo -e "\n${COLOR_RED}✗ 执行失败 / Execution failed (退出码 / Exit code: $EXIT_CODE)${COLOR_RESET}"
fi

exit $EXIT_CODE
