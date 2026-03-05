#!/bin/bash
# Auto-start console if not running
# Usage: auto_console.sh [port]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT="$1"
[ -z "$PORT" ] && PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1)

# If no port found and running in WSL, try to attach USB device
if { [ -z "$PORT" ] || [ ! -e "$PORT" ]; } && grep -qEi "WSL|Microsoft" /proc/version 2>/dev/null; then
    echo "No USB serial port found in WSL"
    echo "Attempting to attach USB device from Windows..."
    "$SCRIPT_DIR/attach_uart_wsl.sh"
    sleep 2
    # Re-check for serial port
    PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -1)
fi

[ -z "$PORT" ] || [ ! -e "$PORT" ] && {
    echo "No USB serial port found"
    echo "Available: $(ls /dev/tty{USB,ACM}* 2>/dev/null | tr '\n' ' ')"
    exit 1
}

PORT_NAME=$(basename "$PORT")
SESSION="gw3-console-$PORT_NAME"

if screen -ls | grep -q "$SESSION"; then
    LOG=$(ls -t local/records/console_${PORT_NAME}_*.log 2>/dev/null | head -1)
    echo "✓ Console active: $SESSION"
    echo "  Port: $PORT"
    echo "  Log:  $LOG ($(du -h "$LOG" 2>/dev/null | cut -f1))"
    echo "  Attach: screen -r $SESSION"
else
    echo "Starting console..."
    $(dirname "$0")/start_console.sh "$PORT"
fi
