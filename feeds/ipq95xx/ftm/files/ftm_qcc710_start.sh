#!/bin/sh
#
# Copyright (c) 2021 Qualcomm Technologies, Inc.
#
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.
#
#

# QCC710 v1.0 reset for BT bringup
qcc710_reset() {
	reset_gpio_pin=$(cat /proc/device-tree/soc/pinctrl@1000000/QCC710_pins/QCC710_reset/pins | sed s/"gpio"//)
	[[ -z $reset_gpio_pin ]] && return
	gpio_base=$(cat /sys/class/gpio/gpiochip*/base | head -n1)
	gpio_reset=$(( gpio_base + reset_gpio_pin ))
	if [[ ! -e /sys/class/gpio/gpio$gpio_reset ]]; then
		[ -z ${SLEEP} ] && echo -e "Enter sleep value for reset. Options:\n10 \n1" && read -p "Enter : "  SLEEP
		[ -z ${SLEEP} ] && SLEEP=10
		echo $gpio_reset > /sys/class/gpio/export
		echo out > /sys/class/gpio/gpio$gpio_reset/direction
		echo "Performing QCC710 reset ...." > /dev/console
		{ echo 1 > /sys/class/gpio/gpio$gpio_reset/value ; \
			sleep $SLEEP; \
			echo 0 > /sys/class/gpio/gpio$gpio_reset/value; \
			echo "QCC710 reset complete ...." > /dev/console; }
	fi
}

while [ -n "$1" ]; do
	case "$1" in
		-h|--help) HELP=1; break;;
		-a|--ipaddr) SERVERIP="$2";shift;;
		-s|--sleep) SLEEP="$2";shift;;
		-r|--baud-rate) BAUDRATE="$2";shift;;
		-*)
			echo "Invalid option: $1"
			ERROR=1;
			break
		;;
		*)break;;
	esac
	shift
done
[ -n "$HELP" -o -n "$ERROR" ] && {
	        cat <<EOF
Usage: $0 [-h] [-a SERVERIP] [-r baud-rate] [-s sleep]
ftm_qcc710_start options:
	-h	print this help
	-a	ipaddr of the server for diag connection
	-r	baudrate
	-s	sleep

Example:
ftm_qcc710_start -a <serverip> -r <baud-rate> -s <sleep>

version 1 : ./sbin/ftm_qcc710_start -a 192.168.1.121 -r 2000000 -s 10
version 2 : ./sbin/ftm_qcc710_start -a 192.168.1.121 -r 115200 -s 1
EOF
	# If we requested the help flag, then exit normally.
	# Else, it's probably an error so report it as such.
	[ -n "$HELP" ] && exit 0
	exit 1
}

[ -z ${SERVERIP} ] && SERVERIP=$(grep -oh "serverip.*#"  /proc/cmdline  | awk -F '#' '{print $2}')
[ -z ${SERVERIP} ] && read -p "No serverip in cmdline, please enter the serverip : "  SERVERIP
[ -z ${BAUDRATE} ] && echo -e "Enter baudrate for stack bringup. Options:\n2000000\n115200" && read -p "Enter : "  BAUDRATE
[ -z ${BAUDRATE} ] && BAUDRATE=2000000
qcc710_reset
DIAG_PID=$(ps | grep diag_socket_app | grep -v grep | awk '{print $1}')
while [ -n "$DIAG_PID" ]
do
    kill -s SIGTERM $DIAG_PID
    DIAG_PID=$(ps | grep diag_socket_app | grep -v grep | awk '{print $1}')
done
echo "Stopped previous instances of diag_socket_app process"
[ -z "$DIAG_PID" ] && /usr/sbin/diag_socket_app -a $SERVERIP -p 2500 &

FTM_PID=$(ps | grep "ftm " | grep -v grep | awk '{print $1}')
while [ -n "$FTM_PID" ]
do
    kill -s SIGTERM $FTM_PID
    FTM_PID=$(ps | grep "ftm " | grep -v grep | awk '{print $1}')
done
echo "Stopped previous instances ftm process"
[ -z "$FTM_PID" ] && /usr/sbin/ftm -n -dd -r $BAUDRATE
