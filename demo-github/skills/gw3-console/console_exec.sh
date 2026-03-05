#!/bin/bash
# Send command to console
# Usage: console_exec.sh [port_name] 'command' [wait]

[[ "$1" =~ ^tty ]] && { PORT="$1"; CMD="$2"; WAIT="${3:-1}"; } || { PORT="ttyUSB0"; CMD="$1"; WAIT="${2:-1}"; }
SESSION="gw3-console-$PORT"

[ -z "$CMD" ] && { echo "Usage: $0 [port] 'command' [wait]"; screen -ls | grep gw3-console; exit 1; }
screen -ls | grep -q "$SESSION" || { echo "Error: $SESSION not running"; exit 1; }

echo "[$SESSION] $CMD"
screen -S "$SESSION" -X stuff "$CMD"$'\015'
sleep "$WAIT"
