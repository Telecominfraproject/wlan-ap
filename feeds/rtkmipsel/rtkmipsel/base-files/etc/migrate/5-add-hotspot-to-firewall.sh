#!/bin/sh

. /lib/functions.sh

check_interface() {
	if [ "$1" == "hotspot_tunnel" ]; then
		hstunnel=1
	fi
}

check_zone() {
	local name

	config_get name $1 name
	if [ "$name" == "hotspot" ]; then
		hszone=1
	fi
}

update_include() {
	local path

	config_get path $1 path
	if [ "$path" == "/etc/firewall.chilli" ]; then
		uci_set firewall $1 reload 1
		uci_set firewall $1 path /etc/firewall.hotspot
	fi
}

add_tunnel_interface() {
	local hstunnel

	config_load network
	config_foreach check_interface interface

	if [ "$hstunnel" ]; then
		return
	fi

	uci_add network interface hotspot_tunnel
	uci_set network hotspot_tunnel ifname tun0
	uci_set network hotspot_tunnel proto none
	uci_commit network
}

add_firewall_zone() {
	local hszone

	config_load firewall
	config_foreach check_zone zone

	if [ "$hszone" ]; then
		return
	fi

	uci batch <<-EOF
	add firewall zone
	set firewall.@zone[-1].name=hotspot
	add_list firewall.@zone[-1].network=hotspot
	add_list firewall.@zone[-1].network=hotspot_tunnel
	set firewall.@zone[-1].input=ACCEPT
	set firewall.@zone[-1].output=ACCEPT
	set firewall.@zone[-1].forward=ACCEPT
	EOF

	uci batch <<-EOF
	add firewall forwarding
	set firewall.@forwarding[-1].src=hotspot
	set firewall.@forwarding[-1].dest=wan
	EOF

	config_foreach update_include include
	uci_commit firewall
}

add_tunnel_interface
add_firewall_zone
