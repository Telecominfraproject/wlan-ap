#!/bin/sh

case $2 in
AP-STA-CONNECTED)
	[ $4 = 0 -o $5 = 0 ] && {
		ubus call ratelimit client_set '{"device": "'$1'", "address": "'$3'", "defaults": "'$(ubus call wifi iface | jsonfilter -e "@['$1'].ssid")'" }'
		logger ratelimit addclient $1 $3 $ssid
		return
	}
	ubus call ratelimit client_set '{"device": "'$1'", "address": "'$3'", "rate_ingress": "'$4'mbit", "rate_egress": "'$5'mbit" }'
	logger ratelimit addclient $1 $3 $4 $5
	;;
AP-STA-DISCONNECTED)
	ubus call ratelimit client_delete '{ "address": "'$3'" }'
	logger ratelimit delclient $3
	;;
esac
