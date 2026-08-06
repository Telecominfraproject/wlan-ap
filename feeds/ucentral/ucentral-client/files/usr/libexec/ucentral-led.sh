#!/bin/sh
#
# ucentral-led.sh - drive the cloud/controller-managed indicator LED.
#
# Called with "on" or "off" by:
#   - /etc/init.d/ucentral   on service stop, and when no gateway is configured
#   - ucentral-state         on online/offline transitions (real connection state)
#
# Boards without a cloud LED, or with LEDs disabled globally, are a no-op.
#

. /lib/functions.sh

case "$(board_name)" in
edgecore,eap104)
	LED_PATH="/sys/class/leds/green:cloud"
	;;
edgecore,eap105|\
edgecore,oap101|\
edgecore,oap101e|\
edgecore,eap111|\
edgecore,eap111e|\
edgecore,eap115|\
edgecore,eap115a)
	LED_PATH="/sys/class/leds/blue:cloud"
	;;
*)
	exit 0
	;;
esac

# The LED may be absent on a board variant that shares a board_name.
[ -d "$LED_PATH" ] || exit 0

# Respect the global LED kill switch, as ucentral-state does for led-running.
[ "$(uci -q get system.@system[-1].leds_off)" = "1" ] && set -- off

echo none > "$LED_PATH/trigger"

case "$1" in
on)
	cat "$LED_PATH/max_brightness" > "$LED_PATH/brightness"
	;;
off|*)
	echo 0 > "$LED_PATH/brightness"
	;;
esac
