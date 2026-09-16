#!/bin/sh
# tisbl.sh $tty $baudrate $tichip $firmware $resetpin $backdoorpin [$sw_rstpin]
# tisbl.sh /dev/ttyMSM1 115200 2652 /etc/tifirmware/blinky_bd13.bin 79 67

DEF_DIR="/etc/default"
LOG_DIR="/var/log"
RUN_DIR="/var/run"
SYSINFO_DIR="/var/sysinfo"
GPIO_DIR="/sys/class/gpio"

DEV_VENDOR="$(cat $SYSINFO_DIR/board_name | awk -F, '{print $1}')"
DEV_MODEL="$(cat $SYSINFO_DIR/board_name | awk -F, '{print $2}')"

#assumption: resetpin and backdoorpin are low active
tty=$1 
baudrate=$2
tichip=$3
firmware=$4
resetpin=$5
backdoorpin=$6
sw_rstpin=$7

if [ -n "${sw_rstpin}" ]; then
	if [ ! -e ${GPIO_DIR}/gpio${sw_rstpin} ]; then
		echo ${sw_rstpin} > ${GPIO_DIR}/export
	fi

	echo "out" > ${GPIO_DIR}/gpio${sw_rstpin}/direction
	echo 1 > ${GPIO_DIR}/gpio${sw_rstpin}/value
fi

ti_reset() #assumption:resetpin is low active 
{
	if [ ! -e ${GPIO_DIR}/gpio${resetpin} ]; then
		echo ${resetpin} > ${GPIO_DIR}/export
	fi

	echo out > ${GPIO_DIR}/gpio${resetpin}/direction

	echo 1 > ${GPIO_DIR}/gpio${resetpin}/value
	echo 0 > ${GPIO_DIR}/gpio${resetpin}/value
	sleep 1
	echo 1 > ${GPIO_DIR}/gpio${resetpin}/value
}

ti_goto_bootloader() #assumption:backdoorpin is low active 
{
	if [ ! -e ${GPIO_DIR}/gpio${backdoorpin} ]; then
		echo ${backdoorpin} > ${GPIO_DIR}/export
	fi

	echo out > ${GPIO_DIR}/gpio${backdoorpin}/direction

	echo 0 > ${GPIO_DIR}/gpio${backdoorpin}/value
	ti_reset
	sleep 1
	echo 1 > ${GPIO_DIR}/gpio${backdoorpin}/value
}

exec 100>"/var/lock/ble.lock"
flock 100

if [ -e "$firmware" ]; then 
	ti_goto_bootloader

	tisbl "$tty" "$baudrate" "$tichip" "$firmware" #try to upgrade firmware
fi
