# SPDX-License-Identifier: BSD-3-Clause
#!/bin/sh
. /lib/functions.sh
. /lib/nft-qos/core.sh

# append rule for ssid_ratelimit qos
qosdef_append_rule_ssid() { # <section> <operator> <default-unit> <default-rate>
	local unit rate iface rlimit
	local operator=$2

	iface=$1

	config_get disabled $1 disabled
	if [ "$disabled" == "1" ]; then
		return
	fi

	config_get rlimit $1 rlimit
	if [ -z "$rlimit" -o "$rlimit" == "0" ]; then
		return
	fi

	config_get unit $1 unit $3
	if [ $operator == "oif" ]; then
		config_get rate $1 drate $4
	fi

	if [ $operator == "iif" ]; then
		config_get rate $1 urate $4
	fi

	if [ -z "$iface" -o -z "$rate" ]; then
		logger -t "nft-qos" "Error: No interface $iface or rate $rate present"
		return
	fi

	state=`cat /sys/class/net/$iface/operstate`
	if [ -f "/sys/class/net/$iface/carrier" ]; then
		logger -t "nft-qos" "$iface carrier exists"
	fi
	logger -t "nft-qos" "$iface state=$state"

	logger -t "nft-qos" "Add rule $iface $operator $unit $rate"
	qosdef_append_rule_iface_limit $iface $operator $unit $rate

	maclist=`iwinfo $iface assoclist | grep dBm | cut -f 1 -s -d" "`

	for mac in $maclist
	do
		logger -t "nft-qos" "Add $mac"
		/lib/nft-qos/mac-rate.sh add $iface $mac
	done
}

# append chain for static qos
qosdef_append_chain_ssid() { # <hook> <name> <section> <unit> <rate>
	local hook=$1 name=$2
	local config=$3 operator

	case "$name" in
		download) operator="oif";;
		upload) operator="iif";;
	esac

	uci_load wireless
	qosdef_appendx "\tchain $name {\n"
	qosdef_append_chain_def filter $hook 0 accept
	qosdef_append_rule_limit_whitelist $name
	config_foreach qosdef_append_rule_ssid $config $operator $4 $5
	qosdef_appendx "\t}\n"
}

qosdef_flush_ssid_ratelimit() {
	logger -t "nft-qos" "flush"
	qosdef_flush_table "$NFT_QOS_BRIDGE_FAMILY" nft-qos-ssid-lan-bridge
}

# ssid ratelimit init
qosdef_init_ssid_ratelimit() {
	exec 500>/tmp/rlimit.lock
	flock 500
	local hook_ul="input" hook_dl="output"
	logger -t "nft-qos" "`date -I'seconds'` "INIT" $0"
	qosdef_appendx "table $NFT_QOS_BRIDGE_FAMILY nft-qos-ssid-lan-bridge {\n"
	qosdef_append_chain_ssid $hook_ul upload wifi-iface $ssid_ratelimit_unit_ul $ssid_ratelimit_rate_ul
	qosdef_append_chain_ssid $hook_dl download wifi-iface $ssid_ratelimit_unit_dl $ssid_ratelimit_rate_dl
	qosdef_appendx "}\n"
	exec 500>&-
}
