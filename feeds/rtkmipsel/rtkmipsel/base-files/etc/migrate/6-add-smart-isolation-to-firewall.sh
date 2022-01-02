#!/bin/sh

. /lib/functions.sh

check_interface() {
	local smi

	if [ "$1" == "hotspot" ]; then
		config_get smi $1 smart_isolation
		if [ -z "$smi" ]; then
			uci_set network $1 smart_isolation 0
		fi
	fi
}

delete_forwarding() {
	uci_remove firewall $1
}

check_include() {
	local path

	config_get path $1 path
	if [ "$path" == "/etc/firewall.smart_isolation" ]; then
		smi_include=1
	fi
}

add_hotspot_policy() {
	config_load network
	config_foreach check_interface interface
	uci_commit network
}

update_firewall_rules() {
	local smi_include

	config_load firewall
	config_foreach delete_forwarding forwarding

	config_foreach check_include include
	if [ "$smi_include" != "1" ]; then
		uci batch <<-EOF
		add firewall include
		set firewall.@include[-1].path=/etc/firewall.smart_isolation
		set firewall.@include[-1].reload=1
		EOF
	fi

	uci_commit firewall
}

add_hotspot_policy
update_firewall_rules
