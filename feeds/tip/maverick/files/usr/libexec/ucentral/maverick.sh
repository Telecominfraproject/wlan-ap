#!/bin/sh

active=$(ubus call ucentral status | jsonfilter -e '@.active')

[ -n "$active" -a ! "$active" -eq 1 ] && {
	logger maverick: all good
	exit 0
}

logger maverick: entering failsafe

rm /etc/config/network /etc/config/wireless
cp /rom/etc/config/uhttpd /rom/etc/config/firewall /rom/etc/config/dhcp /rom/etc/config/dropbear /etc/config
config_generate
wifi config

. /lib/functions.sh

radio_enable() { 
	uci set wireless.$1.disabled=0 
} 

ssid_set() { 
	uci set wireless.$1.ssid='Maverick' 
}

delete_forwarding() {
        uci delete firewall.$1
}

config_load wireless
config_foreach radio_enable wifi-device
config_foreach ssid_set wifi-iface
config_load firewall
config_foreach delete_forwarding forwarding

[ -z "$(uci get network.lan)" ] && {
	# single port devices wont bring up a lan interface by default
	uci set network.lan=interface
	uci set network.lan.type=bridge
	uci set network.lan.proto=static
	uci set network.lan.ipaddr=192.168.1.1
	uci set network.lan.netmask=255.255.255.0
}

uci commit

/etc/init.d/uhttpd start

reload_config
