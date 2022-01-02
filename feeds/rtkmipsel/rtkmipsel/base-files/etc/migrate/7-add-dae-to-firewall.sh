#!/bin/sh

. /lib/functions.sh

check_include() {
	local path

	config_get path $1 path
	if [ "$path" == "/etc/firewall.dae" ]; then
		dae_include=1
	fi
}

update_firewall_rules() {
	local dae_include

	config_load firewall
	config_foreach check_include include

	if [ "$dae_include" != "1" ]; then
		uci batch <<-EOF
		add firewall include
		set firewall.@include[-1].path=/etc/firewall.dae
		set firewall.@include[-1].reload=1
		EOF
	fi

	uci_commit firewall

}

update_firewall_rules
