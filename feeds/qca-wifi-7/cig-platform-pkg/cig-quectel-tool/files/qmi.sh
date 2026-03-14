#!/bin/sh

[ -n "$INCLUDE_ONLY" ] || {
	. /lib/functions.sh
	. ../netifd-proto.sh
	init_proto "$@"
}

proto_qmi_init_config() {
	available=1
	no_device=1
	proto_config_add_string "device:device"
	proto_config_add_string apn
	proto_config_add_string auth
	proto_config_add_string username
	proto_config_add_string password
	proto_config_add_string pincode
	proto_config_add_string pdptype
	proto_config_add_boolean delegate
	proto_config_add_int v6profile
	proto_config_add_int mtu
	proto_config_add_defaults
}

proto_qmi_setup() {
	local interface="$1"

	local apn auth password pdptype pincode username v6profile
	json_get_vars apn auth password pdptype pincode username v6profile

	ip link set rmnet_mhi0.1 down
	ip link set rmnet_mhi0.1 name $interface

	cmd="quectel-CM -o $interface -s $apn"

	[ -n "$username" ] && cmd="$cmd $username"
	[ -n "$password" ] && cmd="$cmd $password"
	[ -n "$auth" -a "x$auth" != "xnone" ] && cmd="$cmd $auth"
	[ -n "$pincode" ] && cmd="$cmd -p $pincode"
	[ -n "$v6profile" ] && cmd="$cmd -6"

	if [ "x$(ps | grep quectel | grep -v grep)" = "x" ]; then
		proto_export "INTERFACE=$interface"
		proto_run_command $interface $cmd
		proto_block_restart $interface
	fi
}

proto_qmi_teardown() {
	local pid=$(ps | grep quectel | grep -v grep | awk -F ' ' '{print $1}')

	proto_kill_command $interface
	for p in $pid
	do
		kill $p
	done

	ip link set $interface down
	ip link set $interface name rmnet_mhi0.1
}

[ -n "$INCLUDE_ONLY" ] || {
	add_protocol qmi
}
