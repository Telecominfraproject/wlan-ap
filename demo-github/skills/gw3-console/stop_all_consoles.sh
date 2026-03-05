#!/bin/bash
# Stop all console sessions

screen -ls 2>/dev/null | grep gw3-console | awk '{print $1}' | while read s; do
    echo "Stopping $s"
    screen -S "$s" -X quit
done
