#!/bin/bash
# 定期从板子采集FCM内存数据

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="$SCRIPT_DIR/fcm_memory_log.csv"

# 初始化日志文件
if [ ! -f "$LOG_FILE" ]; then
    echo "timestamp,pid,vmrss_kb,vmdata_kb" > "$LOG_FILE"
fi

# 采集数据
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
DATA=$($SCRIPT_DIR/ssh2jumphost2dut.sh WF710G "cat /proc/8957/status 2>/dev/null | grep -E 'VmRSS|VmData' | awk '{print \$2}'" 2>/dev/null | grep -oE '^[0-9]+')

if [ -n "$DATA" ]; then
    RSS=$(echo "$DATA" | sed -n '1p')
    VDATA=$(echo "$DATA" | sed -n '2p')
    
    if [ -n "$RSS" ] && [ "$RSS" -gt 0 ]; then
        echo "$TIMESTAMP,8957,$RSS,$VDATA" >> "$LOG_FILE"
        RSS_MB=$((RSS / 1024))
        echo "[$TIMESTAMP] FCM内存: RSS=${RSS}kB (${RSS_MB}MB), Data=${VDATA}kB"
        
        # 显示最近5条记录
        echo "最近记录:"
        tail -5 "$LOG_FILE"
    fi
fi
