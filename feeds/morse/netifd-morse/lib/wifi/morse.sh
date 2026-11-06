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

		# Determine the sysfs device path for this morse phy.
		#
		# ucentral needs radio.path for BOTH capabilities generation
		# (wifi/phy.uc lookup_paths() only maps wifi-devices that have a
		# `path`) AND apply-time binding (wiphy.uc path_to_section() matches
		# s.path == path). Without it the HaLow radio never reaches
		# capabilities.json (cloud rejects "band HaLow") and its SSID never
		# renders.
		#
		# On this platform `iwinfo dot11ah/nl80211 path` returns nothing for
		# the USB morse dongle (iwinfo has no working path backend for it),
		# so we derive the path exactly the way OpenWrt stores it for the
		# PCIe radios: realpath of the phy's device node with the leading
		# "/sys/devices/platform/" (or "/sys/devices/") prefix stripped
		# (mt7996 -> soc/11280000.pcie/...; morse USB -> soc/11200000.usb/...).
		# macaddr is kept too: netifd's drv_morse_setup find_phy() matches on it.
		path="$(iwinfo dot11ah path "$dev" 2>/dev/null)"
		[ -n "$path" ] || path="$(iwinfo nl80211 path "$dev" 2>/dev/null)"
		[ -n "$path" ] || path="$(readlink -f "/sys/class/ieee80211/${dev}/device" 2>/dev/null | sed 's#^/sys/devices/platform/##; s#^/sys/devices/##')"

		uci -q batch <<-EOF
			set wireless.radio${devidx}=wifi-device
			set wireless.radio${devidx}.type=morse
			set wireless.radio${devidx}.path='$path'
			set wireless.radio${devidx}.macaddr=$(cat /sys/class/ieee80211/${dev}/macaddress)
			set wireless.radio${devidx}.band=s1g
			set wireless.radio${devidx}.hwmode=11ah
			set wireless.radio${devidx}.reconf=0
			set wireless.radio${devidx}.disabled=0

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
