#!/bin/sh

cmd=$1
id=$2

[ -f "$cmd" -a -n "$id" ] || {
	logger "ucentral_cmd: invalid paramters"
	exit 1
}

ucode -m uci -m fs -m ubus -E capab=/etc/ucentral/capabilities.json -E cmd=$cmd -e "{\"id\":$id}" -i /usr/share/ucentral/cmd.uc > /tmp/ucentral.cmd

[ $? -eq 0 ] || {
	logger "ucentral_cmd: executing $cmd failed"
	exit 1
}

return 0
