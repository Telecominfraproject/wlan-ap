#!/bin/bash
# Start console session with logging
# Usage: start_console.sh [port] [baud]

PORT="${1:-/dev/ttyUSB0}"
BAUD="${2:-115200}"
PORT_NAME=$(basename "$PORT")
SESSION="gw3-console-$PORT_NAME"

[ -e "$PORT" ] || { echo "Error: $PORT not found"; ls /dev/tty{USB,ACM}* 2>/dev/null; exit 1; }

screen -ls | grep -q "$SESSION" && { echo "Session $SESSION already running"; exit 0; }

mkdir -p local/records
LOG="local/records/console_${PORT_NAME}_$(date +%Y%m%d_%H%M%S).log"

screen -L -Logfile "$LOG" -dmS "$SESSION" "$PORT" "$BAUD"
sleep 0.5

screen -ls | grep -q "$SESSION" && {
    echo "✓ Console started: $SESSION"
    echo "  Port: $PORT ($BAUD)"
    echo "  Log:  $LOG"
    echo "  Attach: screen -r $SESSION"
} || {
    echo "Error: Failed to start session"
    exit 1
}
