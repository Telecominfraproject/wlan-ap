#!/bin/sh

timeout=$1

/etc/init.d/led blink
sleep $1
/etc/init.d/led restart
exit 0
