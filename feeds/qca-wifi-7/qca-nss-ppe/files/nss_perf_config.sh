#!/bin/sh
#
# Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

#
# usage:
# apply perf config for all supported link type
#	./lib/nss_perf_config.sh
# apply perf config for specific link type
#	./lib/nss_perf_config.sh gre
#

num_args=$#
input=$@

link_show="ip link show type"

#add the supported link types
link_type_list="gre"

Help() {
	echo "supported types: $link_type_list"
}

validate_input() {

	[[ "$input" == "all" ]] && return

	for i in $input
	do
		if ! echo "$link_type_list" | grep -q "$i"; then
			Help
			exit 1
		fi
	done
}

#get the net devices based on type
get_devices() {
	local device
	local type="$1"
	device=`$link_show $type | awk '/^[0-9]/' | awk '{print $2}' | awk -F '@' '{print $1}'`
	eval "$2='$device'"
}

#disable GRO
gro_off() {
	# add the type of tunnel to turn off GRO/GSO
	local dev=''
	local dev_list=''

	#add the devices which are to be excluded from GRO off
	local fallback_dev="gre0 gretap0"
	get_devices 'gretap' dev
	dev_list="$dev_list $dev"

	get_devices 'gre' dev
	dev_list="$dev_list $dev"

	for dev in $dev_list; do
		if ! echo "$fallback_dev" | grep -q $dev; then
			ethtool -K $dev gro off
		fi
	done
}

#add GRETUN specific configuration
enable_perf_config_gretun() {
	#disable GRO
	gro_off
}

enable_perf_config_default() {
	enable_perf_config_gretun
}

enable_perf_config() {
	#if no tunnel type passed then apply config for all
	[ $num_args -lt 1 ] && enable_perf_config_default

	for i in $input
	do
		case $i in
		gre)
			enable_perf_config_gretun
		;;
		esac
	done
}

validate_input

enable_perf_config
