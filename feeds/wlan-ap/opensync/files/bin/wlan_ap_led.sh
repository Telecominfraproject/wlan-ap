#!/bin/sh

#Blink AP's LED
/usr/opensync/tools/ovsh insert Node_Config module:="led" key:="led_blink" value:="on"

#Turnoff AP's LED
/usr/opensync/tools/ovsh insert Node_Config module:="led" key:="led_off" value:="off"
