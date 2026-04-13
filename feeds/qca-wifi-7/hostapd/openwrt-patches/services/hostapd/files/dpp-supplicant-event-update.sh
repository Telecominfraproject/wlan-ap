#!/bin/sh
#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#SPDX-License-Identifier: ISC
#

[ -e /lib/functions.sh ] && . /lib/functions.sh

ifname=$1
CMD=$2
CONFIG=$3
shift
shift
SSID=$@
PASS=$@

encryption=
sae_require_mfp=
ieee80211w=
dpp_akm=
sae_pwe=
i=0

get_section() {
	local config=$1
	local ifname
	local device
	local iface

	config_get ifname "$config" ifname
	if [ -n "$ifname" ]; then
		[ "${ifname}" = "$2" ] && eval "$3=$config"
	else
		local phy=${2:4:1}
		local band=${2:6:1}
		local index=${2:8:1}
		iface="radio${phy}_band${band}"

		config_get device "$config" device
		[ "$iface" = "$device" ]  && [ "${index}" = "$i" ] && eval "$3=$config"
		if [ "$index" -lt "$i" ];
		then
			i=$((i+1))
		fi
	fi
}

hex2string()
{
	I=0
	while [ $I -lt ${#1} ];
	do
		echo -en "\x""${1:$I:2}"
		let "I += 2"
	done
}

get_config_val() {
	local key=$1
	#TODO: if update_config permission issue fixed, then this location has to be changed
	local conf=/tmp/wpa_supplicant-$ifname.conf

	if [ ! -f "$conf" ]; then
		conf=/var/run/wpa_supplicant-$ifname.conf
	fi
	config_val=$(wpa_cli -i"$ifname" get_network 0 "$1" | cut -f 2 -d= | sed -e 's/^"\(.*\)"/\1/')
	if [ "$key" == 'psk' ]; then
		config_val=$(awk "BEGIN{FS=\"=\"} /[[:space:]]${key}=/ {print \$0}" "$conf" |grep "${key}=" |tail -n 1 | cut -f 2 -d= | sed -e 's/^"\(.*\)"/\1/')
	fi
	if [ "$key" == 'sae_pwe' ]; then
		config_val=$(wpa_cli -i"$ifname" get "$1")
	fi
	if [ "$config_val" == "FAIL" ]; then
		config_val=''
	fi
}

is_mld() {
        mld_group=$1
        mld_iface=$(uci get wireless."$1".ifname)
}

apply_mld_config() {
        local config=$1
        local mld=

        config_get mld "$config" mld
        if [[ "$mld" = "$mld_group" ]]; then
		update_wireless $config
        fi
}

update_wireless() {
	sect=$1
	get_config_val 'ssid'
	ssid=${config_val}

	get_config_val 'key_mgmt'
	key_mgmt=${config_val}

	get_config_val 'dpp_connector'
	dpp_connector=${config_val}

	get_config_val 'psk'
	psk=${config_val}

	get_config_val 'dpp_csign'
	dpp_csign=${config_val}

	get_config_val 'dpp_pp_key'
	dpp_pp_key=${config_val}

	get_config_val 'dpp_netaccesskey'
	dpp_netaccesskey=${config_val}

	get_config_val 'sae_pwe'
	sae_pwe=${config_val}

	. /sbin/wifi config

	uci set wireless.${sect}.ssid=$ssid
	[ -n "$mld_group" ] && uci set wireless.$mld_group.ssid=$ssid
	uci set wireless.${sect}.dpp_connector=$dpp_connector
	uci set wireless.${sect}.key=$psk
	[ -n "$mld_group" ] && uci set wireless.$mld_group.key=$psk
	[ -n "$sae_pwe" ] && uci set wireless.${sect}.sae_pwe=$sae_pwe
	[ -n "$mld_group" ] && uci set wireless.$mld_group.sae_pwe=$sae_pwe
	uci set wireless.${sect}.dpp_csign=$dpp_csign
	uci set wireless.${sect}.dpp_pp_key=$dpp_pp_key
	uci set wireless.${sect}.dpp_netaccesskey=$dpp_netaccesskey
	[ -n "$encryption" ] && uci set wireless.${sect}.encryption=$encryption
	[ -n "$mld_group" ] && uci set wireless.$mld_group.encryption=$encryption
	[ -n "$sae_require_mfp" ] && uci set wireless.${sect}.sae_require_mfp=$sae_require_mfp
	[ -n "$dpp_akm" ] && uci set wireless.${sect}.dpp_akm=$dpp_akm
	[ -n "$ieee80211w" ] && uci set wireless.${sect}.ieee80211w=$ieee80211w
	[ -n "$mld_group" ] && uci set wireless.$mld_group.ieee80211w=$ieee80211w
	uci commit wireless
}

case "$CMD" in
	DPP-CONF-RECEIVED)
		wpa_cli -i"$ifname" remove_network all
		wpa_cli -i"$ifname" add_network
		wpa_cli -i"$ifname" set_network 0 pairwise "CCMP"
		wpa_cli -i"$ifname" set_network 0 group "CCMP"
		wpa_cli -i"$ifname" set_network 0 proto "RSN"
		;;
	DPP-CONFOBJ-AKM)
		encryption=
		dpp_akm=
		sae_require_mfp=
		ieee80211w=
		key_mgmt=
		sae_pwe=
		case "$CONFIG" in
			dpp+psk+sae|dpp-psk-sae)
				key_mgmt="DPP SAE WPA-PSK"
				encryption="sae-mixed"
				dpp_akm=1
				ieee80211w=1
				sae_require_mfp=1
				;;
			dpp+sae|dpp-sae)
				key_mgmt="DPP SAE"
				encryption="sae"
				ieee80211w=2
				dpp_akm=1
				;;
			dpp)
				key_mgmt="DPP"
				encryption="dpp"
				ieee80211w=2
				dpp_akm=1
				;;
			sae)
				key_mgmt="SAE"
				encryption="sae"
				ieee80211w=2
				sae_pwe=2
				;;
			psk+sae|psk-sae)
				key_mgmt="SAE WPA-PSK"
				encryption="sae-mixed"
				ieee80211w=1
				sae_require_mfp=1
				sae_pwe=2
				;;
			psk)
				key_mgmt="WPA-PSK"
				encryption="psk2"
				ieee80211w=1
				;;
			sae-ext-key)
				key_mgmt="SAE-EXT-KEY"
				encryption="sae-ext-key"
				ieee80211w=2
				sae_pwe=2
				wpa_cli -i"$ifname" set_network 0 pairwise "GCMP-256"
				wpa_cli -i"$ifname" set_network 0 group "GCMP-256"
				;;
		esac
		wpa_cli -i"$ifname"  set_network 0 ieee80211w "$ieee80211w"
		wpa_cli -i"$ifname"  set_network 0 key_mgmt "$key_mgmt"
		wpa_cli -i"$ifname"  set_network 0 sae_require_mfp "$key_mgmt"
		wpa_cli -i"$ifname"  set sae_pwe "$sae_pwe"

		. /sbin/wifi config
		config_foreach is_mld wifi-mld
		if [ -n "$mld_iface" ]; then
			config_foreach apply_mld_config wifi-iface
		else
			sect=
			config_foreach get_section wifi-iface "$ifname" sect
			update_wireless $sect
		fi
		;;
	DPP-CONFOBJ-SSID)
		wpa_cli -i"$ifname"  set_network 0 ssid \""$SSID"\"
		;;
	DPP-CONNECTOR)
		wpa_cli -i"$ifname" set dpp_connector "$CONFIG"
		wpa_cli -i"$ifname" set_network 0 dpp_connector \""${CONFIG}"\"
		;;
	DPP-CONFOBJ-PASS)
		PASS_STR=$(hex2string "$PASS")

		wpa_cli -i"$ifname" set_network 0 psk \""${PASS_STR}"\"
		;;
	DPP-CONFOBJ-PSK)
		PASS_STR=$(hex2string "$CONFIG")
		get_pairwise

		wpa_cli -i"$ifname" set_network 0 psk "$PASS_STR"
		;;
	DPP-EVENT-SAE-PWE)
		wpa_cli -i"$ifname" set sae_pwe $CONFIG
		;;
	DPP-C-SIGN-KEY)
		wpa_cli -i"$ifname" set dpp_csign "$CONFIG"
		wpa_cli -i"$ifname" set_network 0 dpp_csign "$CONFIG"
		;;
        DPP-CONNECTOR-C-SIGN-KEY)
                wpa_cli -i"$ifname" set dpp_connector_csign "$CONFIG"
		wpa_cli -i"$ifname" set_network 0 dpp_connector_csign "$CONFIG"
                ;;
        DPP-PP-KEY)
                wpa_cli -i"$ifname" set dpp_pp_key "$CONFIG"
		wpa_cli -i"$ifname" set_network 0 dpp_pp_key "$CONFIG"
                ;;
	DPP-PP-KEY)
		wpa_cli -i"$ifname" set dpp_pp_key "$CONFIG"
		wpa_cli -i"$ifname" set_network 0 dpp_pp_key "$CONFIG"
		;;
	DPP-NET-ACCESS-KEY)
		wpa_cli -i"$ifname" set dpp_netaccesskey "$CONFIG"
		wpa_cli -i"$ifname" set_network 0 dpp_netaccesskey "$CONFIG"

		wpa_cli -i"$ifname" enable_network 0
		wpa_cli -i"$ifname" save_config

		wpa_cli -i"$ifname" disable 0
		wpa_cli -i"$ifname" enable 0

		. /sbin/wifi config
		config_foreach is_mld wifi-mld
		if [ -n "$mld_iface" ]; then
			config_foreach apply_mld_config wifi-iface
		else
			sect=
			config_foreach get_section wifi-iface "$ifname" sect
			update_wireless $sect
		fi

		;;
esac
