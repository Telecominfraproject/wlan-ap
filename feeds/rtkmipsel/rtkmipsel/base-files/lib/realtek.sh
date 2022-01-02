#!/bin/sh

realtek_board_detect() {
	[ -e /tmp/sysinfo/ ] || mkdir /tmp/sysinfo/
	echo "sp-w2m-ac1200" > /tmp/sysinfo/board_name
	echo "IgniteNet SP-W2M-AC1200" > /tmp/sysinfo/model
}

realtek_board_name() {
	local name

	[ -f /tmp/sysinfo/board_name ] && name=$(cat /tmp/sysinfo/board_name)
	[ -z "$name" ] && name="unknown"

	echo "$name"
}
