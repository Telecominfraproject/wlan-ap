#!/bin/sh

timeout=$1

. /etc/diag.sh
set_state upgrade
sleep $1
status_led_off
set_state done
exit 0
