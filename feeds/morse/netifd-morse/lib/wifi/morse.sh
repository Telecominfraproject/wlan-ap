#!/bin/sh

# This script is copied and modified from
# package/kernel/mac80211/files/lib/wifi/mac80211.sh
# and modified (simplified) to work with morse devices.

append DRIVERS "morse"

# Find and set $phy for this $device (a wifi-device section name)
lookup_phy() {
	[ -n "$phy" ] && {
		[ -d /sys/class/ieee80211/$phy ] && return
	}

	local devpath
	config_get devpath "$device" path
	[ -n "$devpath" ] && {
		phy="$(iwinfo dot11ah phyname "path=$devpath")"
		[ -n "$phy" ] && return
	}

	local macaddr="$(config_get "$device" macaddr | tr 'A-Z' 'a-z')"
	[ -n "$macaddr" ] && {
		for _phy in /sys/class/ieee80211/*; do
			[ -e "$_phy" ] || continue

			[ "$macaddr" = "$(cat ${_phy}/macaddress)" ] || continue
			phy="${_phy##*/}"
			return
		done
	}
	phy=
	return
}

# Find and save the phy and macaddr for this $device (a wifi-device section name)
find_morse_phy() {
	local device="$1"

	config_get phy "$device" phy
	lookup_phy
	[ -n "$phy" -a -d "/sys/class/ieee80211/$phy" ] || {
		echo "PHY for wifi device $1 not found"
		return 1
	}
	config_set "$device" phy "$phy"

	config_get macaddr "$device" macaddr
	[ -z "$macaddr" ] && {
		config_set "$device" macaddr "$(cat /sys/class/ieee80211/${phy}/macaddress)"
	}

	return 0
}

# Set found=1 if the $phy for this $device (a wifi-device section name) is the same as $dev
check_morse_device() {
	config_get phy "$1" phy
	[ -z "$phy" ] && {
		find_morse_phy "$1" >/dev/null || return 0
		config_get phy "$1" phy
	}
	[ "$phy" = "$dev" ] && found=1
}

detect_morse() {
	devidx=0
	config_load wireless
	while :; do
		config_get type "radio$devidx" type
		[ -n "$type" ] || break
		devidx=$(($devidx + 1))
	done

	for _dev in /sys/class/ieee80211/*; do
		[ -e "$_dev" ] || continue

		# Only configure morse devices.
		basename "$(readlink -f "$_dev/device/driver/")" | grep '^morse_' || continue

		dev="${_dev##*/}"

		# Skip already configured devices.
		# The path or macaddr are used to find the corresponding phy.
		found=0
		config_foreach check_morse_device wifi-device
		[ "$found" -gt 0 ] && continue

		path="$(iwinfo dot11ah path "$dev")"
		if [ -n "$path" ]; then
			dev_id="set wireless.radio${devidx}.path='$path'"
		else
			dev_id="set wireless.radio${devidx}.macaddr=$(cat /sys/class/ieee80211/${dev}/macaddress)"
		fi

		uci -q batch <<-EOF
			set wireless.radio${devidx}=wifi-device
			set wireless.radio${devidx}.type=morse
			${dev_id}
			set wireless.radio${devidx}.band=s1g
			set wireless.radio${devidx}.hwmode=11ah
			set wireless.radio${devidx}.reconf=0
			set wireless.radio${devidx}.disabled=1

			set wireless.default_radio${devidx}=wifi-iface
			set wireless.default_radio${devidx}.mode=ap
			set wireless.default_radio${devidx}.wds=1
			set wireless.default_radio${devidx}.device=radio${devidx}
			set wireless.default_radio${devidx}.network=lan
			set wireless.default_radio${devidx}.ssid=MorseMicro
			set wireless.default_radio${devidx}.encryption=sae
			set wireless.default_radio${devidx}.key=12345678
EOF

		board=$(board_name)

		case "$board" in
			morse,ekh01-03 |\
			morse,ekh03v3)
				bcf=bcf_mf08551.bin
			;;
			morse,ekh01v1)
				bcf=bcf_mf03120.bin
			;;
			morse,ekh01v2)
				bcf=bcf_mf08251.bin
			;;
			morse,ekh04v4)
				bcf=bcf_ekh04_v4.bin
			;;
		esac

		[ -n "${bcf}" ] && uci -q set wireless.radio${devidx}.bcf="${bcf}"

		uci -q commit wireless

		devidx=$(($devidx + 1))
	done
}
