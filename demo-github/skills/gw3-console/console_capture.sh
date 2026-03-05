#!/bin/bash
# Execute command and capture output
# Usage: console_capture.sh [port_name] 'command' [wait]

[[ "$1" =~ ^tty ]] && { PORT="$1"; CMD="$2"; WAIT="${3:-2}"; } || { PORT="ttyUSB0"; CMD="$1"; WAIT="${2:-2}"; }
SESSION="gw3-console-$PORT"
OUT="local/records/console_${PORT}_cap_$(date +%s).txt"

[ -z "$CMD" ] && { echo "Usage: $0 [port] 'command' [wait]"; exit 1; }
screen -ls | grep -q "$SESSION" || { echo "Error: $SESSION not running"; exit 1; }

mkdir -p local/records
echo "[$SESSION] $CMD"
screen -S "$SESSION" -X stuff "$CMD"$'\015'
sleep "$WAIT"
screen -S "$SESSION" -X hardcopy "$OUT"
cat "$OUT"
