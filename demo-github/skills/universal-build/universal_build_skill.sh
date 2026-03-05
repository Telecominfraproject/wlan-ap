#!/bin/bash
# skill Shell Script
# Description: 通用 OpenSync 固件构建脚本 / Universal OpenSync Firmware Build Script
# 支持多项目配置和版本管理 / Support multi-project configuration and version management

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${CONFIG_FILE:-$SCRIPT_DIR/build_config.env}"

# 加载配置文件 / Load configuration file
if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
else
    echo "错误 / Error: 配置文件未找到 / Config file not found: $CONFIG_FILE" >&2
    exit 1
fi

WORKSPACE_DEFAULT="${WORKSPACE:-$(dirname $(dirname $(dirname $SCRIPT_DIR)))}"
OWNER=$(echo $HOME | awk -F '/' '{print $3}')

# 加载项目配置 / Load project configuration
load_project_config() {
    local project="${1:-$DEFAULT_PROJECT}"
    MODEL_NAME=$(eval echo \$${project}_MODEL_NAME)
    CONTAINER_DIR=$(eval echo \$${project}_CONTAINER_DIR)
    MAKE_CMD=$(eval echo \$${project}_MAKE_CMD)
    DOCKER_IMAGE=$(eval echo \$${project}_DOCKER_IMAGE)
    DOCKER_EXTRA_FLAG=$(eval echo \$${project}_DOCKER_EXTRA_FLAG)
    # 尝试读取项目特定的 WORKSPACE，如果为空则使用默认值
    local project_workspace=$(eval echo \$${project}_WORKSPACE)
    WORKSPACE="${project_workspace:-$WORKSPACE_DEFAULT}"
    
    if [ -z "$MODEL_NAME" ]; then
        echo "错误 / Error: 未知项目 / Unknown project: '$project'" >&2
        exit 1
    fi
    
    DOCKER_NAME="docker_${OWNER}_${MODEL_NAME}"
}

# 构建固件 / Build firmware
build_firmware() {
    local project="${1:-$DEFAULT_PROJECT}"
    local version="${2:-}"
    
    load_project_config "$project"
    
    echo "========================================" >&2
    echo "固件构建脚本 / Firmware Build Script" >&2
    echo "========================================" >&2
    echo ">>> 开始构建 / Starting build: $MODEL_NAME" >&2
    echo "    项目 / Project: $project" >&2
    echo "    模型 / Model: $MODEL_NAME" >&2
    
    # 更新版本号 / Update version number
    if [ -n "$version" ]; then
        echo "    版本 / Version: $version" >&2
        echo ">>> 更新版本文件 / Updating version file..." >&2

        echo "$version" > "$WORKSPACE/WiFi7/opensync_ws/OPENSYNC/$MODEL_NAME/modify/qsdk/version"
        echo "    ✓ 版本已更新 / Version updated" >&2
    else
        echo "    版本 / Version: (使用现有版本 / Using existing version)" >&2
    fi
    echo "" >&2
    
    cd "$WORKSPACE"
    
    # 停止并删除旧容器 / Stop and remove old container
    echo ">>> 准备 Docker 容器 / Preparing Docker container..." >&2
    docker stop "$DOCKER_NAME" 2>/dev/null && echo "    ✓ 已停止旧容器 / Stopped old container" >&2 || true
    docker rm "$DOCKER_NAME" 2>/dev/null && echo "    ✓ 已删除旧容器 / Removed old container" >&2 || true
    
    # 启动新容器 / Start new container
    USERID=$(id -u)
    echo "    启动新容器 / Starting new container: $DOCKER_NAME" >&2
    echo "    镜像 / Image: $DOCKER_IMAGE" >&2


    docker run --net=host -e BUILDER_UID=$USERID -itd \
        --name "$DOCKER_NAME" -v "$WORKSPACE:/src" $DOCKER_EXTRA_FLAG "$DOCKER_IMAGE" >&2
    echo "    ✓ 容器已启动 / Container started" >&2
    echo "" >&2
    
    # 生成日志文件名 / Generate log file names
    TIMESTAMP=$(date +%y%m%d-%H%M%S)
    LOG_FILE="${MODEL_NAME}_${TIMESTAMP}.log"
    ERR_FILE="${MODEL_NAME}_${TIMESTAMP}.errlog"
    
    echo ">>> 执行构建 / Executing build..." >&2
    echo "    工作目录 / Working directory: $CONTAINER_DIR" >&2
    echo "    构建命令 / Build command: $MAKE_CMD" >&2
    echo "    日志文件 / Log file: $LOG_FILE" >&2
    echo "    错误日志 / Error log: $ERR_FILE" >&2
    echo "    编译命令 / make command: $MAKE_CMD" >&2
    echo "========================================" >&2
    echo ">>> 构建中，请稍候... / Building, please wait..." >&2
    echo "" >&2
    # 执行构建 / Execute build
    echo "###build command $WORKSPACE   $USERID:$USERID $DOCKER_NAME $CONTAINER_DIR $MAKE_CMD /src/$LOG_FILE /src/$ERR_FILE"
    docker exec -u $USERID:$USERID -i "$DOCKER_NAME" bash -c \
        "cd $CONTAINER_DIR && $MAKE_CMD >/src/$LOG_FILE 2>/src/$ERR_FILE"
    
    local exit_code=$?
    
    echo "" >&2
    echo "========================================" >&2
    if [ $exit_code -eq 0 ]; then
        echo "✓ 构建成功 / Build successful!" >&2
    else
        echo "✗ 构建失败 / Build failed!" >&2
        echo "  请检查日志 / Please check logs: $ERR_FILE" >&2
    fi
    echo "========================================" >&2
    
    # 清理容器 / Clean up container
    if [ "$KEEP_CONTAINER" = "false" ]; then
        echo ">>> 清理容器 / Cleaning up container..." >&2
        docker stop "$DOCKER_NAME" 2>/dev/null || true
        docker rm "$DOCKER_NAME" 2>/dev/null || true
        echo "    ✓ 容器已清理 / Container cleaned up" >&2
    else
        echo ">>> 保留容器用于调试 / Keeping container for debugging" >&2
    fi
    
    # 输出 JSON 结果 / Output JSON result
    cat <<JSONEOF
{
  "project": "$project",
  "model": "$MODEL_NAME",
  "status": "$([ $exit_code -eq 0 ] && echo 'success' || echo 'failed')",
  "exit_code": $exit_code,
  "log_file": "$WORKSPACE/$LOG_FILE",
  "error_log": "$WORKSPACE/$ERR_FILE",
  "timestamp": "$TIMESTAMP"
}
JSONEOF
}

# 获取构建状态 / Get build status
get_build_status() {
    local project="${1:-$DEFAULT_PROJECT}"
    load_project_config "$project"
    
    echo "========================================" >&2
    echo "构建状态查询 / Build Status Query" >&2
    echo "========================================" >&2
    echo ">>> 项目 / Project: $project" >&2
    echo "    容器名称 / Container name: $DOCKER_NAME" >&2
    
    local status=$(docker ps --filter "name=$DOCKER_NAME" --format "{{.Status}}" 2>/dev/null || echo "")
    local running=false
    
    if [ -n "$status" ]; then
        running=true
        echo "    状态 / Status: 运行中 / Running" >&2
        echo "    详情 / Details: $status" >&2
    else
        echo "    状态 / Status: 未运行 / Not running" >&2
    fi
    echo "========================================" >&2
    
    cat <<JSONEOF
{
  "container_name": "$DOCKER_NAME",
  "running": $running,
  "status": "$status"
}
JSONEOF
}

# 清理构建目录 / Clean build directory
clean_build() {
    local project="${1:-$DEFAULT_PROJECT}"
    
    echo "========================================" >&2
    echo "清理构建目录 / Clean Build Directory" >&2
    echo "========================================" >&2
    echo ">>> 项目 / Project: $project" >&2
    
    local build_dir="$WORKSPACE/WiFi7/sdk.qca.comp/qca/qca-networking/qsdk/build_dir/target-arm/opensync-6.6"
    echo "    目录 / Directory: $build_dir" >&2
    
    if [ -d "$build_dir" ]; then
        echo ">>> 删除构建文件 / Removing build files..." >&2
        rm -rf "$build_dir"
        echo "    ✓ 清理完成 / Cleanup completed" >&2
        echo "========================================" >&2
        echo '{"status":"cleaned"}' 
    else
        echo "    ! 目录不存在 / Directory not found" >&2
        echo "========================================" >&2
        echo '{"status":"not_found"}'
    fi
}

# 显示配置 / Show configuration
show_config() {
    echo "========================================" >&2
    echo "构建配置 / Build Configuration" >&2
    echo "========================================" >&2
    echo "配置文件 / Config File: $CONFIG_FILE" >&2
    echo "工作空间 / Workspace: $WORKSPACE" >&2
    echo "默认项目 / Default Project: $DEFAULT_PROJECT" >&2
    echo "所有者 / Owner: $OWNER" >&2
    echo "========================================" >&2
}

# 主函数 / Main function
main() {
    local action="${1:-help}"
    shift || true
    
    case "$action" in
        build)
            build_firmware "$@"
            ;;
        status)
            get_build_status "$@"
            ;;
        clean)
            clean_build "$@"
            ;;
        config)
            show_config
            ;;
        help|*)
            cat >&2 <<HELPEOF
========================================
通用构建脚本使用说明 / Universal Build Script Usage
========================================

用法 / Usage: $0 <action> [project] [version]

操作 / Actions:
  build [project] [version]  - 构建固件 / Build firmware
  status [project]           - 检查构建状态 / Check build status
  clean [project]            - 清理构建目录 / Clean build directory
  config                     - 显示配置 / Show configuration
  help                       - 显示帮助 / Show this help

示例 / Examples:
  $0 build WF710G 12.2.6.25-WF710X-T2601203
  $0 status WF710G
  $0 clean WF710G
  $0 config

========================================
HELPEOF
            ;;
    esac
}

# 执行主函数 / Execute main function
[ "${BASH_SOURCE[0]}" = "${0}" ] && main "$@"
