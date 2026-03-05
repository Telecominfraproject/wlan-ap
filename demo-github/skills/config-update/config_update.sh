#!/bin/bash
# config_update.sh — Update variables in skills_config.conf
# Usage: ./config_update.sh <VAR_NAME> <VALUE> [config_file]
#        ./config_update.sh --show [PROJECT] [config_file]
#        ./config_update.sh --list [config_file]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEFAULT_CONF="${SCRIPT_DIR}/../skills_config.conf"

usage() {
    echo "Usage:"
    echo "  $0 <VAR_NAME> <VALUE>          Update a variable"
    echo "  $0 --show [PROJECT]             Show all vars for a project (e.g. WF728)"
    echo "  $0 --list                       List all variable names"
    echo ""
    echo "Examples:"
    echo "  $0 WF728_WORKSPACE /home/user/Project/WF728_new"
    echo "  $0 WF728_DUT_SSH_HOST 192.168.1.1"
    echo "  $0 DEFAULT_PROJECT WF728"
    echo "  $0 --show WF728"
}

CONF_FILE="${3:-${DEFAULT_CONF}}"
[[ -z "$1" ]] && { usage; exit 1; }
[[ ! -f "$CONF_FILE" ]] && { echo "ERROR: Config file not found: $CONF_FILE"; exit 1; }

# --list: 列出所有变量名
if [[ "$1" == "--list" ]]; then
    echo "Variables in $(basename "$CONF_FILE"):"
    grep -E '^[A-Z_]+=.*' "$CONF_FILE" | cut -d= -f1 | grep -v '^#'
    exit 0
fi

# --show: 显示某项目所有变量
if [[ "$1" == "--show" ]]; then
    PROJECT="${2:-}"
    if [[ -n "$PROJECT" ]]; then
        echo "=== ${PROJECT} variables ==="
        grep -E "^${PROJECT}_[A-Z_]+=" "$CONF_FILE" | grep -v '^#'
    else
        echo "=== All variables ==="
        grep -E '^[A-Z_]+=' "$CONF_FILE" | grep -v '^#'
    fi
    exit 0
fi

VAR_NAME="$1"
NEW_VALUE="$2"
[[ -z "$VAR_NAME" || -z "$NEW_VALUE" ]] && { usage; exit 1; }

# 检查变量是否存在
if ! grep -qE "^${VAR_NAME}=" "$CONF_FILE" && ! grep -qE "^${VAR_NAME}=\"" "$CONF_FILE"; then
    echo "ERROR: Variable '${VAR_NAME}' not found in $CONF_FILE"
    echo "Run '$0 --list' to see available variables"
    exit 1
fi

# 读取旧值
OLD_LINE=$(grep -E "^${VAR_NAME}=" "$CONF_FILE" | head -1)
OLD_VALUE=$(echo "$OLD_LINE" | cut -d= -f2- | sed 's/^"//;s/"$//')

# 判断是否需要引号（原值有引号，或新值含空格/特殊字符）
if echo "$OLD_LINE" | grep -q "^${VAR_NAME}=\""; then
    NEW_LINE="${VAR_NAME}=\"${NEW_VALUE}\""
else
    NEW_LINE="${VAR_NAME}=${NEW_VALUE}"
fi

# 用 sed 替换（精确匹配行首）
sed -i "s|^${VAR_NAME}=.*|${NEW_LINE}|" "$CONF_FILE"

echo "Updated: ${VAR_NAME}"
echo "  Old: ${OLD_VALUE}"
echo "  New: ${NEW_VALUE}"
