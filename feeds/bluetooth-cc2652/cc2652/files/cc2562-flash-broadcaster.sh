#!/bin/sh

if [ -f /sys/class/gpio/ble_enable/value ]; then
	echo 1 > /sys/class/gpio/ble_enable/value
fi
echo 0 > /sys/class/gpio/ble_backdoor/value
echo 1 > /sys/class/gpio/ble_reset/value
echo 0 > /sys/class/gpio/ble_reset/value
sleep 1
echo 1 > /sys/class/gpio/ble_reset/value
sleep 1
echo 1 > /sys/class/gpio/ble_backdoor/value
tisbl /dev/ttyMSM1 115200 2652 /lib/firmware/cc2562/simple_broadcaster_bd9.bin 
