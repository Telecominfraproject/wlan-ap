#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause

SYSINFO_DIR="/var/sysinfo"
GPIO_DIR="/sys/class/gpio"
LOCK_FILE="/var/lock/ble.lock"
PID_FILE="/var/run/wdt_touch.pid"

DEV_MODEL="$(awk -F, '{print $2}' "$SYSINFO_DIR/board_name" 2>/dev/null)"
tty="$(uci -q get "ble_chip.${DEV_MODEL}.tty")"
wdt_pin="$(uci -q get "ble_chip.${DEV_MODEL}.wdt_pin")"

[ -n "$tty" ] || tty="/dev/ttyMSM1"
[ -n "$wdt_pin" ] || wdt_pin="17"

case "$wdt_pin" in
	*[!0-9]*|'')
		logger -st wdt_touch "invalid watchdog GPIO '$wdt_pin'"
		exit 1
		;;
esac

mode=${1:-run}
case "$mode" in
	run|active|inactive) ;;
	*)
		echo "usage: $0 [active|inactive]" >&2
		exit 2
		;;
esac

# Avoid starting a second background watchdog process.
if [ "$mode" = "run" ] && [ -r "$PID_FILE" ]; then
	old_pid=$(cat "$PID_FILE" 2>/dev/null)
	case "$old_pid" in
		''|*[!0-9]*) old_pid= ;;
	esac
	[ -n "$old_pid" ] && kill -0 "$old_pid" 2>/dev/null && exit 0
fi

mkdir -p "${LOCK_FILE%/*}" || exit 1
touch "$LOCK_FILE" || exit 1
exec 9>"$LOCK_FILE"
flock 9 || exit 1

case "$mode" in
	inactive)
		com-wr.sh "$tty" 1 '\001\213\376\001\000'
		exit $?
		;;
	active|run)
		com-wr.sh "$tty" 1 '\001\214\376\002\005\000' || exit 1
		com-wr.sh "$tty" 1 '\001\212\376\001\000' || exit 1
		;;
esac

[ "$mode" = "run" ] || exit 0

printf '%s\n' "$$" > "$PID_FILE" || exit 1
exec 9>&-

cleanup() {
	if [ -r "$PID_FILE" ] && [ "$(cat "$PID_FILE" 2>/dev/null)" = "$$" ]; then
		rm -f "$PID_FILE"
	fi
}
trap cleanup EXIT
trap 'exit 0' HUP INT TERM

watchdog_gpio="$GPIO_DIR/gpio$wdt_pin"
if [ ! -d "$watchdog_gpio" ]; then
	printf '%s\n' "$wdt_pin" > "$GPIO_DIR/export" 2>/dev/null || {
		logger -st wdt_touch "cannot export watchdog GPIO $wdt_pin"
		exit 1
	}
	[ -d "$watchdog_gpio" ] || exit 1
fi

printf '%s\n' out > "$watchdog_gpio/direction" 2>/dev/null || exit 1
while :; do
	printf '%s\n' 0 > "$watchdog_gpio/value" 2>/dev/null || exit 1
	sleep 1
	printf '%s\n' 1 > "$watchdog_gpio/value" 2>/dev/null || exit 1
	sleep 1
done
