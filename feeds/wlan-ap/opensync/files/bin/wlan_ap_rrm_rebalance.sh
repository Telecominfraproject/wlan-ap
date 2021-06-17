#!/bin/sh
filename="/tmp/changes.txt"
radio=$(head -n 1 $filename | sed -r 's/phy+/radio/g')
interface=$(head -n 1 $filename | sed -r 's/phy+/wlan/g')
band=$(tail -n 1 $filename)

chan=$(uci get wireless.${radio}.channel)
echo "$radio  $chan $band $interface"

jstr='{"freq_band": "'"$band"'", "channel": '"$chan"'}'
echo $jstr
ubus call osync-rrm rrm_rebalance_channel '$jstr'
