#!/bin/bash
# List active console sessions

echo "=== Active Sessions ==="
screen -ls 2>/dev/null | grep gw3-console | while read line; do
    session=$(echo "$line" | awk '{print $1}')
    port=$(echo "$session" | sed 's/.*gw3-console-//' | cut -d. -f1)
    log=$(ls -t local/records/console_${port}_*.log 2>/dev/null | head -1)
    echo "$session (/dev/$port) - Log: $log"
done

echo ""
echo "=== Available Ports ==="
for p in /dev/tty{USB,ACM}*; do
    [ -e "$p" ] || continue
    n=$(basename "$p")
    screen -ls 2>/dev/null | grep -q "gw3-console-$n" && s="✓ active" || s="○ idle"
    echo "$p $s"
done
