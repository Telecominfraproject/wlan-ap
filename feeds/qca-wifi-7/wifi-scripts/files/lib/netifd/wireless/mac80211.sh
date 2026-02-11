#!/bin/sh

# This has been copied from https://github.com/openwrt/openwrt which is under GPL-2.0-only license
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-only

. /lib/netifd/netifd-wireless.sh
. /lib/netifd/hostapd.sh
. /lib/functions/system.sh

mac80211_update_mld_iface_config() {
	vif_name=$1
	mld_name=$2
	# Get the following from section wifi-mld
	config_get mld_ssid "$mld_name" ssid
	config_get mld_encryption "$mld_name" encryption
	config_get mld_key "$mld_name" key
	config_get mld_sae "$mld_name" sae_pwe
	config_get mld_vp "$mld_name" ppe_vp
	config_get mld_enable_epcs "$mld_name" enable_epcs
	config_get mld_ttlm_enable "$mld_name" ttlm_enable
	if [ -n "$mld_ssid" ]; then
		uci_set wireless "$vif_name" ssid "$mld_ssid"
	fi
	if [ -n "$mld_encryption" ]; then
		uci_set wireless "$vif_name" encryption "$mld_encryption"
	fi
	if [ -n "$mld_key" ]; then
		uci_set wireless "$vif_name" key "$mld_key"
	fi
	if [ -n "$mld_sae" ]; then
		uci_set wireless "$vif_name" sae_pwe "$mld_sae"
	fi
	if [ -n "$mld_vp" ]; then
		uci_set wireless "$vif_name" ppe_vp "$mld_vp"
	fi
	if [ -n "$mld_enable_epcs" ];then
		uci_set wireless "$vif_name" enable_epcs "$mld_enable_epcs"

		if [ "$mld_enable_epcs" = "1" ]; then
			for param in $epcs_params; do
				config_get "mld_$param" "$mld_name" "$param"
				mld_value_var="mld_$param"
				eval "value=\$$mld_value_var"
				if [ -n "$value" ]; then
					uci_set wireless "$vif_name" "$param" "$value"
				fi
			done
		fi
	fi
	if [ -n "$mld_ttlm_enable" ];then
		uci_set wireless "$vif_name" ttlm_enable "$mld_ttlm_enable"
	fi
	uci commit wireless
}

mac80211_update_mld_configs() {
	local iflist
	config_load wireless
	mac80211_update_mld_cfg() {
		append iflist "$1"
	}
	config_foreach mac80211_update_mld_cfg wifi-iface
	for name in $iflist
	do
		config_get mld_name "$name" mld
		config_get ml_device "$name" device
		config_get ht_mode "$ml_device" htmode
		if ([ -n "$ht_mode" ] && [[ "$ht_mode" == "EHT"* ]]  && [ -n "$mld_name" ]); then
			append mld_names "$mld_name"
			mac80211_update_mld_iface_config "$name" "$mld_name"
		fi
	done
}

mlo_add_flag=0
[ -f /tmp/mlo_support.txt ] && mlo_add_flag=$(cat /tmp/mlo_support.txt)
if [ $mlo_add_flag -eq 0 ]; then
	mac80211_update_mld_configs
	init_wireless_driver "$@"
fi

MP_CONFIG_INT="mesh_retry_timeout mesh_confirm_timeout mesh_holding_timeout mesh_max_peer_links
	       mesh_max_retries mesh_ttl mesh_element_ttl mesh_hwmp_max_preq_retries
	       mesh_path_refresh_time mesh_min_discovery_timeout mesh_hwmp_active_path_timeout
	       mesh_hwmp_preq_min_interval mesh_hwmp_net_diameter_traversal_time mesh_hwmp_rootmode
	       mesh_hwmp_rann_interval mesh_gate_announcements mesh_sync_offset_max_neighor
	       mesh_rssi_threshold mesh_hwmp_active_path_to_root_timeout mesh_hwmp_root_interval
	       mesh_hwmp_confirmation_interval mesh_awake_window mesh_plink_timeout"
MP_CONFIG_BOOL="mesh_auto_open_plinks mesh_fwding"
MP_CONFIG_STRING="mesh_power_mode"

is_wiphy_multi_radio=0

#update he params
bss_color=
enable_color=

#ACS DFS
acs_exclude_dfs=

updated_chanlist=
min_tx_power=
noscan=
ht_coex=
rx_stbc=
ldpc=
greenfield=
short_gi_20=
short_gi_40=
tx_stbc=
max_amsdu=
dsss_cck_40=
background_radar=
rxldpc=
short_gi_80=
short_gi_160=
tx_stbc_2by1=
su_beamformer=
su_beamformee=
mu_beamformer=
vht_txop_ps=
htc_vht=
rx_antenna_pattern=
tx_antenna_pattern=
vht160=
vht_max_mpdu=
vht_link_adapt=
vht_max_a_mpdu_len_exp=
disable_eml_cap=
enable_aal=
ml_max_rec_links=
eht_ulmumimo_80mhz=
eht_ulmumimo_160mhz=
eht_ulmumimo_320mhz=
ru_punct_bitmap=
ru_punct_acs_threshold=
use_ru_puncture_dfs=
he_su_beamformer=
he_su_beamformee=
he_mu_beamformer=
he_spr_psr_enabled=
he_twt_required=
he_ul_mumimo=
he_bss_color_enabled=
he_spr_non_srg_obss_pd_max_offset=
disable_csa_dfs=
discard_6g_awgn_event=
atfstrictsched=
wds=
wds_bridge=
start_disabled=
dtim_period=
max_listen_int=
he_6ghz_reg_pwr_type=

#dpp
dpp_ifaces=

#epcs params
enable_epcs=
epcs_params="epcs_he_mu_edca_ac_be_aifsn epcs_he_mu_edca_ac_be_aci epcs_he_mu_edca_ac_be_ecwmin
	     epcs_he_mu_edca_ac_be_ecwmax epcs_he_mu_edca_ac_be_timer epcs_he_mu_edca_ac_bk_aifsn
	     epcs_he_mu_edca_ac_bk_aci epcs_he_mu_edca_ac_bk_ecwmin epcs_he_mu_edca_ac_bk_ecwmax
	     epcs_he_mu_edca_ac_bk_timer epcs_he_mu_edca_ac_vi_ecwmin epcs_he_mu_edca_ac_vi_ecwmax
	     epcs_he_mu_edca_ac_vi_aifsn epcs_he_mu_edca_ac_vi_aci epcs_he_mu_edca_ac_vi_timer
	     epcs_he_mu_edca_ac_vo_aifsn epcs_he_mu_edca_ac_vo_aci epcs_he_mu_edca_ac_vo_ecwmin
	     epcs_he_mu_edca_ac_vo_ecwmax epcs_he_mu_edca_ac_vo_timer epcs_wmm_ac_be_cwmin
	     epcs_wmm_ac_be_cwmax epcs_wmm_ac_be_aifs epcs_wmm_ac_be_txop_limit epcs_wmm_ac_bk_cwmin
	     epcs_wmm_ac_bk_cwmax epcs_wmm_ac_bk_aifs epcs_wmm_ac_bk_txop_limit epcs_wmm_ac_vi_cwmin
	     epcs_wmm_ac_vi_cwmax epcs_wmm_ac_vi_aifs epcs_wmm_ac_vi_txop_limit epcs_wmm_ac_vo_cwmin
	     epcs_wmm_ac_vo_cwmax epcs_wmm_ac_vo_aifs epcs_wmm_ac_vo_txop_limit"
ttlm_enable=
enable_dscp_policy_capa=

#atf commands
atf_offload=

mac80211_freq_to_channel() {
        local freq=$1

        if [ "$freq" -lt 1000 ]; then
		echo 0
		return
        fi
        if [ "$freq" -eq 2484 ]; then
		echo 14
		return
        fi
        if [ "$freq" -eq 5935 ]; then
		echo 2
		return
        fi
        if [ "$freq" -lt 2484 ]; then
		echo $(((freq-2407)/5))
		return
        fi
        if [ "$freq" -ge 4910 ] && [ "$freq" -le 4980 ]; then
		echo $(((freq-4000)/5))
		return
        fi
        if [ "$freq" -lt 5950 ]; then
		echo $(((freq-5000)/5))
		return
        fi
        if [ "$freq" -le 45000 ]; then
		echo $(((freq-5950)/5))
		return
        fi
        if [ "$freq" -ge 58320 ] && [ "$freq" -le 70200 ]; then
		echo $(((freq-56160)/5))
		return
        fi
}

wdev_tool() {
	ucode /usr/share/hostap/wdev.uc "$@"
}

ubus_call() {
	flock /var/run/hostapd.lock ubus call "$@"
}

drv_mac80211_init_device_config() {
	hostapd_common_add_device_config

	config_add_string path phy 'macaddr:macaddr'
	config_add_string tx_burst
	config_add_string distance band
	config_add_int radio beacon_int chanbw frag rts
	config_add_int rxantenna txantenna txpower min_tx_power antenna_gain
	config_add_int num_global_macaddr multiple_bssid
	config_add_boolean noscan ht_coex acs_exclude_dfs background_radar
	config_add_array ht_capab
	config_add_array channels
	config_add_array scan_list
	config_add_boolean \
		rxldpc \
		short_gi_80 \
		short_gi_160 \
		tx_stbc_2by1 \
		su_beamformer \
		su_beamformee \
		mu_beamformer \
		mu_beamformee \
		he_su_beamformer \
		he_su_beamformee \
		he_mu_beamformer \
		vht_txop_ps \
		htc_vht \
		rx_antenna_pattern \
		tx_antenna_pattern \
		he_spr_sr_control \
		he_spr_psr_enabled \
		he_bss_color_enabled \
		he_twt_required \
		use_ru_puncture_dfs
	config_add_int \
		beamformer_antennas \
		beamformee_antennas \
		vht_max_a_mpdu_len_exp \
		vht_max_mpdu \
		vht_link_adapt \
		vht160 \
		rx_stbc \
		tx_stbc \
		he_bss_color \
		he_spr_non_srg_obss_pd_max_offset \
		he_spr_sr_control \
		he_ul_mumimo \
		eht_ulmumimo_160mhz \
		eht_ulmumimo_320mhz \
		ru_punct_bitmap \
		ru_punct_acs_threshold \
		ccfs \
		multiple_bssid \
		mbssid_group_size \
		he_6ghz_reg_pwr_type
	config_add_boolean \
		ldpc \
		greenfield \
		short_gi_20 \
		short_gi_40 \
		max_amsdu \
		dsss_cck_40 \
		disable_eml_cap \
		disable_csa_dfs \
		discard_6g_awgn_event
	config_add_boolean atfstrictsched
}

drv_mac80211_init_iface_config() {
	hostapd_common_add_bss_config

	config_add_string 'macaddr:macaddr' ifname mld

	config_add_boolean wds powersave enable
	config_add_string wds_bridge mld
	config_add_int maxassoc
	config_add_int max_listen_int
	config_add_int dtim_period
	config_add_int start_disabled
	config_add_int ieee80211w
	config_add_int beacon_prot
	config_add_int unsol_bcast_presp
	config_add_int fils_discovery
	config_add_string ppe_vp
	config_add_boolean disable_reconfig
	config_add_boolean enable_epcs
	config_add_boolean enable_scs
	config_add_boolean ttlm_enable
	config_add_boolean enable_mscs
	config_add_boolean enable_dscp_policy_capa

	#atf
	config_add_boolean commitatf
	config_add_int atfssidsched
	config_add_boolean atfssidgroup

	#epcs
	config_add_boolean enable_epcs
	for param in $epcs_params; do
		config_add_int "$param"
	done

	config_add_int twt_responder

	# mesh
	config_add_string mesh_id
	config_add_int $MP_CONFIG_INT
	config_add_boolean $MP_CONFIG_BOOL
	config_add_string $MP_CONFIG_STRING

	config_add_boolean enable_aal
	config_add_int ml_max_rec_links
}

mac80211_add_capabilities() {
	local __var="$1"; shift
	local __mask="$1"; shift
	local __out= oifs

	oifs="$IFS"
	IFS=:
	for capab in "$@"; do
		set -- $capab

		[ "$(($4))" -gt 0 ] || continue
		[ "$(($__mask & $2))" -eq "$((${3:-$2}))" ] || continue
		__out="$__out[$1]"
	done
	IFS="$oifs"

	export -n -- "$__var=$__out"
}

mac80211_add_he_capabilities() {
	local __out= oifs

	oifs="$IFS"
	IFS=:
	for capab in "$@"; do
		set -- $capab
		[ "$(($4))" -gt 0 ] || continue
		[ "$(((0x$2) & $3))" -gt 0 ] || {
			eval "$1=0"
			continue
		}
		append base_cfg "$1=1" "$N"
	done
	IFS="$oifs"
}

get_sta_freq_list() {
	local phy=$1
	local radio_id=$2
	local band_name=$3

	[ "$radio_id" = "-1" ] && return ""

	freq_range=$(iw $phy info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3, $6}')
	set -- $freq_range
	freq1=$1
	freq2=$2

        case "$band_name" in
                2g) band="1:";;
                5gl|5gh|5g) band="2:";;
                60g) band="3:";;
                6g|6gl|6gh) band="4:";;
        esac

	iw "$phy" info | awk -v band="$band" -v min="$freq1" -v max="$freq2" '

$1 ~ /Band/ {
        band_match = band == $2
}

band_match && $3 == "MHz" {
        freq = $2
         if (freq >= min && freq <= max) {
                print int($2)
        }
}
' | tr '\n' ' '
}

cfg_append() {
        echo "$1" >> "$2"
}

mac80211_prepare_atf_config() {
	local config_file="/var/run/hostapd-atf-$phy$vif_phy_suffix.conf"
	local hostapd_config_file=$1
	local cfg=
	local hostapd_cfg=
	config_load wireless

	config_get atf_offload atf_offload Enable


	[ -z "$atf_offload" ] && return
	[ -n "$atf_offload" ] && [ "$atf_offload" -le 0 ] && return

	append hostapd_cfg "atf_offload=1" "$N"
	append hostapd_cfg "atf_offload_config=$config_file" "$N"

	atf_group_configcfg80211() {
		local cmd
		local group
		local ssid
		local airtime
		local atf_device

		config_get atf_device "$1" device

		if [[ "$device" != "$atf_device" ]]; then
			return
		fi

		config_get cmd "$1" command
		config_get group "$1" group
		config_get ssid "$1" ssid
		config_get airtime "$1" airtime

		if [ -z "$cmd" ] || [ -z "$group" ] ; then
			echo "Invalid ATF GROUP Configuration" > /dev/ttyMSM0
			return
		fi

		if [ "$cmd" == "delgroup" ]; then
			append cfg "atf-del-group=$group" "$N"
		fi

		if [ "$cmd" == "addgroup" ] && [ -n "$ssid" ] && [ -n "$airtime" ]; then
			# Validate airtime is a number between 0 and 100
			if ! [[ "$airtime" =~ ^[0-9]+$ ]] || [ "$airtime" -lt 0 ] || [ "$airtime" -gt 100 ]; then
				echo "Invalid airtime value: $airtime. Must be between 0 and 100" > /dev/ttyMSM0
				return
			fi
			append cfg "atf-group=$group" "$N"
			append cfg "atf-group-command=$cmd" "$N"
			append cfg "atf-group-ssid=$ssid" "$N"
			append cfg "atf-group-airtime=$airtime" "$N"
		fi	
	}
	config_foreach atf_group_configcfg80211 atf-config-group

	atf_ssid_configcfg80211() {
		local cmd
		local ssid
		local airtime
		local atf_device

		config_get atf_device "$1" device

		if [[ "$device" != "$atf_device" ]]; then
                        return
		fi

		config_get cmd "$1" command
		config_get ssid "$1" ssid
		config_get airtime "$1" airtime

		if [ -z "$cmd" ] || [ -z "$ssid" ] ; then
			echo "Invalid ATF SSID Configuration" > /dev/ttyMSM0
		fi

		if [ "$cmd" == "addssid" ] && [ -n "$airtime" ]; then
			# Validate airtime is a number between 0 and 100
			if ! [[ "$airtime" =~ ^[0-9]+$ ]] || [ "$airtime" -lt 0 ] || [ "$airtime" -gt 100 ]; then
				echo "Invalid airtime value: $airtime. Must be between 0 and 100" > /dev/ttyMSM0
				return
			fi

			append cfg "atf-ssid=$ssid" "$N"
			append cfg "atf-ssid-command=$cmd" "$N"
			append cfg "atf-ssid-airtime=$airtime" "$N"
		fi

		if [ "$cmd" == "delssid" ]; then
			append cfg "atf-del-ssid=$ssid" "$N"
		fi

	}
	config_foreach atf_ssid_configcfg80211 atf-config-ssid

	atf_sta_configcfg80211() {
		local cmd
		local ssid
		local airtime
		local atf_device
		local mac

		config_get atf_device "$1" device
		if [[ "$device" != "$atf_device" ]]; then
			return
		fi

		config_get cmd "$1" command
		config_get airtime "$1" airtime
		config_get ssid "$1" ssid
		config_get mac "$1" macaddr

		if [ -z "$cmd" ] || [ -z "$mac" ] ; then
			echo "Invalid ATF STA Configuration"
			return
		fi

		if [ "$cmd" == "addsta" ] && [ -n "$airtime" ]; then
			# Validate airtime is a number between 0 and 100
			if ! [[ "$airtime" =~ ^[0-9]+$ ]] || [ "$airtime" -lt 0 ] || [ "$airtime" -gt 100 ]; then
				echo "Invalid airtime value: $airtime. Must be between 0 and 100" > /dev/ttyMSM0
				return
			fi
			append cfg "atf-sta=$mac" "$N"
			append cfg "atf-sta-command=$cmd" "$N"
			append cfg "atf-sta-ssid=$ssid" "$N"
			append cfg "atf-sta-airtime=$airtime" "$N"
		fi

		if [ "$cmd" == "delsta" ]; then
			append cfg "atf-del-sta=$mac" "$N"
		fi
	}
	config_foreach atf_sta_configcfg80211 atf-config-sta

	cat >>  "$hostapd_config_file" <<EOF

$hostapd_cfg

EOF

	cat > "$config_file" <<EOF
$cfg

EOF

}

mac80211_hostapd_setup_base() {
	local phy="$1"
	local sedString=

	json_select config

	[ "$auto_channel" -gt 0 ] && channel=acs_survey

	[ "$auto_channel" -gt 0 ] && json_get_vars acs_exclude_dfs
	[ -n "$acs_exclude_dfs" ] && [ "$acs_exclude_dfs" -gt 0 ] &&
		append base_cfg "acs_exclude_dfs=1" "$N"

	json_get_vars noscan ht_coex min_tx_power:0 tx_burst disable_csa_dfs use_ru_puncture_dfs
	json_get_values ht_capab_list ht_capab
	json_get_values channel_list channels
	json_get_vars disable_eml_cap discard_6g_awgn_event ccfs atfstrictsched

	[ "$auto_channel" = 0 ] && [ -z "$channel_list" ] && \
		channel_list="$channel"

	[ "$min_tx_power" -gt 0 ] && append base_cfg "min_tx_power=$min_tx_power" "$N"

	if [ "$band" = "2g" ]; then
		sedString="iw phy ${phy} info | awk  '/Band 1/{ f = 1; next } /Band /{ f = 0 } f'"
	elif [ "$band" = "5g" ]; then
		sedString="iw phy ${phy} info | awk  '/Band 2/{ f = 1; next } /Band /{ f = 0 } f'"
	elif [ "$band" = "6g" ]; then
		sedString="iw phy ${phy} info | awk  '/Band 4/{ f = 1; next } /Band /{ f = 0 } f'"
	fi

	set_default noscan 0

	[ "$noscan" -gt 0 ] && hostapd_noscan=1
	[ "$tx_burst" = 0 ] && tx_burst=

	chan_ofs=0
	[ "$band" = "6g" ] && chan_ofs=1

	if [ "$band" != "6g" ]; then
		ieee80211n=1
		ht_capab=
		case "$htmode" in
			VHT20|HT20|HE20|EHT20) ;;
			HT40*|VHT40|VHT80|VHT160|HE40|HE80|HE160|EHT40|EHT80|EHT160|EHT320)
				case "$hwmode" in
					a)
						case "$(( (($channel / 4) + $chan_ofs) % 2 ))" in
							1) ht_capab="[HT40+]";;
							0) ht_capab="[HT40-]";;
						esac
					;;
					*)
						case "$htmode" in
							HT40+) ht_capab="[HT40+]";;
							HT40-) ht_capab="[HT40-]";;
							*)
								if [ "$channel" -lt 7 ]; then
									ht_capab="[HT40+]"
								else
									ht_capab="[HT40-]"
								fi
							;;
						esac
					;;
				esac
				[ "$auto_channel" -gt 0 ] && ht_capab="[HT40+]"
			;;
			*) ieee80211n= ;;
		esac

		[ -n "$ieee80211n" ] && {
			append base_cfg "ieee80211n=1" "$N"

			set_default ht_coex 0
			append base_cfg "ht_coex=$ht_coex" "$N"

			json_get_vars \
				ldpc:1 \
				greenfield:0 \
				short_gi_20:1 \
				short_gi_40:1 \
				tx_stbc:1 \
				rx_stbc:3 \
				max_amsdu:1 \
				dsss_cck_40:1

			ht_cap_mask=0
			for cap in $(eval $sedString | grep -E '^\s*Capabilities:' | cut -d: -f2); do
				ht_cap_mask="$(($ht_cap_mask | $cap))"
			done

			cap_rx_stbc=$((($ht_cap_mask >> 8) & 3))
			[ "$rx_stbc" -lt "$cap_rx_stbc" ] && cap_rx_stbc="$rx_stbc"
			ht_cap_mask="$(( ($ht_cap_mask & ~(0x300)) | ($cap_rx_stbc << 8) ))"

			mac80211_add_capabilities ht_capab_flags $ht_cap_mask \
				LDPC:0x1::$ldpc \
				GF:0x10::$greenfield \
				SHORT-GI-20:0x20::$short_gi_20 \
				SHORT-GI-40:0x40::$short_gi_40 \
				TX-STBC:0x80::$tx_stbc \
				RX-STBC1:0x300:0x100:1 \
				RX-STBC12:0x300:0x200:1 \
				RX-STBC123:0x300:0x300:1 \
				MAX-AMSDU-7935:0x800::$max_amsdu \
				DSSS_CCK-40:0x1000::$dsss_cck_40

			ht_capab="$ht_capab$ht_capab_flags"
			[ -n "$ht_capab" ] && append base_cfg "ht_capab=$ht_capab" "$N"
		}
	fi

	# 802.11ac
	enable_ac=0
	vht_oper_chwidth=0
	vht_center_seg0=
	eht_oper_chwidth=0
	eht_center_seg0=

	idx="$channel"
	case "$htmode" in
		VHT20|HE20|EHT20)
			enable_ac=1
			if [ "$hwmode" = "a" ]; then
				vht_oper_chwidth=0
				vht_center_seg0=$idx
				eht_center_seg0=$idx
			fi
		;;
		VHT40|HE40|EHT40)
			if [ "$channel" -le 2 ]; then
				idx=$(($channel + 2))
			else

				case "$(( (($channel / 4) + $chan_ofs) % 2 ))" in
					1) idx=$(($channel + 2));;
					0) idx=$(($channel - 2));;
				esac
			fi
			enable_ac=1
			vht_oper_chwidth=0
			vht_center_seg0=$idx
			if [ "$hwmode" = "a" ]; then
				vht_oper_chwidth=0
				vht_center_seg0=$idx
			fi
		;;
		VHT80|HE80|EHT80)
			case "$(( (($channel / 4) + $chan_ofs) % 4 ))" in
				1) idx=$(($channel + 6));;
				2) idx=$(($channel + 2));;
				3) idx=$(($channel - 2));;
				0) idx=$(($channel - 6));;
			esac
			enable_ac=1
			vht_oper_chwidth=1
			vht_center_seg0=$idx
		;;
		VHT160|HE160|EHT160|EHT320)
			if [ "$band" = "6g" ]; then
				case "$channel" in
					1|5|9|13|17|21|25|29) idx=15;;
					33|37|41|45|49|53|57|61) idx=47;;
					65|69|73|77|81|85|89|93) idx=79;;
					97|101|105|109|113|117|121|125) idx=111;;
					129|133|137|141|145|149|153|157) idx=143;;
					161|165|169|173|177|181|185|189) idx=175;;
					193|197|201|205|209|213|217|221) idx=207;;
				esac
			else
				case "$channel" in
					36|40|44|48|52|56|60|64) idx=50;;
					100|104|108|112|116|120|124|128) idx=114;;
					149|153|157|161|165|169|173|177) idx=163;;
				esac
			fi
			enable_ac=1
			vht_oper_chwidth=2
			vht_center_seg0=$idx
		;;
	esac
	[ "$band" = "5g" ] && {
		json_get_vars background_radar:0

		[ "$background_radar" -eq 1 ] && append base_cfg "enable_background_radar=1" "$N"
	}

	if [ "$htmode" = "EHT320" ]; then
		eht_oper_chwidth=9
		if [ "$freq" -ge 5500 ] && [ "$freq" -le 5730 ]; then
			eht_center_seg0=130
		else
			eht_center_seg0=$vht_center_seg0
		fi
	else
		eht_oper_chwidth=$vht_oper_chwidth
		eht_center_seg0=$vht_center_seg0
	fi

	[ "$band" = "6g" ] && {
		op_class=
		case "$htmode" in
			HE20|EHT20)
				if [ "$freq" == "5935" ]; then
					op_class=136
				else
					op_class=131
				fi
			;;
			EHT320)
				if [ -n "$ccfs" ] && [ "$ccfs" -gt 0 ]; then
					idx="$ccfs"
				elif [ -z "$ccfs" ] || [ "$ccfs" -eq "0" ]; then
					idx="$(mac80211_get_seg0 "320")"
				fi

				op_class=137
				eht_center_seg0=$idx
				eht_oper_chwidth=9
			;;
			HE*|EHT*) op_class=$((132 + $vht_oper_chwidth));;
		esac
		[ -n "$op_class" ] && append base_cfg "op_class=$op_class" "$N"
	}

	[ "$band" = "6g" ] && enable_ac=0

	if [ "$enable_ac" != "0" ]; then
		json_get_vars \
			rxldpc:1 \
			short_gi_80:1 \
			short_gi_160:1 \
			tx_stbc_2by1:1 \
			su_beamformer:1 \
			su_beamformee:1 \
			mu_beamformer:1 \
			mu_beamformee:1 \
			vht_txop_ps:1 \
			htc_vht:1 \
			beamformee_antennas:4 \
			beamformer_antennas:4 \
			rx_antenna_pattern:1 \
			tx_antenna_pattern:1 \
			vht_max_a_mpdu_len_exp:7 \
			vht_max_mpdu:11454 \
			rx_stbc:4 \
			vht_link_adapt:3 \
			vht160:2

		set_default tx_burst 2.0
		append base_cfg "ieee80211ac=1" "$N"
		vht_cap=0
		for cap in $(eval $sedString | awk -F "[()]" '/VHT Capabilities/ { print $2 }'); do
			vht_cap="$(($vht_cap | $cap))"
		done

		append base_cfg "vht_oper_chwidth=$vht_oper_chwidth" "$N"
		append base_cfg "vht_oper_centr_freq_seg0_idx=$vht_center_seg0" "$N"

		cap_rx_stbc=$((($vht_cap >> 8) & 7))
		[ "$rx_stbc" -lt "$cap_rx_stbc" ] && cap_rx_stbc="$rx_stbc"
		vht_cap="$(( ($vht_cap & ~(0x700)) | ($cap_rx_stbc << 8) ))"

		[ "$vht_oper_chwidth" -lt 2 ] && {
			vht160=0
			short_gi_160=0
		}

		mac80211_add_capabilities vht_capab $vht_cap \
			RXLDPC:0x10::$rxldpc \
			SHORT-GI-80:0x20::$short_gi_80 \
			SHORT-GI-160:0x40::$short_gi_160 \
			TX-STBC-2BY1:0x80::$tx_stbc_2by1 \
			SU-BEAMFORMER:0x800::$su_beamformer \
			SU-BEAMFORMEE:0x1000::$su_beamformee \
			MU-BEAMFORMER:0x80000::$mu_beamformer \
			MU-BEAMFORMEE:0x100000::$mu_beamformee \
			VHT-TXOP-PS:0x200000::$vht_txop_ps \
			HTC-VHT:0x400000::$htc_vht \
			RX-ANTENNA-PATTERN:0x10000000::$rx_antenna_pattern \
			TX-ANTENNA-PATTERN:0x20000000::$tx_antenna_pattern \
			RX-STBC-1:0x700:0x100:1 \
			RX-STBC-12:0x700:0x200:1 \
			RX-STBC-123:0x700:0x300:1 \
			RX-STBC-1234:0x700:0x400:1 \

		[ "$(($vht_cap & 0x800))" -gt 0 -a "$su_beamformer" -gt 0 ] && {
			cap_ant="$(( ( ($vht_cap >> 16) & 3 ) + 1 ))"
			[ "$cap_ant" -gt "$beamformer_antennas" ] && cap_ant="$beamformer_antennas"
			[ "$cap_ant" -gt 1 ] && vht_capab="$vht_capab[SOUNDING-DIMENSION-$cap_ant]"
		}

		[ "$(($vht_cap & 0x1000))" -gt 0 -a "$su_beamformee" -gt 0 ] && {
			cap_ant="$(( ( ($vht_cap >> 13) & 3 ) + 1 ))"
			[ "$cap_ant" -gt "$beamformee_antennas" ] && cap_ant="$beamformee_antennas"
			[ "$cap_ant" -gt 1 ] && vht_capab="$vht_capab[BF-ANTENNA-$cap_ant]"
		}

		# supported Channel widths
		vht160_hw=0
		case "$htmode" in
			VHT160|HE160|EHT160|EHT320)
				([ "$(($vht_cap & 12))" -eq 4 ] && [ 1 -le "$vht160" ]) && \
				vht160_hw=1
				[ "$vht160_hw" = 1 ] && vht_capab="$vht_capab[VHT160]"
				;;
			VHT80+80|HE80+80)
				([ "$(($vht_cap & 12))" -eq 8 ] && [ 2 -le "$vht160" ]) && \
				vht160_hw=2
				[ "$vht160_hw" = 2 ] && vht_capab="$vht_capab[VHT160-80PLUS80]"
				;;
		esac

		# maximum MPDU length
		vht_max_mpdu_hw=3895
		[ "$(($vht_cap & 3))" -ge 1 -a 7991 -le "$vht_max_mpdu" ] && \
			vht_max_mpdu_hw=7991
		[ "$(($vht_cap & 3))" -ge 2 -a 11454 -le "$vht_max_mpdu" ] && \
			vht_max_mpdu_hw=11454
		[ "$vht_max_mpdu_hw" != 3895 ] && \
			vht_capab="$vht_capab[MAX-MPDU-$vht_max_mpdu_hw]"

		# maximum A-MPDU length exponent
		vht_max_a_mpdu_len_exp_hw=0
		[ "$(($vht_cap & 58720256))" -ge 8388608 -a 1 -le "$vht_max_a_mpdu_len_exp" ] && \
			vht_max_a_mpdu_len_exp_hw=1
		[ "$(($vht_cap & 58720256))" -ge 16777216 -a 2 -le "$vht_max_a_mpdu_len_exp" ] && \
			vht_max_a_mpdu_len_exp_hw=2
		[ "$(($vht_cap & 58720256))" -ge 25165824 -a 3 -le "$vht_max_a_mpdu_len_exp" ] && \
			vht_max_a_mpdu_len_exp_hw=3
		[ "$(($vht_cap & 58720256))" -ge 33554432 -a 4 -le "$vht_max_a_mpdu_len_exp" ] && \
			vht_max_a_mpdu_len_exp_hw=4
		[ "$(($vht_cap & 58720256))" -ge 41943040 -a 5 -le "$vht_max_a_mpdu_len_exp" ] && \
			vht_max_a_mpdu_len_exp_hw=5
		[ "$(($vht_cap & 58720256))" -ge 50331648 -a 6 -le "$vht_max_a_mpdu_len_exp" ] && \
			vht_max_a_mpdu_len_exp_hw=6
		[ "$(($vht_cap & 58720256))" -ge 58720256 -a 7 -le "$vht_max_a_mpdu_len_exp" ] && \
			vht_max_a_mpdu_len_exp_hw=7
		vht_capab="$vht_capab[MAX-A-MPDU-LEN-EXP$vht_max_a_mpdu_len_exp_hw]"

		# whether or not the STA supports link adaptation using VHT variant
		vht_link_adapt_hw=0
		[ "$(($vht_cap & 201326592))" -ge 134217728 -a 2 -le "$vht_link_adapt" ] && \
			vht_link_adapt_hw=2
		[ "$(($vht_cap & 201326592))" -ge 201326592 -a 3 -le "$vht_link_adapt" ] && \
			vht_link_adapt_hw=3
		[ "$vht_link_adapt_hw" != 0 ] && \
			vht_capab="$vht_capab[VHT-LINK-ADAPT-$vht_link_adapt_hw]"

		[ -n "$vht_capab" ] && append base_cfg "vht_capab=$vht_capab" "$N"
	fi

	# 802.11ax
	enable_ax=0
	enable_be=0
	case "$htmode" in
		HE*) enable_ax=1 ;;
		EHT*) enable_ax=1; enable_be=1
		      [ -n "$disable_eml_cap" ] && append base_cfg "disable_eml_cap=$disable_eml_cap" "$N"
		;;
	esac

	if [ "$enable_ax" != "0" ]; then
		json_get_vars \
			he_su_beamformer:1 \
			he_su_beamformee:1 \
			he_mu_beamformer:1 \
			he_twt_required:0 \
			he_spr_sr_control:3 \
			he_spr_psr_enabled:0 \
			he_spr_non_srg_obss_pd_max_offset:0 \
			he_bss_color:128 \
			he_bss_color_enabled:1 \
			he_ul_mumimo \
			eht_ulmumimo_80mhz \
			eht_ulmumimo_160mhz \
			eht_ulmumimo_320mhz \
			multiple_bssid \
			mbssid_group_size \
			he_6ghz_reg_pwr_type:0

		if [ "$band" = "6g" ]; then
			append base_cfg "he_6ghz_reg_pwr_type=$he_6ghz_reg_pwr_type" "$N"
		fi

		he_phy_cap=$(eval $sedString | awk -F "[()]" '/HE PHY Capabilities/ { print $2 }' | head -1)
		he_phy_cap=${he_phy_cap:2}
		he_mac_cap=$(eval $sedString | awk -F "[()]" '/HE MAC Capabilities/ { print $2 }' | head -1)
		he_mac_cap=${he_mac_cap:2}

		append base_cfg "ieee80211ax=1" "$N"
		[ "$hwmode" = "a" ] && {
			append base_cfg "he_oper_chwidth=$vht_oper_chwidth" "$N"
			append base_cfg "he_oper_centr_freq_seg0_idx=$vht_center_seg0" "$N"
		}

		mac80211_add_he_capabilities \
			he_su_beamformer:${he_phy_cap:6:2}:0x80:$he_su_beamformer \
			he_su_beamformee:${he_phy_cap:8:2}:0x1:$he_su_beamformee \
			he_mu_beamformer:${he_phy_cap:8:2}:0x2:$he_mu_beamformer \
			he_spr_psr_enabled:${he_phy_cap:14:2}:0x1:$he_spr_psr_enabled \
			he_twt_required:${he_mac_cap:0:2}:0x6:$he_twt_required

		append base_cfg "he_default_pe_duration=4" "$N"
		append base_cfg "he_rts_threshold=1023" "$N"
		append base_cfg "he_mu_edca_qos_info_param_count=0" "$N"
		append base_cfg "he_mu_edca_qos_info_q_ack=0" "$N"
		append base_cfg "he_mu_edca_qos_info_queue_request=0" "$N"
		append base_cfg "he_mu_edca_qos_info_txop_request=0" "$N"
		append base_cfg "he_mu_edca_ac_be_aifsn=8" "$N"
		append base_cfg "he_mu_edca_ac_be_aci=0" "$N"
		append base_cfg "he_mu_edca_ac_be_ecwmin=9" "$N"
		append base_cfg "he_mu_edca_ac_be_ecwmax=10" "$N"
		append base_cfg "he_mu_edca_ac_be_timer=255" "$N"
		append base_cfg "he_mu_edca_ac_bk_aifsn=15" "$N"
		append base_cfg "he_mu_edca_ac_bk_aci=1" "$N"
		append base_cfg "he_mu_edca_ac_bk_ecwmin=9" "$N"
		append base_cfg "he_mu_edca_ac_bk_ecwmax=10" "$N"
		append base_cfg "he_mu_edca_ac_bk_timer=255" "$N"
		append base_cfg "he_mu_edca_ac_vi_ecwmin=5" "$N"
		append base_cfg "he_mu_edca_ac_vi_ecwmax=7" "$N"
		append base_cfg "he_mu_edca_ac_vi_aifsn=5" "$N"
		append base_cfg "he_mu_edca_ac_vi_aci=2" "$N"
		append base_cfg "he_mu_edca_ac_vi_timer=255" "$N"
		append base_cfg "he_mu_edca_ac_vo_aifsn=5" "$N"
		append base_cfg "he_mu_edca_ac_vo_aci=3" "$N"
		append base_cfg "he_mu_edca_ac_vo_ecwmin=5" "$N"
		append base_cfg "he_mu_edca_ac_vo_ecwmax=7" "$N"
		append base_cfg "he_mu_edca_ac_vo_timer=255" "$N"

		if [ -n "$he_ul_mumimo" ]; then
			if [ "$he_ul_mumimo" -eq 0 ]; then
				append base_cfg "he_ul_mumimo=0" "$N"
			elif [ "$he_ul_mumimo" -gt 0 ]; then
				append base_cfg "he_ul_mumimo=1" "$N"
			fi
		else
			append base_cfg "he_ul_mumimo=-1" "$N"
		fi

		# If he_bss_color_enabled is set to zero by default, handle enable_color accordingly.
		# he_bss_color will not work in this case.
		if [ "$he_bss_color_enabled" -gt 0 ]; then
			config_get enable_color mac80211 enable_color 1
			if [ "$enable_color" -eq 1 ]; then
				bss_color=$(head -1 /dev/urandom | tr -dc '0-9' | head -c2)
				[ -z "$bss_color" ] && bss_color=0
				[ "$bss_color" != "0" ] && bss_color=${bss_color#0}
				bss_color=$((bss_color % 63))
				bss_color=$((bss_color + 1))
				append base_cfg "he_bss_color=$bss_color" "$N"
			fi

			if [ "$he_spr_non_srg_obss_pd_max_offset" -gt 0 ]; then
				append base_cfg "he_spr_non_srg_obss_pd_max_offset=$he_spr_non_srg_obss_pd_max_offset" "$N"
				he_spr_sr_control=$((he_spr_sr_control | (1 << 2)))
			fi

			[ "$he_spr_psr_enabled" -gt 0 ] || he_spr_sr_control=$((he_spr_sr_control | (1 << 0)))
			append base_cfg "he_spr_sr_control=$he_spr_sr_control" "$N"
		else
			append base_cfg "he_bss_color_disabled=1" "$N"
		fi

		if [ "$enable_be" != "0" ]; then
			append base_cfg "ieee80211be=1" "$N"
			[ "$hwmode" = "a" ] && {
				append base_cfg "eht_oper_chwidth=$eht_oper_chwidth" "$N"
				append base_cfg "eht_oper_centr_freq_seg0_idx=$eht_center_seg0" "$N"
			}
			json_get_vars ru_punct_bitmap:0 ru_punct_acs_threshold:0 ccfs:0
			append base_cfg "eht_su_beamformer=1" "$N"
			append base_cfg "eht_mu_beamformer=1" "$N"
			append base_cfg "eht_su_beamformee=1" "$N"

			if [ -n "$eht_ulmumimo_80mhz" ]; then
				if [ "$eht_ulmumimo_80mhz" -eq 0 ]; then
					append base_cfg "eht_ulmumimo_80mhz=0" "$N"
				elif [  "$eht_ulmumimo_80mhz" -gt 0 ]; then
					append base_cfg "eht_ulmumimo_80mhz=1" "$N"
				fi
			else
				append base_cfg "eht_ulmumimo_80mhz=-1" "$N"
			fi

			if [ -n "$eht_ulmumimo_160mhz" ]; then
				if [ "$eht_ulmumimo_160mhz" -eq 0 ]; then
					append base_cfg "eht_ulmumimo_160mhz=0" "$N"
				elif [  "$eht_ulmumimo_160mhz" -gt 0 ]; then
					append base_cfg "eht_ulmumimo_160mhz=1" "$N"
				fi
			else
				append base_cfg "eht_ulmumimo_160mhz=-1" "$N"
			fi

			if [ -n "$eht_ulmumimo_320mhz" ]; then
				if [ "$eht_ulmumimo_320mhz" -eq 0 ]; then
					append base_cfg "eht_ulmumimo_320mhz=0" "$N"
				elif [  "$eht_ulmumimo_320mhz" -gt 0 ]; then
					append base_cfg "eht_ulmumimo_320mhz=1" "$N"
				fi
			else
				append base_cfg "eht_ulmumimo_320mhz=-1" "$N"
			fi

			if [ -n "$ru_punct_bitmap" ] && [ "$ru_punct_bitmap" -gt 0 ]; then
				append base_cfg "punct_bitmap=$ru_punct_bitmap" "$N"
			fi

			if [ -n "$ru_punct_acs_threshold" ] && [ "$ru_punct_acs_threshold" -gt 0 ]; then
				append base_cfg "punct_acs_threshold=$ru_punct_acs_threshold" "$N"
			fi

			[ -n "$use_ru_puncture_dfs" ] && append base_cfg "use_ru_puncture_dfs=$use_ru_puncture_dfs" "$N"
		fi

		if [ "$band" = "6g" ]; then
			if [ -z "$multiple_bssid" ] && [ "$has_ap" -gt 1 ]; then
				multiple_bssid=3
			fi
		fi

		if [ "$multiple_bssid" == "3" ]; then
			if [ -z "$mbssid_group_size" ]; then
				mbssid_group_size=4
			fi
		fi

		if [[ "$htmode" == "HE"* ]] || [ "$band" = "6g" ]; then
			if [ "$has_ap" -gt 1 ]; then
				append base_cfg "mbssid=$multiple_bssid" "$N"
			fi

			if [ "$multiple_bssid" == "3" ]; then
				append base_cfg "mbssid_group_size=$mbssid_group_size" "$N"
			fi
		fi
	fi

	[ -n "$disable_csa_dfs" ] && append base_cfg "disable_csa_dfs=$disable_csa_dfs" "$N"
	[ -n "$discard_6g_awgn_event" ] && append base_cfg "discard_6g_awgn_event=$discard_6g_awgn_event" "$N"
	[ -n "$atfstrictsched" ] && append base_cfg "atfstrictsched=$atfstrictsched" "$N"

	hostapd_prepare_device_config "$hostapd_conf_file" nl80211

	[ -n "$updated_chanlist" ] && channel_list=$(echo $updated_chanlist)

	cat >> "$hostapd_conf_file" <<EOF
${channel:+channel=$channel}
${channel_list:+chanlist=$channel_list}
${hostapd_noscan:+noscan=1}
${tx_burst:+tx_queue_data2_burst=$tx_burst}
#num_global_macaddr=$num_global_macaddr
$base_cfg

EOF
	json_select ..
	mac80211_prepare_atf_config "$hostapd_conf_file" "$radio"
}

mac80211_wds_support_check() {
	local phy="$1"
	wds_support=1

	local platform freq board_type
	platform=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $1}')
	case "$platform" in
		"IPQ8074" | "IPQ6018" | "IPQ5018")
			wds_support=$(cat /sys/module/ath11k/parameters/frame_mode)
			;;
		"IPQ9574")
			freq="$(get_freq "$phy" "$channel" "$band")"
			board_type=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $3}' | awk -F- '{print $3}')

			if [ "$board_type" == "C6" ] && [ "$freq" -gt 2000 ] && [ "$freq" -lt 3000 ]; then
				wds_support=$(cat /sys/module/ath11k/parameters/frame_mode)
			fi
			;;
	esac

	echo "$wds_support"
}


mac80211_hostapd_setup_bss() {
	local phy="$1"
	local ifname="$2"
	local macaddr="$3"
	local type="$4"

	hostapd_cfg=
	append hostapd_cfg "$type=$ifname" "$N"

	hostapd_set_bss_options hostapd_cfg "$phy" "$vif" || return 1
	json_get_vars wds wds_bridge dtim_period max_listen_int start_disabled ieee80211w beacon_prot ppe_vp
	json_get_vars unsol_bcast_presp fils_discovery
	json_get_vars enable_epcs ttlm_enable enable_aal ml_max_rec_links enable_scs enable_mscs enable_dscp_policy_capa
	json_get_vars commitatf atfssidsched atfssidgroup

	#epcs params
	json_get_vars enable_epcs
	for param in $epcs_params; do
		json_get_vars "$param"
	done

	json_get_vars twt_responder

	set_default wds 0
	set_default start_disabled 0

	case "$auth_type" in
		psk|sae|psk-sae|owe|eap*|wep|sae-mixed|ft-sae-ext-key)
			if [ "$ieee80211w" -gt 0 ] && [ "$beacon_prot" -gt 0 ]; then
				append hostapd_cfg "beacon_prot=1" "$N"
			fi
		;;
	esac

	[ "$wds" -gt 0 ] && {
		wds_support=$(mac80211_wds_support_check "$phy")
                if [ "$wds_support" -ne 1 ]; then
                        echo WDS is supported only in native wifi mode for ath11k driver. Kindly update the config > /dev/ttyMSM0
                        return
                fi

		append hostapd_cfg "wds_sta=1" "$N"
		[ -n "$wds_bridge" ] && append hostapd_cfg "wds_bridge=$wds_bridge" "$N"
	}
	[ "$staidx" -gt 0 -o "$start_disabled" -eq 1 ] && append hostapd_cfg "start_disabled=1" "$N"

	if [ "$band" = "6g" ]; then
		fils_cfg=
		if [ "$unsol_bcast_presp" -gt 0 ] && [ "$unsol_bcast_presp" -le 20 ]; then
			append fils_cfg "unsol_bcast_probe_resp_interval=$unsol_bcast_presp" "$N"
		elif [ "$fils_discovery" -gt 0 ] && [ "$fils_discovery" -le 20 ]; then
			append fils_cfg "fils_discovery_max_interval=$fils_discovery" "$N"
		else
			append fils_cfg "fils_discovery_max_interval=20" "$N"
		fi

		if [ -n "$multiple_bssid" ] && [ "$multiple_bssid" -ge 1 ] && [ "$type" == "interface" ]; then
			append hostapd_cfg "$fils_cfg" "$N"
		elif [ -z "$multiple_bssid" ] || [ "$multiple_bssid" -eq 0 ]; then
			append hostapd_cfg "$fils_cfg" "$N"
		elif [ -n "$multiple_bssid" ] && [ "$multiple_bssid" -eq 3 ] && [ $((id % mbssid_group_size)) == "0" ]; then
			append hostapd_cfg "$fils_cfg" "$N"
		fi
        fi

	if [ -n "$enable_scs" ]; then
		append hostapd_cfg "enable_scs=$enable_scs" "$N"
	else
		append hostapd_cfg "enable_scs=1" "$N"
	fi

	if [ -n "$enable_mscs" ]; then
		append hostapd_cfg "enable_mscs=$enable_mscs" "$N"
	else
		append hostapd_cfg "enable_mscs=1" "$N"
	fi

	if [ -n "$enable_dscp_policy_capa" ]; then
		append hostapd_cfg "enable_dscp_policy_capa=$enable_dscp_policy_capa" "$N"
	fi

	if [[ "$htmode" == "EHT"* ]]; then
		append hostapd_cfg "mld_ap=1" "$N"

		if [ -n "$enable_epcs" ]; then
			append hostapd_cfg "enable_epcs=$enable_epcs" "$N"
		fi

		if [ "$enable_epcs" == "1" ]; then

			for param in $epcs_params; do
				eval "value=\$$param"
				if [ -n "$value" ]; then
					append hostapd_cfg "$param=$value" "$N"
				else
					append hostapd_cfg "$param=0" "$N"
				fi
			done
		fi
		if [ -n "$ttlm_enable" ]; then
			append hostapd_cfg "ttlm_enable=$ttlm_enable" "$N"
		else
			append hostapd_cfg "ttlm_enable=1" "$N"
		fi

		[ -n "$enable_aal" ] && append hostapd_cfg "enable_aal=$enable_aal" "$N"
		if [ "$ml_max_rec_links" -ge 0 ] && [ "$ml_max_rec_links" -le 3 ]; then
			append hostapd_cfg "ml_max_rec_links=$ml_max_rec_links" "$N"
		fi
	fi

	if [ -n "$twt_responder" ]; then
		append hostapd_cfg "twt_responder_caps=$twt_responder" "$N"
	fi

	case "$ppe_vp" in
		"passive")
			append hostapd_cfg "ppe_vp=1" "$N"
			;;
		"active")
			append hostapd_cfg "ppe_vp=2" "$N"
			;;
		"ds")
			append hostapd_cfg "ppe_vp=3" "$N"
			;;
		*)
			append hostapd_cfg "ppe_vp=3" "$N"
			;;
	esac

        [ -n "$commitatf" ] && append hostapd_cfg "commitatf=$commitatf" "$N"
        [ -n "$atfssidsched" ] && append hostapd_cfg "atfssidsched=$atfssidsched" "$N"
        [ -n "$atfssidgroup" ] && append hostapd_cfg "atfssidgroup=$atfssidgroup" "$N"

	cat >> /var/run/hostapd-$phy$vif_phy_suffix.conf <<EOF
$hostapd_cfg
bssid=$macaddr
${default_macaddr:+#default_macaddr}
${dtim_period:+dtim_period=$dtim_period}
${max_listen_int:+max_listen_interval=$max_listen_int}
EOF
}

mac80211_get_addr() {
	local phy="$1"
	local idx="$(($2 + 1))"

	head -n $idx /sys/class/ieee80211/${phy}/addresses | tail -n1
}

mac80211_generate_mac() {
	local phy="$1"
	local id="${macidx:-0}"
	local group_size
	local mode="$2"
	local mbssid=""
	if [ -n "mbssid_group_size" ]; then
		group_size="$mbssid_group_size"
	else
		group_size="$has_ap"
	fi

	if [ "$mode" = "ap" ]; then
		mbssid=$multiple_bssid
	fi

	wdev_tool "$phy$phy_suffix" get_macaddr id=$id num_global=$num_global_macaddr mbssid=$mbssid mbssid_group_size=$group_size
}

get_board_phy_name() (
	local path="$1"
	local fallback_phy=""

	__check_phy() {
		local val="$1"
		local key="$2"
		local ref_path="$3"

		json_select "$key"
		json_get_vars path
		json_select ..

		[ "${ref_path%+*}" = "$path" ] && fallback_phy=$key
		[ "$ref_path" = "$path" ] || return 0

		echo "$key"
		exit
	}

	json_load_file /etc/board.json
	json_for_each_item __check_phy wlan "$path"
	[ -n "$fallback_phy" ] && echo "${fallback_phy}.${path##*+}"
)

rename_board_phy_by_path() {
	local path="$1"

	local new_phy="$(get_board_phy_name "$path")"
	[ -z "$new_phy" -o "$new_phy" = "$phy" ] && return

	iw "$phy" set name "$new_phy" && phy="$new_phy"
}

rename_board_phy_by_name() (
	local phy="$1"
	local suffix="${phy##*.}"
	[ "$suffix" = "$phy" ] && suffix=

	json_load_file /etc/board.json
	json_select wlan
	json_select "${phy%.*}" || return 0
	json_get_vars path

	prev_phy="$(iwinfo nl80211 phyname "path=$path${suffix:++$suffix}")"
	[ -n "$prev_phy" ] || return 0

	[ "$prev_phy" = "$phy" ] && return 0

	iw "$prev_phy" set name "$phy"
)

find_phy() {
	[ -n "$phy" ] && {
		rename_board_phy_by_name "$phy"
		[ -d /sys/class/ieee80211/$phy ] && return 0
	}
	[ -n "$path" ] && {
		phy="$(iwinfo nl80211 phyname "path=$path")"
		[ -n "$phy" ] && {
			rename_board_phy_by_path "$path"
			return 0
		}
	}
	[ -n "$macaddr" ] && {
		for phy in $(ls /sys/class/ieee80211 2>/dev/null); do
			grep -i -q "$macaddr" "/sys/class/ieee80211/${phy}/macaddress" && {
				path="$(iwinfo nl80211 path "$phy")"
				rename_board_phy_by_path "$path"
				return 0
			}
		done
	}
	return 1
}

mac80211_check_ap() {
        has_ap=$((has_ap+1))
}

mac80211_get_band_name() {
	local radio_id=$1

	[ "$radio_id" = "-1" ] && return $band

	freq_range=$(iw ${phy} info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3, $6}')
	set -- $freq_range
	freq1=$1
	freq2=$2

	if [ "$freq1" -gt 2400 ] && [ "$freq2" -lt 2495 ]; then
		band_name=2g
	elif [ "$freq1" -gt 5100 ] && [ "$freq2" -lt 5900 ]; then
		if [ "$freq1" -gt 5100 ] && [ "$freq2" -lt 5400 ]; then
			band_name=5gl
		elif [ "$freq1" -gt 5400 ] && [ "$freq2" -lt 5900 ]; then
			band_name=5gh
		else
			band_name=5g
		fi
	elif [ "$freq1" -gt 5900 ] && [ "$freq2" -lt 7200 ]; then
		if [ "$freq1" -gt 5900 ] && [ "$freq2" -lt 6425 ]; then
			band_name=6gl
		elif [ "$freq1" -gt 6500 ] && [ "$freq2" -lt 7200 ]; then
			band_name=6gh
		else
			band_name=6g
		fi
	else
		band_name=invalid
	fi

	echo $band_name
}

mac80211_set_ifname() {
	local phy="$1"
	local prefix="$2"
	eval "ifname=\"$phy-$prefix\${idx_$prefix:-0}\"; idx_$prefix=\$((\${idx_$prefix:-0 } + 1))"
}

mac80211_prepare_vif() {
	ppe_vp="ds"
	json_select config

	json_get_vars ifname mode ssid wds powersave macaddr enable wpa_psk_file vlan_file ppe_vp mld

	[ -n "$ifname" ] || {
                if [ "$is_wiphy_multi_radio" -eq 1 ]; then
                        if [[ "$htmode" == EHT* ]] && [ -n "$mld" ]; then
				config_get mld_ifname "$mld" ifname
				if [ -z "$mld_ifname" ]; then
					ifname=$phy-$mld
				else
					ifname="$mld_ifname"
				fi
                        else
				mac80211_set_ifname "$phy$vif_phy_suffix"
                        fi
                else
                        mac80211_set_ifname "$phy$vif_phy_suffix"
                fi

	}

	[ -z "$mld" ] || {
		uci set wireless.${mld}.ifname=$ifname
		uci commit wireless
	}

	[ -z $ppe_vp ] && ppe_vp="ds"

        if [ $mode == "mesh" ] && [ $ppe_vp == "ds" ]; then
                ppe_vp="passive"
        fi
        if [ $mode == "monitor" ]; then
                 append mon_ifname "$ifname"
        fi

	append active_ifnames "$ifname"
	set_default wds 0
	set_default powersave 0
	json_add_string _ifname "$ifname"
	json_add_string _ppe_vp "$ppe_vp"

	default_macaddr=
	if [ -z "$macaddr" ]; then
		macaddr="$(mac80211_generate_mac $phy $mode)"
		macidx="$(($macidx + 1))"
		#default_macaddr=1
	elif [ "$macaddr" = 'random' ]; then
		macaddr="$(macaddr_random)"
	fi
	json_add_string _macaddr "$macaddr"
	json_add_string _default_macaddr "$default_macaddr"
	json_select ..

	[ -f /tmp/mlo_support.txt ] && mlo_add_flag=$(cat /tmp/mlo_support.txt)
	if [ $mlo_add_flag -eq 0 ]; then
		[ "$mode" == "ap" ] && {
			[ -z "$wpa_psk_file" ] && hostapd_set_psk "$ifname"
			[ -z "$vlan_file" ] && hostapd_set_vlan "$ifname"
		}
	fi

	json_select config

	# It is far easier to delete and create the desired interface
	case "$mode" in
		ap)
			# Hostapd will handle recreating the interface and
			# subsequent virtual APs belonging to the same PHY
			if [ -n "$hostapd_ctrl" ]; then
				type=bss
			else
				type=interface
			fi

			mac80211_hostapd_setup_bss "$phy" "$ifname" "$macaddr" "$type" || return

			[ -n "$dpp" ] && append dpp_ifaces $ifname

			[ -n "$hostapd_ctrl" ] || {
				ap_ifname="${ifname}"
				hostapd_ctrl="${hostapd_ctrl:-/var/run/hostapd/$ifname}"
			}
		;;
	esac

	json_select ..
}

mac80211_prepare_iw_htmode() {
	case "$htmode" in
		VHT20|HT20|HE20) iw_htmode=HT20;;
		HT40*|VHT40|VHT160|HE40)
			case "$band" in
				2g)
					case "$htmode" in
						HT40+) iw_htmode="HT40+";;
						HT40-) iw_htmode="HT40-";;
						*)
							if [ "$channel" -lt 7 ]; then
								iw_htmode="HT40+"
							else
								iw_htmode="HT40-"
							fi
						;;
					esac
				;;
				*)
					case "$(( ($channel / 4) % 2 ))" in
						1) iw_htmode="HT40+" ;;
						0) iw_htmode="HT40-";;
					esac
				;;
			esac
			[ "$auto_channel" -gt 0 ] && iw_htmode="HT40+"
		;;
		VHT80|HE80)
			iw_htmode="80MHZ"
		;;
		NONE|NOHT)
			iw_htmode="NOHT"
		;;
		*) iw_htmode="" ;;
	esac
}

mac80211_add_mesh_params() {
	for var in $MP_CONFIG_INT $MP_CONFIG_BOOL $MP_CONFIG_STRING; do
		eval "mp_val=\"\$$var\""
		[ -n "$mp_val" ] && json_add_string "$var" "$mp_val"
	done
}

mac80211_setup_adhoc() {
	local enable=$1
	json_get_vars bssid ssid key mcast_rate

	NEWUMLIST="${NEWUMLIST}$ifname "

	[ "$enable" = 0 ] && {
		ip link set dev "$ifname" down
		return 0
	}

	keyspec=
	[ "$auth_type" = "wep" ] && {
		set_default key 1
		case "$key" in
			[1234])
				local idx
				for idx in 1 2 3 4; do
					json_get_var ikey "key$idx"

					[ -n "$ikey" ] && {
						ikey="$(($idx - 1)):$(prepare_key_wep "$ikey")"
						[ $idx -eq $key ] && ikey="d:$ikey"
						append keyspec "$ikey"
					}
				done
			;;
			*)
				append keyspec "d:0:$(prepare_key_wep "$key")"
			;;
		esac
	}

	brstr=
	for br in $basic_rate_list; do
		wpa_supplicant_add_rate brstr "$br"
	done

	mcval=
	[ -n "$mcast_rate" ] && wpa_supplicant_add_rate mcval "$mcast_rate"

	local prev
	json_set_namespace wdev_uc prev

	json_add_object "$ifname"
	json_add_string mode adhoc
	[ -n "$default_macaddr" ] || json_add_string macaddr "$macaddr"
	json_add_string ssid "$ssid"
	json_add_string freq "$freq"
	json_add_string htmode "$iw_htmode"
	[ -n "$bssid" ] && json_add_string bssid "$bssid"
	json_add_int beacon-interval "$beacon_int"
	[ -n "$brstr" ] && json_add_string basic-rates "$brstr"
	[ -n "$mcval" ] && json_add_string mcast-rate "$mcval"
	[ -n "$keyspec" ] && json_add_string keys "$keyspec"
	json_close_object

	json_set_namespace "$prev"
}

mac80211_setup_mesh() {
	json_get_vars ssid mesh_id mcast_rate

	mcval=
	[ -n "$mcast_rate" ] && wpa_supplicant_add_rate mcval "$mcast_rate"
	[ -n "$mesh_id" ] && ssid="$mesh_id"

	local prev
	json_set_namespace wdev_uc prev

	json_add_object "$ifname"
	json_add_string mode mesh
	[ -n "$default_macaddr" ] || json_add_string macaddr "$macaddr"
	json_add_string ssid "$ssid"
	json_add_string freq "$freq"
	json_add_string htmode "$iw_htmode"
	[ -n "$mcval" ] && json_add_string mcast-rate "$mcval"
	json_add_int beacon-interval "$beacon_int"
	json_add_string ru-puncturing-bitmap "$ru_punct_bitmap"
	mac80211_add_mesh_params

	json_close_object

	json_set_namespace "$prev"
}

mac80211_get_seg0() {
        local ht_mode="$1"
        local seg0=0

        case "$ht_mode" in
                40)
                        if [ "$freq" -gt 5950 ] && [ "$freq" -le 7115 ]; then
                                case "$(( (channel / 4) % 2 ))" in
                                        1) seg0=$((channel - 2));;
                                        0) seg0=$((channel + 2));;
                                esac
                        elif [ "$freq" -lt 2484 ]; then
                                if [ "$channel" -lt 7 ]; then
                                        seg0=$((channel + 2))
                                else
                                        seg0=$((channel - 2))
                                fi
                        elif [ "$freq" != 5935 ]; then
                                case "$(( (channel / 4) % 2 ))" in
                                        1) seg0=$((channel + 2));;
                                        0) seg0=$((channel - 2));;
                                esac
                        fi
                ;;
		80)
                        if [ "$freq" -gt 5950 ] && [ "$freq" -le 7115 ]; then
                                case "$(( (channel / 4) % 4 ))" in
                                        0) seg0=$((channel + 6));;
                                        1) seg0=$((channel + 2));;
                                        2) seg0=$((channel - 2));;
                                        3) seg0=$((channel - 6));;
                                esac
                        elif [ "$freq" != 5935 ]; then
                                case "$(( (channel / 4) % 4 ))" in
                                        1) seg0=$((channel + 6));;
                                        2) seg0=$((channel + 2));;
                                        3) seg0=$((channel - 2));;
                                        0) seg0=$((channel - 6));;
                                esac
                        fi
                ;;
		160)
                        if [ "$freq" -gt 5950 ] && [ "$freq" -le 7115 ]; then
                                case "$channel" in
                                        1|5|9|13|17|21|25|29) seg0=15;;
                                        33|37|41|45|49|53|57|61) seg0=47;;
                                        65|69|73|77|81|85|89|93) seg0=79;;
                                        97|101|105|109|113|117|121|125) seg0=111;;
                                        129|133|137|141|145|149|153|157) seg0=143;;
                                        161|165|169|173|177|181|185|189) seg0=175;;
                                        193|197|201|205|209|213|217|221) seg0=207;;
                                esac
                        elif [ "$freq" != 5935 ]; then
                                case "$channel" in
                                        36|40|44|48|52|56|60|64) seg0=50;;
                                        100|104|108|112|116|120|124|128) seg0=114;;
                                        149|153|157|161|165|169|173|177) seg0=163;;
                                esac
                        fi
                ;;
		320)
                        if [ "$freq" -ge 5955 ] && [ "$freq" -le 7115 ]; then
                                case "$channel" in
                                        1|5|9|13|17|21|25|29|33|37|41|45) seg0=31;;
                                        49|53|57|61|65|69|73|77) seg0=63;;
                                        81|85|89|93|97|101|105|109) seg0=95;;
                                        113|117|121|125|129|133|137|141) seg0=127;;
                                        145|149|153|157|161|165|169|173) seg0=159;;
                                        177|181|185|189|193|197|201|205|209|213|217|221) seg0=191;;
                                esac
                        elif [ "$freq" -ge 5500 ] && [ "$freq" -le 5730 ]; then
                                seg0=130
                        fi
                ;;
                esac
                printf "$seg0"
}

get_seg0_freq() {
        local ctrl_freq="$1"
        local ctrl_chan="$2"
        local seg0_chan="$3"

        if [ $((seg0_chan)) -gt $((ctrl_chan)) ]; then
                printf $(($ctrl_freq + (($seg0_chan - $ctrl_chan) * 5)))
        else
                printf $(($ctrl_freq - (($ctrl_chan - $seg0_chan) * 5)))
        fi
}

mac80211_setup_monitor() {
	local prev idx
	json_set_namespace wdev_uc prev

	json_add_object "$ifname"
	json_add_string mode monitor
	[ -n "$freq" ] && json_add_string freq "$freq"
	json_add_string htmode "$htmode"
	case "$htmode" in
                VHT20|HT20|HE20|EHT20)
                        bw=20
                        ;;
                HT40*|VHT40|HE40|EHT40)
                        bw=40
			center_freq=$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 40)")
                        ;;
                VHT80|HE80|EHT80)
                        bw=80
                        center_freq=$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 80)")
                        ;;
                VHT160|HE160|EHT160)
                        bw=160
                        center_freq=$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 160)")
                        ;;
		EHT320)
			if [ "$band" = "6g" ]; then
				bw=320
				if [ -n "$ccfs" ] && [ "$ccfs" -gt 0 ]; then
					idx="$ccfs"
				elif [ -z "$ccfs" ] || [ "$ccfs" -eq "0" ]; then
					idx="$(mac80211_get_seg0 "320")"
				fi
				center_freq=$(get_seg0_freq "$freq" "$channel" "$idx")
			fi
			;;
        esac

        if [ $is_wiphy_multi_radio -eq 1 ]; then
                json_add_boolean is_multi_radio 1
                [ -n "$center_freq" ] && json_add_string center_freq "$center_freq"
                json_add_string bw "$bw"
        fi
	json_close_object

	json_set_namespace "$prev"
}

get_link_id() {
	local target_mac_addr=$1
	local target_ifname=$2
	local link_ids
	local link_mac

	if [ -z "$target_mac_addr" ]; then
		echo ""
		return
	fi

	if [ -z "$target_ifname" ]; then
		echo ""
		return
	fi

	link_ids=$(iw dev $target_ifname info | grep link | cut -d':' -f 1 2> /dev/null  | cut -d ' ' -f 2)
	if [ -n "$link_ids" ]; then
		for i in $link_ids
		do
			link_mac=$(iw dev $target_ifname info | grep -A 2 "link $i:" | grep "addr" | awk '{print $2}')
			if [ "$link_mac" == "$target_mac_addr" ]; then
				echo "$i"
				return
			fi
		done
	fi

	echo ""
}

mac80211_set_vif_txpower() {
	local name="$1"

	json_select config
	json_get_var ifname _ifname
	json_get_var ppe_vp _ppe_vp
	json_get_vars vif_txpower
	json_select ..

	set_default vif_txpower "$txpower"

	local link_id=""
	case "$htmode" in
		EHT20|EHT40|EHT80|EHT160|EHT320)
			json_select config
			json_get_var link_mac_addr _macaddr
			json_select ..

			link_id=$(get_link_id "$link_mac_addr" "$ifname")
		;;
	esac

	if [ -n "$vif_txpower" ]; then
		if [ -n "$link_id" ]; then
			iw dev "$ifname" set txpower -l "$link_id" fixed "${vif_txpower%%.*}00"
		else
			iw dev "$ifname" set txpower fixed "${vif_txpower%%.*}00"
		fi
	else
		if [ -n "$link_id" ]; then
			iw dev "$ifname" set txpower -l "$link_id" auto
		else
			iw dev "$ifname" set txpower auto
		fi
	fi

	iw dev "$ifname" set_intf_offload type "$ppe_vp"
}

mac80211_set_fq_limit() {
        json_select data
        json_get_vars ifname
        json_select ..

        json_select config
        json_get_vars fq_limit

        if [ "$fq_limit" -gt 0 ]; then
                tc qdisc add dev "$ifname" parent :1 fq_codel limit "$fq_limit"
                tc qdisc add dev "$ifname" parent :2 fq_codel limit "$fq_limit"
                tc qdisc add dev "$ifname" parent :3 fq_codel limit "$fq_limit"
                tc qdisc add dev "$ifname" parent :4 fq_codel limit "$fq_limit"
        fi
        json_select ..
}

wpa_supplicant_init_config() {
	json_set_namespace wpa_supp prev

	json_init
	json_add_array config

	json_set_namespace "$prev"
}

wpa_supplicant_add_interface() {
	local ifname="$1"
	local mode="$2"
	local prev

	_wpa_supplicant_common "$ifname"

	json_set_namespace wpa_supp prev

	json_add_object
	json_add_string ctrl "$_rpath"
	json_add_string iface "$ifname"
	json_add_string mode "$mode"
	json_add_int radio "$radio"
	if [ "$mode" = "sta" ]; then
		[ -n "$mld" ] && {
			json_add_string mld "$mld"
		}
	fi

	json_add_string config "$_config"
	[ -n "$default_macaddr" ] || json_add_string macaddr "$macaddr"
	[ -n "$network_bridge" ] && json_add_string bridge "$network_bridge"
	[ -n "$wds" ] && json_add_boolean 4addr "$wds"
	json_add_boolean powersave "$powersave"
	[ "$mode" = "mesh" ] && mac80211_add_mesh_params
	json_close_object

	json_set_namespace "$prev"

	wpa_supp_init=1
}

wpa_supplicant_set_config() {
	local phy="$1"
	local radio="$2"
	local mon_if_name="$3"
	local prev

	json_set_namespace wpa_supp prev
	json_close_array
	json_add_string phy "$phy"
	json_add_int radio "$radio"
	json_add_int num_global_macaddr "$num_global_macaddr"
	json_add_boolean defer 1
	[ -n "$mld" ] && json_add_boolean is_ml 1
	json_add_string mon_if_name "$mon_if_name"
	local data="$(json_dump)"

	json_cleanup
	json_set_namespace "$prev"

	ubus -S -t 0 wait_for wpa_supplicant || {
		[ -n "$wpa_supp_init" ] || return 0

		ubus wait_for wpa_supplicant
	}

	local supplicant_res="$(ubus_call wpa_supplicant config_set "$data")"
	ret="$?"
	[ "$ret" != 0 -o -z "$supplicant_res" ] && wireless_setup_vif_failed WPA_SUPPLICANT_FAILED

	wireless_add_process "$(jsonfilter -s "$supplicant_res" -l 1 -e @.pid)" "/usr/sbin/wpa_supplicant" 1 1
}

hostapd_set_config() {
	local phy="$1"
	local radio="$2"

	[ -n "$hostapd_ctrl" ] || {
		ubus_call hostapd config_set '{ "phy": "'"$phy"'", "radio": '"$radio"', "config": "", "prev_config": "'"${hostapd_conf_file}.prev"'" }' > /dev/null
		return 0;
	}

	ubus wait_for hostapd
	local hostapd_res="$(ubus_call hostapd config_set "{ \"phy\": \"$phy\", \"radio\": $radio, \"config\":\"${hostapd_conf_file}\", \"prev_config\": \"${hostapd_conf_file}.prev\"}")"
	ret="$?"
	[ "$ret" != 0 -o -z "$hostapd_res" ] && {
		wireless_setup_failed HOSTAPD_START_FAILED
		return
	}
	wireless_add_process "$(jsonfilter -s "$hostapd_res" -l 1 -e @.pid)" "/usr/sbin/hostapd" 1 1
}


wpa_supplicant_start() {
	local phy="$1"
	local radio="$2"
	local is_mld="false"

	[ -n "$wpa_supp_init" ] || return 0
	[ -n "$mld" ] && is_mld="true"

	ubus_call wpa_supplicant config_set '{ "phy": "'"$phy"'", "radio": '"$radio"', "num_global_macaddr": '"$num_global_macaddr"', "is_ml": '"$is_mld"' }' > /dev/null
	if [ "${dpp}" -eq 1 ]; then
		/usr/sbin/wpa_cli -i $ifname -p /var/run/wpa_supplicant -a /lib/netifd/dpp-supplicant-event-update -B
	fi
}

mac80211_setup_supplicant() {
	local enable=$1
	local add_sp=0

	wpa_supplicant_prepare_interface "$ifname" nl80211 || return 1

	if [ "$mode" = "sta" ]; then
		wpa_supplicant_add_network "$ifname"
	else
		wpa_supplicant_add_network "$ifname" "$freq" "$htmode" "$hostapd_noscan" "$ru_punct_bitmap" "$disable_csa_dfs" "$ccfs"
	fi

	wpa_supplicant_add_interface "$ifname" "$mode"

	return 0
}

mac80211_setup_vif() {
	local name="$1"
	local failed

	json_select config
	json_get_var ifname _ifname
	json_get_var macaddr _macaddr
	json_get_var default_macaddr _default_macaddr
	json_get_vars mode wds powersave mld

	set_default powersave 0
	set_default wds 0

	case "$mode" in
		mesh)
			freq_list=$(get_sta_freq_list "$phy" "$radio" "$band_name")
			json_get_vars $MP_CONFIG_INT $MP_CONFIG_BOOL $MP_CONFIG_STRING
			wireless_vif_parse_encryption
			[ -z "$htmode" ] && htmode="NOHT";
			if wpa_supplicant -vmesh; then
				mac80211_setup_supplicant || failed=1
			else
				mac80211_setup_mesh
			fi
		;;
		adhoc)
			wireless_vif_parse_encryption
			if [ "$wpa" -gt 0 -o "$auto_channel" -gt 0 ]; then
				mac80211_setup_supplicant || failed=1
			else
				mac80211_setup_adhoc
			fi
		;;
		sta)
			if [ -n "$mld" ]; then
				for value in $_sta_radios
				do
					bname=$(mac80211_get_band_name $value)
					flist=$(get_sta_freq_list "$phy" "$value" "$bname")
					append freq_list "$flist"
				done
				freq_list=$freq_list
			else
				freq_list=$(get_sta_freq_list "$phy" "$radio" "$band_name")
			fi
			mac80211_setup_supplicant || failed=1
		;;
		monitor)
			mac80211_setup_monitor
		;;
	esac

	json_select ..
	[ -n "$failed" ] || wireless_add_vif "$name" "$ifname"
}

get_freq() {
	local phy="$1"
	local channel="$2"
	local band="$3"

	case "$band" in
		2g) band="1:";;
		5g) band="2:";;
		60g) band="3:";;
		6g) band="4:";;
	esac

	iw "$phy" info | awk -v band="$band" -v channel="[$channel]" '

$1 ~ /Band/ {
	band_match = band == $2
}

band_match && $3 == "MHz" && $4 == channel {
	print int($2)
	exit
}
'
}

chan_is_dfs() {
	local phy="$1"
	local chan="$2"
	iw "$phy" info | grep -E -m1 "(\* ${chan:-....} MHz${chan:+|\\[$chan\\]})" | grep -q "MHz.*radar detection"
	return $!
}

mac80211_set_noscan() {
	hostapd_noscan=1
}

drv_mac80211_cleanup() {
	:
}

mac80211_reset_config() {
	hostapd_conf_file="/var/run/hostapd-$phy$vif_phy_suffix.conf"
	wdev_tool "$phy$phy_suffix" set_config '{}'
	ubus_call hostapd config_set '{ "phy": "'"$phy"'", "radio": '"$radio"', "config": "", "prev_config": "'"$hostapd_conf_file"'" }' > /dev/null
	ubus_call wpa_supplicant config_set '{ "phy": "'"$phy"'", "radio": '"$radio"', "config": [] }' > /dev/null
}

mac80211_set_suffix() {

	band_name=$(mac80211_get_band_name $radio)

	[ "$radio" = "-1" ] && radio=
	phy_suffix="${radio:+:$radio}"
	vif_phy_suffix="${radio:+.$radio}"
	set_default radio -1
}

drv_mac80211_setup() {
	local device=$1

	mac80211_derive_ml_info

	json_select config
	json_get_vars \
		radio phy macaddr path \
		country chanbw distance band\
		txpower \
		rxantenna txantenna antenna_gain\
		frag rts beacon_int:100 htmode \
		num_global_macaddr:1 multiple_bssid \
		eht_ulmumimo_80mhz eht_ulmumimo_160mhz eht_ulmumimo_320mhz \
		ccfs disable_csa_dfs ru_punct_bitmap
	json_get_values basic_rate_list basic_rate
	json_get_values scan_list scan_list
	json_select ..

	if [ ${#device} -eq 12 ]; then
		is_wiphy_multi_radio=1
	fi

	if [ "$is_wiphy_multi_radio" -eq 1 ]; then
		echo [debug] $device - radio as $radio > /tmp/logs
        else
                radio=-1
        fi

	[ -f /tmp/mlo_support.txt ] && mlo_add_flag=$(cat /tmp/mlo_support.txt)
	if [ $mlo_add_flag -eq 0 ]; then
		json_select data && {
			json_get_var prev_rxantenna rxantenna
			json_get_var prev_txantenna txantenna
			json_select ..
		}
	fi

	find_phy || {
		echo "Could not find PHY for device '$1'"
		wireless_set_retry 1
		return 1
	}

	mac80211_set_suffix

	[ -f /tmp/mlo_support.txt ] && mlo_add_flag=$(cat /tmp/mlo_support.txt)
	if [ $mlo_add_flag -eq 0 ]; then
		if [ "$(cat /sys/module/ath12k/parameters/ppe_rfs_support)" == 'Y' ]; then
			# Note: ppe_vp_accel and ppe_vp_rfs are mutually exclusive.
			#       ppe_vp_accel enables PPE acceleration path and ppe_vp_rfs
			#       is expected to enable only flow steering for VLAN type
			#       interface (eg: WDS root).
			echo 1 >> /sys/module/mac80211/parameters/ppe_vp_rfs
			# Note: Format is default MLO mask followed by band specific core masks
			#       in order of 2 GHz, 5 GHz and 6GHz bands
			#       echo <DEFAULT/ MLO MASK>,<2GHZ MASK>,<5GHZ MASK>,<6GHZ_MASK>
			echo 0x7,0x7,0x7,0x7 > /sys/module/ath12k/parameters/rfs_core_mask

			if [ "$(cat /sys/module/mac80211/parameters/ppe_vp_accel)" == 'Y' ]; then
				echo "ppe_vp_accel is enabled. Please disable to support RFS on WDS" > /dev/ttyMSM0
			fi

			if echo "$(cat /sys/sfe/ppe_rfs_feature)" | grep -q "disabled"; then
				echo 1 >> /sys/sfe/ppe_rfs_feature
				echo "enabled ppe_rfs_feature" > /dev/ttyMSM0
			fi
		fi
	fi

	local wdev
	local cwdev
	local found

	# convert channel to frequency
	[ "$auto_channel" -gt 0 ] || freq="$(get_freq "$phy" "$channel" "$band")"

	if [ $mlo_add_flag -eq 0 ]; then
		[ -n "$country" ] && {
			iw reg get | grep -q "^country $country:" || {
				iw reg set "$country"
				sleep 1
			}
			if [ "$country" = "00" ]; then
				iw reg set "$country"
				sleep 1
			fi
		}
	fi

	Update_channel_list() {
		local start_freq end_freq start_chan end_chan
		local device_name=$1
		local radio=$(uci get wireless.$device_name.radio)
		start_chan=0
		end_chan=0
		#Fetch all the radio names and iterate for each radio to update channel list
		start_freq=$(iw $phy info | grep -A 2 "Idx $radio:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $phy info | grep -A 2 "Idx $radio:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		if [ "$start_chan" != "0" ] && [ "$end_chan" != "0" ]; then
			uci set wireless.$device_name.channels=$start_chan-$end_chan
			uci commit wireless
		fi
	}

	config_foreach Update_channel_list wifi-device
	updated_chanlist=$(uci get wireless.$device.channels)
	hostapd_conf_file="/var/run/hostapd-$phy$vif_phy_suffix.conf"

	macidx=0
	staidx=0

	if [ $mlo_add_flag -eq 0 ]; then
		[ -n "$chanbw" ] && {
			for file in /sys/kernel/debug/ieee80211/$phy/ath9k*/chanbw /sys/kernel/debug/ieee80211/$phy/ath5k/bwmode; do
				[ -f "$file" ] && echo "$chanbw" > "$file"
			done
		}

		set_default rxantenna 0xffffffff
		set_default txantenna 0xffffffff
		set_default distance 0

		[ "$txantenna" = "all" ] && txantenna=0xffffffff
		[ "$rxantenna" = "all" ] && rxantenna=0xffffffff

		[ "$rxantenna" = "$prev_rxantenna" -a "$txantenna" = "$prev_txantenna" ] || mac80211_reset_config "$phy"
		wireless_set_data phy="$phy" radio="$radio" txantenna="$txantenna" rxantenna="$rxantenna"

		# Use the legacy command for non multi radio chipset
		if [ "$is_wiphy_multi_radio" -eq 0 ]; then
			iw phy "$phy" set antenna $txantenna $rxantenna >/dev/null 2>&1
		else
			iw phy "$phy" set antenna radio $radio "$txantenna" "$rxantenna" >/dev/null 2>&1
		fi
		iw phy "$phy" set distance "$distance" >/dev/null 2>&1

		[ -n "$frag" ] && iw phy "$phy" set frag "${frag%%.*}"
		[ -n "$rts" ] && iw phy "$phy" set rts "${rts%%.*}"
	fi

	has_ap=
	hostapd_ctrl=
	ap_ifname=
	hostapd_noscan=
	wpa_supp_init=
	for_each_interface "ap" mac80211_check_ap

	[ -f "$hostapd_conf_file" ] && mv "$hostapd_conf_file" "$hostapd_conf_file.prev"

	for_each_interface "sta adhoc mesh" mac80211_set_noscan
	[ -n "$has_ap" ] && mac80211_hostapd_setup_base "$phy"

        if [ "$mlo_add_flag" = 1 ]; then
		for_each_interface "ap" mac80211_prepare_vif
                return;
        fi

	local prev
	json_set_namespace wdev_uc prev
	json_init
	json_set_namespace "$prev"

	wpa_supplicant_init_config

	mac80211_prepare_iw_htmode
	active_ifnames=
	mon_ifname=

	for_each_interface "ap" mac80211_prepare_vif
	for_each_interface "sta adhoc mesh monitor" mac80211_prepare_vif
	for_each_interface "ap sta adhoc mesh monitor" mac80211_setup_vif

	echo 3 > /sys/kernel/debug/ecm/ecm_classifier_default/accel_delay_pkts

	[ -x /usr/sbin/wpa_supplicant ] && wpa_supplicant_set_config "$phy" "$radio" "$mon_ifname"
	[ -x /usr/sbin/hostapd ] && hostapd_set_config "$phy" "$radio"

	[ -x /usr/sbin/wpa_supplicant ] && wpa_supplicant_start "$phy" "$radio"

	json_set_namespace wdev_uc prev
	wdev_tool "$phy$phy_suffix" set_config "$(json_dump)" $active_ifnames
	json_set_namespace "$prev"

	for_each_interface "ap sta adhoc mesh monitor" mac80211_set_vif_txpower

	config_get enable_smp_affinity mac80211 enable_smp_affinity 0
	if [ "$enable_smp_affinity" -eq 1 ]; then
		[ -f "/lib/smp_affinity_settings.sh" ] && {
			. /lib/smp_affinity_settings.sh
			enable_smp_affinity_wifi
		}
		[ -f "/lib/update_smp_affinity.sh" ] && {
			. /lib/update_smp_affinity.sh
			enable_smp_affinity_wigig
		}
	fi

	[ -f "/lib/performance.sh" ] && {
		. /lib/performance.sh
	}

	hostapd_dpp_action "$dpp_ifaces"

	for_each_interface "ap mesh" mac80211_set_fq_limit
	wireless_set_up
}

_list_phy_interfaces() {
	local phy="$1"
	if [ -d "/sys/class/ieee80211/${phy}/device/net" ]; then
		ls "/sys/class/ieee80211/${phy}/device/net" 2>/dev/null;
	else
		ls "/sys/class/ieee80211/${phy}/device" 2>/dev/null | grep net: | sed -e 's,net:,,g'
	fi
}

list_phy_interfaces() {
	local phy="$1"

	for dev in $(_list_phy_interfaces "$phy"); do
		readlink "/sys/class/net/${dev}/phy80211" | grep -q "/${phy}\$" || continue
		echo "$dev"
	done
}

drv_mac80211_teardown() {
	json_select data
	json_get_vars phy radio
	json_select ..
	[ -n "$phy" ] || {
		echo "Bug: PHY is undefined for device '$1'"
		return 1
	}

	mac80211_set_suffix
	mac80211_reset_config "$phy"
}

_sta_radios=
mac80211_derive_ml_info() {
	local _mlds
	local sta_mldevices

	config_load wireless

	mac80211_get_wifi_mlds() {
		append _mlds "$1"
	}
	config_foreach mac80211_get_wifi_mlds wifi-mld

	if [ -z "$_mlds" ]; then
		return
	fi

	mac80211_get_wifi_ifaces() {
	        config_get iface_mode "$1" mode
	        if [ -n "$iface_mode" ] && [ "$iface_mode" = "sta" ]; then
	                 append _staifaces "$1"
	        fi
	}
	config_foreach mac80211_get_wifi_ifaces wifi-iface

	if [ -z "$_staifaces" ]; then
	        return
	fi

	for _mld in $_mlds
	do
	        for _staifname in $_staifaces
	        do
	                config_get mld_name "$_staifname" mld
	                config_get mldevice "$_staifname" device
	                if [ ${#mldevice} -ne 12 ]; then
	                        continue;
	                fi

	                if ! [[ "$sta_mldevices" =~ "$mldevice" ]]; then

	                        if [ -n "$mld_name" ] &&  [ "$_mld" = "$mld_name" ]; then
	                        	append sta_mldevices "$mldevice"
	                                config_get disabled "$mldevice" disabled
	                                if [ "$disabled" -eq 1 ]; then
	                                        continue;
	                                fi
	                                config_get radio "$mldevice" radio
	                                append _sta_radios $radio
	                        fi
	                fi
	        done
	done
}


add_driver mac80211
