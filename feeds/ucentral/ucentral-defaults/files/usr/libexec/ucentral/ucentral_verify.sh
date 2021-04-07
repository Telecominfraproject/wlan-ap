#!/bin/sh

config=$1
uuid=$2

[ -f "$config" -a -n "$uuid" ] || {
	logger "ucentral_verify: invalid paramters"
	exit 1
}

jsonschema $1 /usr/share/ucentral/ucentral.schema.json > /tmp/ucentral.verify

[ $? -eq 0 ] || {
	ubus call ucentral log "{\"error\": \"schema failed\", \"uuid\": $uuid}"	
	logger "ucentral_verify: schema failed"
	exit 1
}

cp $config /etc/ucentral/ucentral.cfg.$uuid

return 0
