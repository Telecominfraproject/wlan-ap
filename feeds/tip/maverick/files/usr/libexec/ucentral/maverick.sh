#!/bin/sh

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

config_load wireless
config_foreach radio_enable wifi-device
config_foreach ssid_set wifi-iface
uci commit

/etc/init.d/ucentral stop

reload_config
