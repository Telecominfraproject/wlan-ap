#!/bin/sh

#Turnoff blinking of AP's LED
/usr/opensync/tools/ovsh insert Node_Config module:="led" key:="led_off" value:="off"

