#!/bin/sh

. /etc/diag.sh
set_state upgrade
sleep 60
status_led_off
set_state done
