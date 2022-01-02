[ -n "$(uci -q show snmpd)" ] \
&& [ -n "$(uci -q show mini_snmpd)" ] \
&& {
	community="$(uci -q get snmpd.public.community)"
	location="$(uci -q get snmpd.@system[0].sysLocation)"
	contact="$(uci -q get snmpd.@system[0].sysContact)"
	uci set mini_snmpd.@mini_snmpd[0].community="$community"
	uci set mini_snmpd.@mini_snmpd[0].location="$location"
	uci set mini_snmpd.@mini_snmpd[0].contact="$contact"
	#firewall rule is in another config, it won't be affected
	uci commit mini_snmpd
	rm /etc/config/snmpd
}

