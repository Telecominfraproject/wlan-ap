#!/bin/sh
while [ 1 ]; do
    echo 1 > /sys/class/leds/hwwatchdog/brightness
	sleep 1
    echo  0 > /sys/class/leds/hwwatchdog/brightness
	sleep 1
done

