#!/bin/sh
[ -f /tmp/run/hostapd-cli-$1.pid ] && kill "$(cat /tmp/run/hostapd-cli-$1.pid)"
ubus -t 120 wait_for hostapd.$1
[ $? = 0 ] && hostapd_cli -a /usr/libexec/ratelimit.sh -i $1 -P /tmp/run/hostapd-cli-$1.pid -B
