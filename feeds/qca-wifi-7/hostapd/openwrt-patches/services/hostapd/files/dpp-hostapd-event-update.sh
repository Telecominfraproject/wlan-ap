#!/bin/sh
#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#SPDX-License-Identifier: ISC
#

[ -e /lib/functions.sh ] && . /lib/functions.sh

IFNAME=$1
CMD=$2
CONFIG=$3
shift
shift
SSID=$@
PASS=$@

i=0
mld_iface=
mld_group=

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

apply_mld_config() {
	local config=$1
	local mld=

	config_get mld "$config" mld
	if [[ "$mld" = "$mld_group" ]]; then
		apply_dpp_config "$config" "$mld_iface"_link"$i"
		i=$((i+1))
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

apply_dpp_config() {
local sect=$1
local ifname=$2
	case "$CMD" in
		DPP-CONF-RECEIVED)
			hostapd_cli -i"$ifname" set wpa 2
			hostapd_cli -i"$ifname" set wpa_key_mgmt "DPP"
			hostapd_cli -i"$ifname" set ieee80211w 1
			hostapd_cli -i"$ifname" set rsn_pairwise "CCMP"
			;;
		DPP-CONFOBJ-AKM)
			encryption=
			dpp_akm=
			ieee80211w=
			sae_pwe=
			case "$CONFIG" in
				dpp)
					hostapd_cli -i"$ifname" set wpa_key_mgmt "DPP"
					hostapd_cli -i"$ifname" set ieee80211w 2
					encryption="dpp"
					ieee80211w=2
					;;
				sae)
					hostapd_cli -i"$ifname" set wpa_key_mgmt "SAE"
					hostapd_cli -i"$ifname" set ieee80211w 2
					hostapd_cli -i"$ifname" set sae_pwe 2
					encryption="sae"
					ieee80211w=2
					sae_pwe=2
					;;
				psk+sae|psk-sae)
					hostapd_cli -i"$ifname" set wpa_key_mgmt "WPA-PSK SAE"
					hostapd_cli -i"$ifname" set ieee80211w 1
					hostapd_cli -i"$ifname" set sae_pwe 2
					encryption="sae-mixed"
					ieee80211w=1
					sae_pwe=2
					sae_require_mfp=1
					;;
				psk)
					hostapd_cli -i"$ifname" set wpa_key_mgmt "WPA-PSK"
					hostapd_cli -i"$ifname" set ieee80211w 1
					encryption="psk2"
					ieee80211w=1
					;;
				dpp+sae|dpp-sae)
					hostapd_cli -i"$ifname" set wpa_key_mgmt "DPP SAE"
					hostapd_cli -i"$ifname" set ieee80211w 2
					hostapd_cli -i"$ifname" set sae_pwe 2
					encryption="sae"
					ieee80211w=2
					sae_pwe=2
					dpp_akm=1
					;;
				dpp+psk+sae|dpp-psk-sae)
					hostapd_cli -i"$ifname" set wpa_key_mgmt "DPP WPA-PSK SAE"
					hostapd_cli -i"$ifname" set ieee80211w 1
					hostapd_cli -i"$ifname" set sae_pwe 2
					encryption="sae-mixed"
					ieee80211w=1
					sae_require_mfp=1
					sae_pwe=2
					dpp_akm=1
					;;
				sae-ext-key)
					hostapd_cli -i"$ifname" set wpa_key_mgmt "SAE-EXT-KEY"
					hostapd_cli -i"$ifname" set rsn_pairwise "GCMP-256"
					hostapd_cli -i"$ifname" set group_cipher "GCMP-256"
					hostapd_cli -i"$ifname" set ieee80211w 2
					hostapd_cli -i"$ifname" set sae_pwe 2
					encryption="sae-ext-key"
					ieee80211w=2
					sae_pwe=2
					;;
			esac
			[ -n "$mld_group" ] && uci set wireless.$mld_group.encryption=$encryption
			uci set wireless."${sect}".encryption=$encryption
			uci set wireless."${sect}".dpp_akm=$dpp_akm
			uci set wireless."${sect}".ieee80211w=$ieee80211w
			uci set wireless."${sect}".sae_pwe=$sae_pwe
			uci commit wireless
			;;
		DPP-CONFOBJ-SSID)
			hostapd_cli -i"$ifname" set ssid "$SSID"
			[ -n "$mld_group" ] && uci set wireless.$mld_group.ssid="$SSID"
			uci set wireless."${sect}".ssid="$SSID"
			uci commit wireless
			;;
		DPP-CONNECTOR)
			hostapd_cli -i"$ifname" set dpp_connector "$CONFIG"
			uci set wireless."${sect}".dpp_connector="$CONFIG"
			uci commit wireless
			;;
		DPP-CONFOBJ-PASS)
			PASS_STR=$(hex2string "$PASS")
			hostapd_cli -i"$ifname" set wpa_passphrase "$PASS_STR"
			uci set wireless."${sect}".key="$PASS_STR"
			[ -n "$mld_group" ] && uci set wireless.$mld_group.key="$PASS_STR"
			uci commit wireless
			;;
		DPP-EVENT-SAE-PWE)
			hostapd_cli -i"$ifname" set sae_pwe "$CONFIG"
			uci set wireless."${sect}".sae_pwe="$CONFIG"
			uci commit wireless
			;;
		DPP-CONFOBJ-PSK)
			PASS_STR=$(hex2string "$CONFIG")
			hostapd_cli -i"$ifname" set wpa_psk "$PASS_STR"
			uci set wireless."${sect}".key="$PASS_STR"
			uci commit wireless
			;;
		DPP-C-SIGN-KEY)
			hostapd_cli -i"$ifname" set dpp_csign "$CONFIG"
			uci set wireless."${sect}".dpp_csign="$CONFIG"
			uci commit wireless
			;;
		DPP-CONNECTOR-C-SIGN-KEY)
			hostapd_cli -i"$ifname" set dpp_connector_csign "$CONFIG"
			uci set wireless."${sect}".dpp_connector_csign="$CONFIG"
			uci commit wireless
			;;
		DPP-PP-KEY)
			hostapd_cli -i"$ifname" set dpp_pp_key "$CONFIG"
			uci set wireless."${sect}".dpp_pp_key="$CONFIG"
			uci commit wireless
			;;
		DPP-NET-ACCESS-KEY)
			hostapd_cli -i"$ifname" set dpp_netaccesskey "$CONFIG"
			uci set wireless."${sect}".dpp_netaccesskey="$CONFIG"
			uci commit wireless

			hostapd_cli -i"$ifname" disable
			hostapd_cli -i"$ifname" enable
			;;
	esac
}

is_mld() {
	mld_group=$1
	mld_iface=$(uci get wireless."$1".ifname)
}

if [[ "$CMD" == *DPP* ]]; then
	. /sbin/wifi config
	config_foreach is_mld wifi-mld
	if [ -n "$mld_iface" ]; then
		config_foreach apply_mld_config wifi-iface
		[ "$CMD" = "DPP-NET-ACCESS-KEY" ] &&  wifi;
		#TODO. change needed here to bring up MLO vaps without wifi command
	else
		sect=
		config_foreach get_section wifi-iface "$IFNAME" sect
		apply_dpp_config "$sect" "$IFNAME"
	fi
fi
