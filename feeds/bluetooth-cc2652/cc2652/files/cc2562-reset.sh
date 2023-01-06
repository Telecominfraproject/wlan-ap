#!/bin/sh

echo 1 > /sys/class/gpio/ble_enable/value
echo 1 > /sys/class/gpio/ble_backdoor/value
echo 1 > /sys/class/gpio/ble_reset/value
sleep 1
echo 1 > /sys/class/gpio/ble_reset/value
sleep 1
