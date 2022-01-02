#!/bin/sh
killall -9 ser2net
tisbl.sh /dev/ttyMSM1 115200 2652 /etc/tifirmware/ble5_host_test_bd9.bin 79 34
echo "7000:telnet:0:/dev/ttyMSM1:115200 8DATABITS NONE 1STOPBIT remctl" > /tmp/ser2net.conf
ser2net -c /tmp/ser2net.conf
