#!/bin/sh

WD_ENABLE_GPIO=$1
WD_SET_GPIO=$2
TIMER=$3

disable_watchdog() {
	echo 1 > /sys/class/gpio/gpio${WD_ENABLE_GPIO}/value
}

trap disable_watchdog INT TERM

while true; do
	echo 1 > /sys/class/gpio/gpio${WD_SET_GPIO}/value
	sleep ${TIMER}
	echo 0 > /sys/class/gpio/gpio${WD_SET_GPIO}/value
	sleep ${TIMER}
done
