#!/bin/sh
. /usr/share/libubox/jshn.sh

json_init
json_load "$(cat /etc/board.json)"
json_select network
	json_select "wan"
		json_get_vars ifname
	json_select ..
json_select ..

[ -n "$ifname" ] || {
	ifname=$(uci get network.wan.ifname)
	ifname=${ifname%% *}
}


if="$ifname"
[ "$(cat /sys/class/net/"${if}"/carrier)" = 0 ] && {
	return 0
}
return 1
