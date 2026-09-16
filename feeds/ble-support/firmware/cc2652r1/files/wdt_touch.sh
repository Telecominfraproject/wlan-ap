#!/bin/sh
DEF_DIR="/etc/default"
LOG_DIR="/var/log"
RUN_DIR="/var/run"
SYSINFO_DIR="/var/sysinfo"
GPIO_DIR="/sys/class/gpio"

DEV_VENDOR="$(cat $SYSINFO_DIR/board_name | awk -F, '{print $1}')"
DEV_MODEL="$(cat $SYSINFO_DIR/board_name | awk -F, '{print $2}')"

tty="$(uci -q get ble_chip.$DEV_MODEL.tty)"
[ -z "$tty" ] && tty='/dev/ttyMSM1'
exec 100>"/var/lock/ble.lock"
fpid="/var/run/wdt_touch.pid"
exit=0
flock 100

if [ "$1" == "inactive" ]; then
	com-wr.sh "$tty" 1 "\x01\x8B\xFE\x01\x00" >/dev/null      # watchdog off
else
	com-wr.sh "$tty" 1 "\x01\x8C\xFE\x02\x05\x00" >/dev/null  # set 5 seconds timeout
	com-wr.sh "$tty" 1 "\x01\x8A\xFE\x01\x00" >/dev/null      # watchdog on
fi

if [ "$1" == "active" -o "$1" == "inactive" ]; then
	exit=1
elif [ -f "$fpid" ]; then
	{ pgrep -f "wdt_touch.sh" | grep -w "$(cat "$fpid")"; } >/dev/null 2>&1 && exit=1
fi

[ "$exit" == "1" ] || echo $$ > "$fpid"
exec 100>&-
[ "$exit" == "1" ] && exit

[ -e $GPIO_DIR/gpio17 ] || echo 17 > $GPIO_DIR/export
echo "out" > $GPIO_DIR/gpio17/direction

while true
do
  echo 0 > $GPIO_DIR/gpio17/value
  sleep 1
  echo 1 > $GPIO_DIR/gpio17/value
  sleep 1
done
