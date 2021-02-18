#!/bin/sh

config=$1

echo config:$config

[ -f "$config" ] || {
	logger "ucentral_apply: invalid paramters"
	exit 1
}

ucode -m ubus -m uci -m fs -E capab=/etc/ucentral/capabilities.json -E cfg=$1 -i /usr/share/ucentral/ucentral.uc > /tmp/ucentral.uci

[ $? -eq 0 ] || {
	logger "ucentral_apply: applying $1 failed"
	exit 1
}

active=$(readlink /etc/ucentral/ucentral.active)
[ -n "$active" -a -f "$active" ] && {
	rm -f /etc/ucentral/ucentral.old
	ln -s $active /etc/ucentral/ucentral.old
}

rm -f /etc/ucentral/ucentral.active
ln -s $config /etc/ucentral/ucentral.active

rm -rf /tmp/config-shadow
cp -r /etc/config-shadow /tmp
cat /tmp/ucentral.uci | uci -c /tmp/config-shadow batch 2> /dev/null
uci -c /tmp/config-shadow commit

cp /tmp/config-shadow/* /etc/config/

reload_config

rm -rf /tmp/config-shadow

return 0
