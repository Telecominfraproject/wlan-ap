#!/bin/sh

# This script is copied and modified from
# package/kernel/mac80211/files/lib/wifi/mac80211.sh
# and modified (simplified) to work with morse devices.

append DRIVERS "morse"

# Set found=1 and the morse_device if the $device (a wifi-device section name) type is morse
find_morse_device() {
	local device="$1"
	local type

	config_get type "$device" type
	[ "$type" != "morse" ] && return 0

	found=1
	morse_device=$device
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

		local path="$(iwinfo nl80211 path "$dev")"
		local macaddr="$(cat /sys/class/ieee80211/${dev}/macaddress)"
		local board_type="$(cat /sys/class/ieee80211/${dev}/device/board_type)"

		# Skip if neither path nor macaddr is available
		if [ -z "$path" ] && [ -z "$macaddr" ]; then
			logger -p 3 -t wifi-morse "Ignoring $dev: unable to find sysfs path or macaddr"
			continue
		fi

		# Assumes there is only one Morse device in the system.
		# If a Morse device already exists, update its path to match the current device.
		# Skip creating a new configuration or applying defaults when one is already present.

		found=0
		config_foreach find_morse_device wifi-device

		if [ "$found" -gt 0 ]; then
			# Verify whether the existing Morse device's sysfs path still exists in the system.
			# If it does, avoid updating the path because having two sysfs entries for the same Morse device
			# indicates an invalid or inconsistent system state.
			config_get devpath ${morse_device} path
			phy="$(iwinfo nl80211 phyname "path=$devpath")"
			if [ -n "$phy" ] && [ "$phy" != "$dev" ]; then
				logger -p 3 -t wifi-morse "Ignoring $dev, as Morse device $phy already exists."
				continue
			fi

			if [ -n "$path" ]; then
				uci set wireless.${morse_device}.path=$path
			else
				uci set wireless.${morse_device}.macaddr=$macaddr
			fi
			uci -q commit wireless
			continue
		else
			if [ -n "$path" ]; then
				dev_id="set wireless.radio${devidx}.path='$path'"
			else
				dev_id="set wireless.radio${devidx}.macaddr=$macaddr"
			fi
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

		# board_type is 'we have OTP bits set', in which case it should
		# automatically load the correct file (bcf_boardtype...) and
		# we don't need to override.
		# We force HaLowLink 1 since 4v3 support currently requires
		# an explicit BCF file, though currently the AZW modules
		# do not have OTP bits burnt.
		if [ "$board_type" -eq 0 ] || [ "$board" = morse,halowlink1 ] || [ "$board" = morse,halowlink2 ]; then
			case "$board" in
				morse,ekh04v6)
					bcf=bcf_ekh04_v6.bin
				;;
				morse,halowlink1)
					bcf=bcf_mm_hl1.bin
				;;
				morse,halowlink2)
					bcf=bcf_mm_hl2_ext.bin
				;;
				*)
					if [[ $path  = *usb* ]]; then
						bcf=bcf_mf15457.bin
					fi
				;;
			esac

			[ -n "${bcf}" ] && uci -q set wireless.radio${devidx}.bcf="${bcf}"
		fi

		uci -q commit wireless

		devidx=$(($devidx + 1))
	done
}
