#!/bin/sh
. /lib/netifd/netifd-wireless.sh
. /lib/netifd/hostapd.sh
. /lib/functions/system.sh
[ -e /lib/functions.sh ] && . /lib/functions.sh

init_wireless_driver "$@"
MLD_VAP_DETAILS="/lib/netifd/wireless/wifi_mld_cfg.config"

MP_CONFIG_INT="mesh_retry_timeout mesh_confirm_timeout mesh_holding_timeout mesh_max_peer_links
	       mesh_max_retries mesh_ttl mesh_element_ttl mesh_hwmp_max_preq_retries
	       mesh_path_refresh_time mesh_min_discovery_timeout mesh_hwmp_active_path_timeout
	       mesh_hwmp_preq_min_interval mesh_hwmp_net_diameter_traversal_time mesh_hwmp_rootmode
	       mesh_hwmp_rann_interval mesh_gate_announcements mesh_sync_offset_max_neighor
	       mesh_rssi_threshold mesh_hwmp_active_path_to_root_timeout mesh_hwmp_root_interval
	       mesh_hwmp_confirmation_interval mesh_awake_window mesh_plink_timeout"
MP_CONFIG_BOOL="mesh_auto_open_plinks mesh_fwding"
MP_CONFIG_STRING="mesh_power_mode"

NEWAPLIST=
OLDAPLIST=
NEWSPLIST=
OLDSPLIST=
NEWUMLIST=
OLDUMLIST=

drv_mac80211_init_device_config() {
	hostapd_common_add_device_config

	config_add_string path phy 'macaddr:macaddr'
	config_add_string tx_burst
	config_add_string distance band
	config_add_int beacon_int chanbw frag rts
	config_add_int rxantenna txantenna antenna_gain txpower min_tx_power
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
		multiple_bssid \
		ema \
		ru_punct_ofdma \
		use_ru_puncture_dfs \
	config_add_int \
		beamformer_antennas \
		beamformee_antennas \
		vht_max_a_mpdu_len_exp \
		vht_max_mpdu \
		vht_link_adapt \
		vht160 \
		he_ul_mumimo \
		he_ul_mumimo \
		eht_ulmumimo_160mhz \
		eht_ulmumimo_320mhz \
		rx_stbc \
		tx_stbc \
		he_bss_color \
		he_spr_non_srg_obss_pd_max_offset \
		ru_punct_bitmap \
		ru_punct_acs_threshold \
		ccfs
	config_add_boolean \
		ldpc \
		greenfield \
		short_gi_20 \
		short_gi_40 \
		max_amsdu \
		disable_eml_cap \
		dsss_cck_40
}

drv_mac80211_init_iface_config() {
	hostapd_common_add_bss_config

	config_add_string 'macaddr:macaddr' ifname mld

	config_add_boolean wds powersave enable
	config_add_string wds_bridge
	config_add_int maxassoc
	config_add_int max_listen_int
	config_add_int dtim_period
	config_add_int start_disabled

	# mesh
	config_add_string mesh_id
	config_add_int $MP_CONFIG_INT
	config_add_boolean $MP_CONFIG_BOOL
	config_add_string $MP_CONFIG_STRING
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

mac80211_hostapd_setup_base() {
	local phy="$1"

	json_select config

	[ "$auto_channel" -gt 0 ] && channel=acs_survey

	[ "$auto_channel" -gt 0 ] && json_get_vars acs_exclude_dfs
	[ -n "$acs_exclude_dfs" ] && [ "$acs_exclude_dfs" -gt 0 ] &&
		append base_cfg "acs_exclude_dfs=1" "$N"

	json_get_vars noscan ht_coex min_tx_power:0 tx_burst use_ru_puncture_dfs
	json_get_values ht_capab_list ht_capab
	json_get_values channel_list channels

	[ "$auto_channel" = 0 ] && [ -z "$channel_list" ] && \
		channel_list="$channel"

	[ "$min_tx_power" -gt 0 ] && append base_cfg "min_tx_power=$min_tx_power"

	set_default noscan 0

	[ "$noscan" -gt 0 ] && hostapd_noscan=1
	[ "$tx_burst" = 0 ] && tx_burst=

	chan_ofs=0
	[ "$band" = "6g" ] && chan_ofs=1

	ieee80211n=1
	ht_capab=
	case "$htmode" in
		VHT20|HT20|HE20) ;;
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
		for cap in $(iw phy "$phy" info | grep 'Capabilities:' | cut -d: -f2); do
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

	# 802.11ac
	enable_ac=0
	vht_oper_chwidth=0
	eht_oper_chwidth=0
	vht_center_seg0=
	eht_center_seg0=
	is_6ghz=0
	if [ -n "$band" ] && [ "$band" = "6g" ]; then
		is_6ghz=1
	fi
	idx="$channel"
	case "$htmode" in
		VHT20|HE20|EHT20) enable_ac=1;;
		VHT40|HE40|EHT40)
			case "$(( (($channel / 4) + $chan_ofs) % 2 ))" in
				1) idx=$(($channel + 2));;
				0) idx=$(($channel - 2));;
			esac
			enable_ac=1
			vht_center_seg0=$idx
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
			eht_oper_chwidth=1
			vht_center_seg0=$idx
			eht_center_seg0=$idx
		;;
		VHT160|HE160|EHT160)
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
				esac
			fi
			enable_ac=1
			vht_oper_chwidth=2
			eht_oper_chwidth=2
			vht_center_seg0=$idx
			eht_center_seg0=$idx
		;;
		EHT320)
			if [ "$band" = "6g" ]; then
				case "$channel" in
					1|5|9|13|17|21|25|29|33|37|41|45) idx=31;;
					49|53|57|61|65|69|73|77) idx=63;;
					81|85|89|93|97|101|105|109) idx=95;;
					113|117|121|125|129|133|137|141) idx=127;;
					177|181|185|189|193|197|201|205|209|213|217|221) idx=191;;
				esac
			else
				idx=0
			fi
			eht_oper_chwidth=9
			eht_centr_seg0=$idx
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
				esac
			fi
			vht_oper_chwidth=2
			vht_center_seg0=$idx
			if [ -n $ccfs ] && [ $ccfs -gt 0 ]; then
				append base_cfg "eht_oper_centr_freq_seg0_idx=$ccfs" "$N"
			elif [ -z $ccfs ] || [ "$ccfs" -eq "0" ]; then
				append base_cfg "eht_oper_centr_freq_seg0_idx=$idx" "$N"
			fi
		;;
	esac
	[ "$band" = "5g" ] && {
		json_get_vars background_radar:0

		[ "$background_radar" -eq 1 ] && append base_cfg "enable_background_radar=1" "$N"
	}
	[ "$band" = "6g" ] && {
		op_class=
		case "$htmode" in
			HE20|EHT20) op_class=131;;
			EHT320) op_class=137;;
			HE*|EHT*) op_class=$((132 + $vht_oper_chwidth))
		esac
		[ -n "$op_class" ] && append base_cfg "op_class=$op_class" "$N"
	}
	[ "$hwmode" = "a" ] || enable_ac=0

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
		for cap in $(iw phy "$phy" info | awk -F "[()]" '/VHT Capabilities/ { print $2 }'); do
			vht_cap="$(($vht_cap | $cap))"
		done

		append base_cfg "vht_oper_chwidth=$vht_oper_chwidth" "$N"
		append base_cfg "vht_oper_centr_freq_seg0_idx=$vht_center_seg0" "$N"

		cap_rx_stbc=$((($vht_cap >> 8) & 7))
		[ "$rx_stbc" -lt "$cap_rx_stbc" ] && cap_rx_stbc="$rx_stbc"
		vht_cap="$(( ($vht_cap & ~(0x700)) | ($cap_rx_stbc << 8) ))"

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
		[ "$(($vht_cap & 12))" -eq 4 -a 1 -le "$vht160" ] && \
			vht160_hw=1
		[ "$(($vht_cap & 12))" -eq 8 -a 2 -le "$vht160" ] && \
			vht160_hw=2
		[ "$vht160_hw" = 1 ] && vht_capab="$vht_capab[VHT160]"
		[ "$vht160_hw" = 2 ] && vht_capab="$vht_capab[VHT160-80PLUS80]"

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
		EHT* )	enable_ax=1 ;
			enable_be=1
			[ -n "$disable_eml_cap" ] && append base_cfg "disable_eml_cap=$disable_eml_cap" "$N"
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
			eht_ulmumimo_320mhz

		he_phy_cap=$(iw phy "$phy" info | sed -n '/HE Iftypes: AP/,$p' | awk -F "[()]" '/HE PHY Capabilities/ { print $2 }' | head -1)
		he_phy_cap=${he_phy_cap:2}
		he_mac_cap=$(iw phy "$phy" info | sed -n '/HE Iftypes: AP/,$p' | awk -F "[()]" '/HE MAC Capabilities/ { print $2 }' | head -1)
		he_mac_cap=${he_mac_cap:2}

		append base_cfg "ieee80211ax=1" "$N"
		[ "$hwmode" = "a" ] && {
			append base_cfg "he_oper_chwidth=$vht_oper_chwidth" "$N"
			append base_cfg "he_oper_centr_freq_seg0_idx=$vht_center_seg0" "$N"
			if [ "$enable_be" != "0" ]; then
				append base_cfg "eht_oper_chwidth=$eht_oper_chwidth" "$N"
				append base_cfg "eht_oper_centr_freq_seg0_idx=$eht_center_seg0"  "$N"
			fi
		}
		if [ "$enable_be" != "0" ]; then
			json_get_vars ru_punct_bitmap:0 ru_punct_ofdma:0 ru_punct_acs_threshold:0 ccfs:0
			append base_cfg "ieee80211be=1" "$N"
			append base_cfg "eht_su_beamformer=1" "$N"
			append base_cfg "eht_mu_beamformer=1" "$N"
			append base_cfg "eht_su_beamformee=1" "$N"
			if [ -n "$eht_ulmumimo_80mhz" ]; then
				if [ $eht_ulmumimo_80mhz -eq 0 ]; then
					append base_cfg "eht_ulmumimo_80mhz=0" "$N"
				elif [  $eht_ulmumimo_80mhz -gt 0 ]; then
					append base_cfg "eht_ulmumimo_80mhz=1" "$N"
				fi
			else
				append base_cfg "eht_ulmumimo_80mhz=-1" "$N"
			fi
			if [ -n "$eht_ulmumimo_320mhz" ]; then
				if [ $eht_ulmumimo_320mhz -eq 0 ]; then
					append base_cfg "eht_ulmumimo_320mhz=0" "$N"
				elif [  $eht_ulmumimo_320mhz -gt 0 ]; then
					append base_cfg "eht_ulmumimo_320mhz=1" "$N"
				fi
			else
				append base_cfg "eht_ulmumimo_320mhz=-1" "$N"
			fi
			if [ -n $ru_punct_bitmap ] && [ $ru_punct_bitmap -gt 0 ]; then
				append base_cfg "ru_punct_bitmap=$ru_punct_bitmap" "$N"
			fi
			if [ -n $ru_punct_ofdma ] && [ $ru_punct_ofdma -gt 0 ]; then
				append base_cfg "ru_punct_ofdma=$ru_punct_ofdma" "$N"
			fi
			if [ -n $ru_punct_acs_threshold ] && [ $ru_punct_acs_threshold -gt 0 ]; then
				append base_cfg "ru_punct_acs_threshold=$ru_punct_acs_threshold" "$N"
			fi
			[ -n "$use_ru_puncture_dfs" ] && append base_cfg "n=$use_ru_puncture_dfs" "$N"
		fi
		mac80211_add_he_capabilities \
			he_su_beamformer:${he_phy_cap:6:2}:0x80:$he_su_beamformer \
			he_su_beamformee:${he_phy_cap:8:2}:0x1:$he_su_beamformee \
			he_mu_beamformer:${he_phy_cap:8:2}:0x2:$he_mu_beamformer \
			he_spr_psr_enabled:${he_phy_cap:14:2}:0x1:$he_spr_psr_enabled \
			he_twt_required:${he_mac_cap:0:2}:0x6:$he_twt_required
		if [ -n "$he_ul_mumimo" ]; then
			if [ $he_ul_mumimo -eq 0 ]; then
				append base_cfg "he_ul_mumimo=0" "$N"
			elif [  $he_ul_mumimo -gt 0 ]; then
				append base_cfg "he_ul_mumimo=1" "$N"
			fi
		else
			append base_cfg "he_ul_mumimo=-1" "$N"
		fi

		if [ "$he_bss_color_enabled" -gt 0 ]; then
			append base_cfg "he_bss_color=$he_bss_color" "$N"
			[ "$he_spr_non_srg_obss_pd_max_offset" -gt 0 ] && { \
				append base_cfg "he_spr_non_srg_obss_pd_max_offset=$he_spr_non_srg_obss_pd_max_offset" "$N"
				he_spr_sr_control=$((he_spr_sr_control | (1 << 2)))
			}
			[ "$he_spr_psr_enabled" -gt 0 ] || he_spr_sr_control=$((he_spr_sr_control | (1 << 0)))
			append base_cfg "he_spr_sr_control=$he_spr_sr_control" "$N"
		else
			append base_cfg "he_bss_color_disabled=1" "$N"
		fi

		if [ "$is_6ghz" == "1" ]; then
			if [ -z $multiple_bssid ] && [ -z $ema ]; then
				multiple_bssid=1
				ema=1
			fi
		fi

		if [[ $htmode == "EHT"* ]] || [ "$is_6ghz" == "1" ]; then
			if [ "$has_ap" -gt 1 ]; then
				[ -n $multiple_bssid ] && [ $multiple_bssid -gt 0 ] && ([ -z $ema ] || ([ -n $ema ] && [ $ema -eq 0 ])) && append base_cfg "mbssid=1" "$N"
				[ -n $ema ] && [ $ema -gt 0 ] && append base_cfg "mbssid=2" "$N" && append base_cfg "ema=1" "$N"
			fi
		fi

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
	fi

	hostapd_prepare_device_config "$hostapd_conf_file" nl80211
	cat >> "$hostapd_conf_file" <<EOF
${channel:+channel=$channel}
${channel_list:+chanlist=$channel_list}
${hostapd_noscan:+noscan=1}
${tx_burst:+tx_queue_data2_burst=$tx_burst}
$base_cfg

EOF
	json_select ..
	radio_md5sum=$(md5sum $hostapd_conf_file | cut -d" " -f1)
}

mac80211_wds_support_check() {
	local phy="$1"

	local platform=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $1}')
	case "$platform" in
		"IPQ8074" | "IPQ6018" | "IPQ5018")
			frame_mode=$(cat /sys/module/ath11k/parameters/frame_mode)
			;;
		"IPQ9574")
			local freq="$(get_freq "$phy" "$channel" "$band")"
			local board_type=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $2}' | awk -F- '{print $3}')

			if [ $board_type == "C6" ] && [ $freq -gt 2000 ] && [ $freq -lt 3000]; then
				frame_mode=$(cat /sys/module/ath11k/parameters/frame_mode)
			else
				frame_mode=$(cat /sys/module/ath12k/parameters/frame_mode)
			fi
			;;
		 "IPQ5332")
			frame_mode=$(cat /sys/module/ath12k/parameters/frame_mode)
			;;
	esac

	echo "$frame_mode"
}

mac80211_hostapd_setup_bss() {
	local phy="$1"
	local ifname="$2"
	local macaddr="$3"
	local type="$4"

	hostapd_cfg=
	append hostapd_cfg "$type=$ifname" "$N"

	hostapd_set_bss_options hostapd_cfg "$phy" "$vif" || return 1
	json_get_vars wds wds_bridge dtim_period max_listen_int start_disabled

	set_default wds 0
	set_default start_disabled 0

	if [ "$wds" -gt 0 ]; then
		frame_mode=$(mac80211_wds_support_check "$phy")

		if [ $frame_mode -ne 1 ]; then
			echo WDS is supported only in native wifi mode. Kindly update the config > /dev/ttyMSM0
			return
		fi
		append hostapd_cfg "wds_sta=1" "$N"
		[ -n "$wds_bridge" ] && append hostapd_cfg "wds_bridge=$wds_bridge" "$N"
	fi

	[ "$staidx" -gt 0 -o "$start_disabled" -eq 1 ] && append hostapd_cfg "start_disabled=1" "$N"

	if [ "$is_6ghz" == "1" ]; then
		fils_cfg=
		if [ "$unsol_bcast_presp" -gt 0 ] && [ "$unsol_bcast_presp" -le 20 ]; then
			append fils_cfg "unsol_bcast_probe_resp_interval=$unsol_bcast_presp" "$N"
		elif [ "$fils_discovery" -gt 0 ] && [ "$fils_discovery" -le 20 ]; then
			append fils_cfg "fils_discovery_max_interval=$fils_discovery" "$N"
		else
			append fils_cfg "fils_discovery_max_interval=20" "$N"
		fi

		if [ -n "$multiple_bssid" ] && [ "$multiple_bssid" -eq 1 ] && [ "$type" == "interface" ]; then
			append hostapd_cfg "$fils_cfg" "$N"
		elif [ -z "$multiple_bssid" ] || [ "$multiple_bssid" -eq 0 ]; then
			append hostapd_cfg "$fils_cfg" "$N"
		fi
	fi

	config_get ht_mode $device htmode
	if ([ -n "$ht_mode" ] && [[ $ht_mode == "EHT"* ]]); then
		append hostapd_cfg "mld_ap=1" "$N"
	fi

	cat >> "$hostapd_conf_file"  <<EOF
$hostapd_cfg
bssid=$macaddr
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

	local ref="$(cat /sys/class/ieee80211/${phy}/macaddress)"
	local mask="$(cat /sys/class/ieee80211/${phy}/address_mask)"
	if [ $is_sphy_mband -eq 1 ]; then
		dev_idx=${device:11:1}
		local ref_dec=$( printf '%d\n' $( echo "0x$ref" | tr -d ':' ) )
		local mac_mask=$(($(($(($dev_idx << 8)) | $dev_idx))))
		local genref="$( echo $( printf '%012x\n' $(($(($mac_mask + $ref_dec))))) \
			| sed 's!\(..\)!\1:!g;s!:$!!' )"
			ref=$genref
	else

		[ "$mask" = "00:00:00:00:00:00" ] && {
			mask="ff:ff:ff:ff:ff:ff";

			[ "$(wc -l < /sys/class/ieee80211/${phy}/addresses)" -gt $id ] && {
				addr="$(mac80211_get_addr "$phy" "$id")"
				[ -n "$addr" ] && {
					echo "$addr"
					return
				}
			}
		}
	fi
	local oIFS="$IFS"; IFS=":"; set -- $mask; IFS="$oIFS"

	local mask1=$1
	local mask6=$6

	local oIFS="$IFS"; IFS=":"; set -- $ref; IFS="$oIFS"

	macidx=$(($id + 1))

	if [ "$mode" == "ap" ] && [ $multiple_bssid -eq 1 ] && [ $id -gt 0 ]; then
		local ref_dec=$( printf '%d\n' $( echo "0x$ref" | tr -d ':' ) )
		local bssid_l_mask=$(((1 << $max_bssid_ind) - 1))
		local bssid_l=$(((($ref_dec & $bssid_l_mask) + $id) % $max_bssid))
		local bssid_h=$((($bssid_l_mask ^ 0xFFFFFFFFFFFF) & $ref_dec))
		printf $( echo $( printf '%012x\n' $((bssid_h | bssid_l))) | sed 's!\(..\)!\1:!g;s!:$!!' )
		return
	fi

	[ "$((0x$mask1))" -gt 0 ] && {
		b1="0x$1"
		[ "$id" -gt 0 ] && \
			b1=$(($b1 ^ ((($id - !($b1 & 2)) << 2)) | 0x2))
		printf "%02x:%s:%s:%s:%s:%s" $b1 $2 $3 $4 $5 $6
		return
	}

	[ "$((0x$mask6))" -lt 255 ] && {
		printf "%s:%s:%s:%s:%s:%02x" $1 $2 $3 $4 $5 $(( 0x$6 ^ $id ))
		return
	}

	off2=$(( (0x$6 + $id) / 0x100 ))
	printf "%s:%s:%s:%s:%02x:%02x" \
		$1 $2 $3 $4 \
		$(( (0x$5 + $off2) % 0x100 )) \
		$(( (0x$6 + $id) % 0x100 ))
}

get_board_phy_name() (
	local path="$1"
	local fallback_phy=""

	__check_phy() {
		local val="$1"
		local key="$2"
		local ref_path="$3"

		json_select "$key"
		json_get_values path
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
	json_get_values path

	prev_phy="$(iwinfo nl80211 phyname "path=$path${suffix:++$suffix}")"
	[ -n "$prev_phy" ] || return 0

	[ "$prev_phy" = "$phy" ] && return 0

	iw "$prev_phy" set name "$phy"
)

find_phy() {
	[ -n "$phy" -a -d /sys/class/ieee80211/$phy ] && return 0

	# Incase multiple radio's are in the same soc, device path
	# for these radio's will be the same. In such case we can
	# get the phy based on the phy index of the soc
	local radio_idx=${1:5:1}
	local first_phy_idx=0
	local delta=0
	local phy_count=0
	local try=0
	config_load wireless
	while :; do
	if [ $is_sphy_mband -eq 1 ]; then
		devname=radio$radio_idx\_band$first_phy_idx
	else
		devname=radio$first_phy_idx
	fi
	config_get devicepath "$devname" path

	[ -n "$devicepath" -a -n "$path" ] || break
	[ "$path" == "$devicepath" ] && break
	first_phy_idx=$(($first_phy_idx + 1))
	done

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

mac80211_iw_interface_add() {
	local phy="$1"
	local ifname="$2"
	local type="$3"
	local wdsflag="$4"
	local rc
	local oldifname

	iw phy "$phy" interface add "$ifname" type "$type" $wdsflag >/dev/null 2>&1
	rc="$?"

	[ "$rc" = 233 ] && {
		# Device might have just been deleted, give the kernel some time to finish cleaning it up
		sleep 1

		iw phy "$phy" interface add "$ifname" type "$type" $wdsflag >/dev/null 2>&1
		rc="$?"
	}

	[ "$rc" = 233 ] && {
		# Keep matching pre-existing interface
		[ -d "/sys/class/ieee80211/${phy}/device/net/${ifname}" ] && \
		case "$(iw dev $ifname info | grep "^\ttype" | cut -d' ' -f2- 2>/dev/null)" in
			"AP")
				[ "$type" = "__ap" ] && rc=0
				;;
			"IBSS")
				[ "$type" = "adhoc" ] && rc=0
				;;
			"managed")
				[ "$type" = "managed" ] && rc=0
				;;
			"mesh point")
				[ "$type" = "mp" ] && rc=0
				;;
			"monitor")
				[ "$type" = "monitor" ] && rc=0
				;;
		esac
	}

	[ "$rc" = 233 ] && {
		iw dev "$ifname" del >/dev/null 2>&1
		[ "$?" = 0 ] && {
			sleep 1

			iw phy "$phy" interface add "$ifname" type "$type" $wdsflag >/dev/null 2>&1
			rc="$?"
		}
	}

	[ "$rc" != 0 ] && {
		# Device might not support virtual interfaces, so the interface never got deleted in the first place.
		# Check if the interface already exists, and avoid failing in this case.
		[ -d "/sys/class/ieee80211/${phy}/device/net/${ifname}" ] && rc=0
	}

	[ "$rc" != 0 ] && {
		# Device doesn't support virtual interfaces and may have existing interface other than ifname.
		oldifname="$(basename "/sys/class/ieee80211/${phy}/device/net"/* 2>/dev/null)"
		[ "$oldifname" ] && ip link set "$oldifname" name "$ifname" 1>/dev/null 2>&1
		rc="$?"
	}

	[ "$rc" != 0 ] && echo "Failed to create interface $ifname"
	return $rc
}

mac80211_set_ifname() {
	local phy="$1"
	local prefix="$2"
	eval "ifname=\"$phy-$prefix\${idx_$prefix:-0}\"; idx_$prefix=\$((\${idx_$prefix:-0 } + 1))"
}

mac80211_prepare_vif() {
	json_select config

	json_get_vars ifname mode ssid wds powersave macaddr enable wpa_psk_file vlan_file mld
	if [ $is_sphy_mband -eq 1 ]; then
		wdev=$((${1:5:1} + ${1:11:1}))
		config_get ht_mode $device htmode
		if ([ -n "$ht_mode" ] && [[ $ht_mode == "EHT"* ]] && [ -n "$mld" ]); then
			config_get mld_ifname "$mld" ifname
			if [ -z "$mld_ifname" ]; then
				ml_idx=$(mac80211_get_mld_idx $mld)
				[ -z "$ml_idx" ] || ifname="wlan$ml_idx"
			else
				ifname="$mld_ifname"
			fi
			[ -n "$ifname" ] || [ -n "$if_idx" ] || if_idx=1
		else
			if [ $sta_vaps_count -gt 0 -o $mld_vaps_count -gt 0 ]; then
				[ -n "$if_idx" ] || if_idx=1
			fi
		fi
		[ -n "$ifname" ] || ifname="wlan${wdev#wlan}${if_idx:+-$if_idx}"
			if ([ -n "$ht_mode" ] && [[ $ht_mode == "EHT"* ]] && [ -n "$mld" ]); then
				uci_set wireless "$mld" ifname "$ifname"
				uci commit wireless
			fi
	else
		for wdev in $(list_phy_interfaces "$phy"); do
			phy_name="$(cat /sys/class/ieee80211/${phy}/device/net/${wdev}/phy80211/name)"
			if [ "$phy_name" == "$phy" ]; then
				if_name = $wdev
				break;
			fi
		done
		[ -n "$ifname" ] || ifname="wlan${phy#phy}${if_idx:+-$if_idx}"
	fi

	if_idx=$((${if_idx:-0} + 1))

	set_default wds 0
	set_default powersave 0

	json_select ..

	if [ -z "$macaddr" ]; then
		macaddr="$(mac80211_generate_mac $phy)"
		macidx="$(($macidx + 1))"
	elif [ "$macaddr" = 'random' ]; then
		macaddr="$(macaddr_random)"
	fi

	json_add_object data
	json_add_string ifname "$ifname"
	json_close_object

	[ "$mode" == "ap" ] && {
		[ -z "$wpa_psk_file" ] && hostapd_set_psk "$ifname"
		[ -z "$vlan_file" ] && hostapd_set_vlan "$ifname"
	}

	json_select config

	# It is far easier to delete and create the desired interface
	case "$mode" in
		adhoc)
			mac80211_iw_interface_add "$phy" "$ifname" adhoc || return
		;;
		ap)
			# Hostapd will handle recreating the interface and
			# subsequent virtual APs belonging to the same PHY
			if [ -n "$hostapd_ctrl" ]; then
				type=bss
			else
				type=interface
			fi

			mac80211_hostapd_setup_bss "$phy" "$ifname" "$macaddr" "$type" || return

			NEWAPLIST="${NEWAPLIST}$ifname "
			[ -n "$hostapd_ctrl" ] || {
				ap_ifname="${ifname}"
				hostapd_ctrl="${hostapd_ctrl:-/var/run/hostapd/$ifname}"
			}
		;;
		mesh)
			mac80211_iw_interface_add "$phy" "$ifname" mp || return
		;;
		monitor)
			mac80211_iw_interface_add "$phy" "$ifname" monitor || return
			NEWUMLIST="${NEWUMLIST}$ifname"
		;;
		sta)
			local wdsflag=
			[ "$enable" = 0 ] || staidx="$(($staidx + 1))"
			if [ "$wds" -gt 0 ]; then
				frame_mode=$(mac80211_wds_support_check "$phy")
				if [ $frame_mode -ne 1 ]; then
					echo WDS is supported only in native wifi mode. Kindly update the config > /dev/ttyMSM0
					return
				fi
				wdsflag="4addr on"
			fi
			mac80211_iw_interface_add "$phy" "$ifname" managed "$wdsflag" || return
			if [ "$wds" -gt 0 ]; then
				iw "$ifname" set 4addr on
			else
				iw "$ifname" set 4addr off
			fi
			[ "$powersave" -gt 0 ] && powersave="on" || powersave="off"
			iw "$ifname" set power_save "$powersave"
			sta_ifname=$ifname
		;;
	esac

	case "$mode" in
		monitor)
			if echo "$devname" | grep -q  "band"; then
				[ "$auto_channel" -gt 0 ] || iw dev "$ifname" add channel "$channel" $htmode
			else
				[ "$auto_channel" -gt 0 ] || iw dev "$ifname" set channel "$channel" $htmode
			fi
		;;

		mesh)
			[ "$auto_channel" -gt 0 ] || iw dev "$ifname" set channel "$channel" $iw_htmode
		;;
	esac

	if [ "$mode" != "ap" ]; then
		# ALL ap functionality will be passed to hostapd
		# All interfaces must have unique mac addresses
		# which can either be explicitly set in the device
		# section, or automatically generated
		ip link set dev "$ifname" address "$macaddr"
	fi

	json_select ..
}

mac80211_setup_supplicant() {
	local enable=$1
	local add_sp=0
	local spobj="$(ubus -S list | grep wpa_supplicant.${ifname})"

	[ "$enable" = 0 ] && {
		ubus call wpa_supplicant.${phy} config_remove "{\"iface\":\"$ifname\"}"
		ip link set dev "$ifname" down
		iw dev "$ifname" del
		return 0
	}

	wpa_supplicant_prepare_interface "$ifname" nl80211 || {
		iw dev "$ifname" del
		return 1
	}
	if [ "$mode" = "sta" ]; then
		wpa_supplicant_add_network "$ifname"
	else
		wpa_supplicant_add_network "$ifname" "$freq" "$htmode" "$noscan"
	fi

	NEWSPLIST="${NEWSPLIST}$ifname "

	if [ "${NEWAPLIST%% *}" != "${OLDAPLIST%% *}" ]; then
		[ "$spobj" ] && ubus call wpa_supplicant config_remove "{\"iface\":\"$ifname\"}"
		add_sp=1
	fi
	[ -z "$spobj" ] && add_sp=1

	NEW_MD5_SP=$(test -e "${_config}" && md5sum ${_config})
	OLD_MD5_SP=$(uci -q -P /var/state get wireless.${device}.md5_${ifname})
	if [ "$add_sp" = "1" ]; then
		wpa_supplicant_run "$ifname" "$hostapd_ctrl"
	else
		[ "${NEW_MD5_SP}" == "${OLD_MD5_SP}" ] || ubus call $spobj reload
	fi
	uci -q -P /var/state set wireless.${device}.md5_${ifname}="${NEW_MD5_SP}"
	return 0
}

mac80211_setup_supplicant_noctl() {
	local enable=$1
	local spobj="$(ubus -S list | grep wpa_supplicant.${ifname})"
	wpa_supplicant_prepare_interface "$ifname" nl80211 || {
		iw dev "$ifname" del
		return 1
	}

	wpa_supplicant_add_network "$ifname" "$freq" "$htmode" "$noscan"
	wpa_supplicant_run "$ifname"

	NEWSPLIST="${NEWSPLIST}$ifname "
	[ "$enable" = 0 ] && {
		ubus call wpa_supplicant config_remove "{\"iface\":\"$ifname\"}"
		ip link set dev "$ifname" down
		return 0
	}
	if [ -z "$spobj" ]; then
		wpa_supplicant_run "$ifname"
	else
		ubus call $spobj reload
	fi
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

	iw dev "$ifname" set type ibss
	iw dev "$ifname" ibss join "$ssid" $freq $iw_htmode fixed-freq $bssid \
		beacon-interval $beacon_int \
		${brstr:+basic-rates $brstr} \
		${mcval:+mcast-rate $mcval} \
		${keyspec:+keys $keyspec}
}

mac80211_setup_mesh() {
	local enable=$1
	json_get_vars ssid mesh_id mcast_rate

	NEWUMLIST="${NEWUMLIST}$ifname "

	[ "$enable" = 0 ] && {
		ip link set dev "$ifname" down
		return 0
	}
	json_get_vars key
	if [ -n "$key" ]; then
		wireless_vif_parse_encryption
		mac80211_setup_supplicant_noctl || failed=1
	else
		case "$htmode" in
			VHT20|HT20|HE20) mesh_htmode=HT20;;
			HT40*|VHT40|HE40)
				case "$hwmode" in
					a)
						case "$(( ($channel / 4) % 2 ))" in
							1) mesh_htmode="HT40+" ;;
							0) mesh_htmode="HT40-";;
						esac
						;;
					*)
						case "$htmode" in
							HT40+) mesh_htmode="HT40+";;
							HT40-) mesh_htmode="HT40-";;
					*)
						if [ "$channel" -lt 7 ]; then
							mesh_htmode="HT40+"
						else
							mesh_htmode="HT40-"
						fi
						;;
						esac
						;;
				esac
			;;
			VHT80|HE80|EHT80)
					mesh_htmode="80MHz"
			;;
			*) mesh_htmode="NOHT" ;;
		esac

		mcval=
		[ -n "$mcast_rate" ] && wpa_supplicant_add_rate mcval "$mcast_rate"
		[ -n "$mesh_id" ] && ssid="$mesh_id"
		
		iw dev "$ifname" mesh join "$ssid" freq $freq $mesh_htmode \
			${mcval:+mcast-rate $mcval} \
			beacon-interval $beacon_int
	fi
}

mac80211_get_seg0() {
	local ht_mode="$1"
	local seg0=0

	case "$ht_mode" in
		40)
			if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
				case "$(( ($channel / 4) % 2 ))" in
					1) seg0=$(($channel - 2));;
					0) seg0=$(($channel + 2));;
				esac
			elif [ $freq != 5935 ]; then
				case "$(( ($channel / 4) % 2 ))" in
					1) seg0=$(($channel + 2));;
					0) seg0=$(($channel - 2));;
				esac
			fi
		;;
		80)
			if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
				case "$(( ($channel / 4) % 4 ))" in
					0) seg0=$(($channel + 6));;
					1) seg0=$(($channel + 2));;
					2) seg0=$(($channel - 2));;
					3) seg0=$(($channel - 6));;
				esac
			elif [ $freq != 5935 ]; then
				case "$(( ($channel / 4) % 4 ))" in
					1) seg0=$(($channel + 6));;
					2) seg0=$(($channel + 2));;
					3) seg0=$(($channel - 2));;
					0) seg0=$(($channel - 6));;
				esac
			fi
		;;
		160)
			if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
				case "$channel" in
					1|5|9|13|17|21|25|29) seg0=15;;
					33|37|41|45|49|53|57|61) seg0=47;;
					65|69|73|77|81|85|89|93) seg0=79;;
					97|101|105|109|113|117|121|125) seg0=111;;
					129|133|137|141|145|149|153|157) seg0=143;;
					161|165|169|173|177|181|185|189) seg0=175;;
					193|197|201|205|209|213|217|221) seg0=207;;
				esac
			elif [ $freq != 5935 ]; then
				case "$channel" in
					36|40|44|48|52|56|60|64) seg0=50;;
					100|104|108|112|116|120|124|128) seg0=114;;
					149|153|157|161|165|169|173|177) seg0=163;;
				esac
			fi
		;;
		320)
			if [ $freq -ge 5955 ] && [ $freq -le 7115 ]; then
				case "$channel" in
					1|5|9|13|17|21|25|29|33|37|41|45) seg0=31;;
					49|53|57|61|65|69|73|77) seg0=63;;
					81|85|89|93|97|101|105|109) seg0=95;;
					113|117|121|125|129|133|137|141) seg0=127;;
					145|149|153|157|161|165|169|173) seg0=159;;
					177|181|185|189|193|197|201|205|209|213|217|221) seg0=191;;
				esac
			elif [ $freq -ge 5500 ] && [ $freq -le 5730 ]; then
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

mac80211_setup_vif() {
	local name="$1"
	local failed
	local action=up

	json_select data
	json_get_vars ifname
	json_select ..

	json_select config
	json_get_vars mode mld
	json_get_var vif_txpower
	json_get_var vif_enable enable 1

	[ "$vif_enable" = 1 ] || action=down
	if [ "$mode" != "ap" ] || [ "$ifname" = "$ap_ifname" ]; then
		ip link set dev "$ifname" "$action" || {
			wireless_setup_vif_failed IFUP_ERROR
			json_select ..
			return
		}
		[ -z "$vif_txpower" ] || iw dev "$ifname" set txpower fixed "${vif_txpower%%.*}00"
	fi

	case "$mode" in
		mesh)
			wireless_vif_parse_encryption
			if [ $auto_channel -gt 0 ]; then
				chan=$(echo ${channel_list} | cut -d '-' -f 1)
				freq="$(get_freq "$phy" "$chan" "$band")"
			else
				freq="$(get_freq "$phy" "$channel" "$band")"
			fi
			[ -z "$htmode" ] && htmode="NOHT";
			if wpa_supplicant -vmesh || [ "$wpa" -gt 0 -o "$auto_channel" -gt 0 ] || chan_is_dfs "$phy" "$channel"; then
				mac80211_setup_supplicant $vif_enable || failed=1
			else
				mac80211_setup_mesh $vif_enable $freq
			fi
			for var in $MP_CONFIG_INT $MP_CONFIG_BOOL $MP_CONFIG_STRING; do
				json_get_var mp_val "$var"
				[ -n "$mp_val" ] && iw dev "$ifname" set mesh_param "$var" "$mp_val"
			done
		;;
		adhoc)
			wireless_vif_parse_encryption
			if [ "$wpa" -gt 0 -o "$auto_channel" -gt 0 ]; then
				mac80211_setup_supplicant_noctl $vif_enable || failed=1
			else
				mac80211_setup_adhoc $vif_enable
			fi
		;;
		sta)
			if [ $auto_channel -gt 0 ]; then
                                chan=$(echo ${channel_list} | cut -d '-' -f 1)
                                freq="$(get_freq "$phy" "$chan" "$band")"
                        else
                                freq="$(get_freq "$phy" "$channel" "$band")"
                        fi
                        freq_list=$(get_sta_freq_list $phy $freq)
                        sta_started=0
                        if ([ "$is_sphy_mband" -eq 1 ] &&
                            [ "$sta_vaps_count" -gt 1 ] && [ "$sta_radio" -gt 1 ]); then
                                #Keep count of the links to be supported before the supplicant is started
                                touch /var/run/wpa_supplicant-$device-updated-cfg
                                sta_cfg_updated=$(ls /var/run/wpa_supplicant-*-updated-cfg | wc -l)
                                if [ -n "$freq_list" ]; then
                                        if [ -f  "/tmp/${mld}_freq_list" ]; then
                                                tmp_freq="$(cat /tmp/${mld}_freq_list)"
                                                if ! [[ "$tmp_freq" =~ "$freq_list" ]]; then
                                                        echo -n "$freq_list " >> /tmp/${mld}_freq_list
                                                fi
                                        else
                                                echo -n "$freq_list " >> /tmp/${mld}_freq_list
                                        fi
                                        freq_list=$(cat /tmp/${mld}_freq_list)
                                fi

                                if [ "$sta_cfg_updated" = "$sta_radio" ]; then
                                        mac80211_setup_supplicant || failed=1
                                        sta_started=1
					rm -rf /var/run/wpa_supplicant-*-updated-cfg
					rm -rf /tmp/*_freq_list
                                fi
                        else
                                mac80211_setup_supplicant || failed=1
                                sta_started=1
                        fi
		;;
		monitor)
			case "$htmode" in
				VHT20|HT20|HE20|EHT20)
					if echo "$devname" | grep -q  "band"; then
						iw dev "$ifname" add freq "$freq" "20"
					else
						iw dev "$ifname" set freq "$freq" "20"
					fi
					;;
				HT40*|VHT40|HE40|EHT40)
					if echo "$devname" | grep -q  "band"; then
						iw dev "$ifname" add freq "$freq" "40" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "40")")"
					else
						iw dev "$ifname" set freq "$freq" "40" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "40")")"
					fi
					;;
				VHT80|HE80|EHT80)
					if echo "$devname" | grep -q  "band"; then
						iw dev "$ifname" add freq "$freq" "80" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "80")")"
					else
						iw dev "$ifname" set freq "$freq" "80" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "80")")"
					fi
					;;
				VHT160|HE160|EHT160)
					if echo "$devname" | grep -q  "band"; then
						iw dev "$ifname" add freq "$freq" "160" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "160")")"
					else
						iw dev "$ifname" set freq "$freq" "160" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "160")")"
					fi
					;;
			esac
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
	print $2
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

mac80211_vap_cleanup() {
	local service="$1"
	local vaps="$2"
	local phy=$3
	local device=$5

	for wdev in $vaps; do
		phy_name="$(cat /sys/class/ieee80211/${phy}/device/net/${wdev}/phy80211/name)"
		if [ -n "$phy_name" && "$phy_name" != "$phy" ]; then
			continue;
		fi

		case $service in
			"hostapd")
				if ( [ -f "/var/run/hostapd-${wdev}.lock" ] || \
					[ -f "/var/run/hostapd-${4}.lock" ] ); then
					hostapd_cli -iglobal raw REMOVE ${wdev}
					rm /var/run/hostapd-${wdev}.lock
					rm /var/run/hostapd/${wdev}
				fi

				if [ -f "/var/run/wifi-$device.pid" ]; then
					pid=$(cat /var/run/wifi-$device.pid)
					kill -9 $pid
					rm -rf  /var/run/wifi-$device.pid
					rm /var/run/hostapd/w*
				fi
			;;
			"wpa_supplicant")
				[ -f "/var/run/wpa_supplicant-${wdev}.lock" ] && { \
					wpa_cli -g /var/run/wpa_supplicantglobal interface_remove ${wdev}
					rm /var/run/wpa_supplicant-${wdev}.lock
				}

				[ -f "/var/run/wpa_supplicant-${wdev}.pid" ] && { \
					kill -9 $(cat /var/run/wpa_supplicant-${wdev}.pid)
					rm -rf /var/run/wpa_supplicant-${wdev}.pid
					rm /var/run/wpa_supplicant-${wdev}.lock
				}
			;;
		esac
		ip link set dev "$wdev" down 2>/dev/null
		iw dev "$wdev" del
	done
}

mac80211_interface_cleanup() {
	local phy="$1"
	local primary_ap=$(uci -q -P /var/state get wireless.${device}.aplist)
	local device="$2"
	primary_ap=${primary_ap%% *}

	if [ ${#device} -eq 12 ]; then
		local dev_wlan=wlan$((${2:5:1} + ${2:11:1}))
		local ap_ifnames="$(uci -q -P /var/state get wireless.${device}.aplist)"
		local sta_ifnames="$(uci -q -P /var/state get wireless.${device}.splist)"
		local adhoc_ifnames="$(uci -q -P /var/state get wireless.${device}.umlist)"
		mac80211_vap_cleanup hostapd "$ap_ifnames" $phy $dev_wlan $2
		mac80211_vap_cleanup wpa_supplicant "$sta_ifnames" $phy
		mac80211_vap_cleanup none "$adhoc_ifnames" $phy
	else
		mac80211_vap_cleanup hostapd "${primary_ap}"
		mac80211_vap_cleanup wpa_supplicant "$(uci -q -P /var/state get wireless.${device}.splist)"
		mac80211_vap_cleanup none "$(uci -q -P /var/state get wireless.${device}.umlist)"
	fi
}

mac80211_set_noscan() {
	hostapd_noscan=1
}

drv_mac80211_cleanup() {
	hostapd_common_cleanup
}

mac80211_update_bondif() {
	local iflist
	config_load wireless
	mac80211_get_wifi_mlds() {
		append _mlds $1
	}

	config_foreach mac80211_get_wifi_mlds wifi-mld

	if [ -z "$_mlds" ]; then
		return
	fi
	for _mld in $_mlds
	do
		config_get mld_ifname "$_mld" ifname
		config_get is_bonded "$_mld" bonded
		mac80211_export_mld_info
		#Check if ppe_ds_enable is set and then update the bondif
		if ([ $mld_vaps_count -ge 2 ] && [ -n $mld_ifname ]); then
			cmd="ls /sys/class/net"
			iface=$($cmd | grep "$mld_ifname"_b)
			if ([ -d /sys/class/net/"$mld_ifname"_b ] && [ -n $network_bridge ]); then
				bonded_macaddr=$(iw dev $mld_ifname info | grep addr | head -1 | awk '{print $2}')
				brctl delif $network_bridge $mld_ifname
				brctl addif $network_bridge "$mld_ifname"_b
				ifconfig "$mld_ifname"_b down
				ifconfig "$mld_ifname"_b hw ether $bonded_macaddr
				ifconfig "$mld_ifname"_b up
			fi
		fi
	done
	return
}

drv_mac80211_setup() {
	local device=$1
	# Note: In case of single wiphy, the device name would be radio#idx_band#bid
	#       where idx is 0 and bid is [0 - 2] when there is one phy and 3 bands.
	#       similarly it can be extended when there are multiple phy and multiple
	#       bands.
	if [ ${#device} -eq 12 ]; then
		local is_sphy_mband=1
		mac80211_export_mld_info
	fi
	json_select config
	json_get_vars \
		phy macaddr path \
		country chanbw distance \
		txpower antenna_gain \
		multiple_bssid noscan \
		rxantenna txantenna \
		frag rts beacon_int:100 htmode \
		ru_punct_bitmap \
		he_ul_mumimo \
		eht_ulmumimo_80mhz \
		eht_ulmumimo_160mhz \
		eht_ulmumimo_320mhz \
		ccfs
	json_get_values basic_rate_list basic_rate
	json_get_values scan_list scan_list
	json_select ..

	find_phy $1 || {
		echo "Could not find PHY for device '$1'"
		wireless_set_retry 1
		return 1
	}

	wireless_set_data phy="$phy"
	[ -z "$(uci -q -P /var/state show wireless._${phy})" ] && uci -q -P /var/state set wireless._${phy}=phy

	OLDAPLIST=$(uci -q -P /var/state get wireless.${device}.aplist)
	OLDSPLIST=$(uci -q -P /var/state get wireless.${device}.splist)
	OLDUMLIST=$(uci -q -P /var/state get wireless.${device}.umlist)

	local wdev
	local cwdev
	local found

	for wdev in $(list_phy_interfaces "$phy"); do
		found=0
		for cwdev in $OLDAPLIST $OLDSPLIST $OLDUMLIST; do
			if [ "$wdev" = "$cwdev" ]; then
				found=1
				break
			fi
		done
		if [ "$found" = "0" ]; then
			ip link set dev "$wdev" down
			iw dev "$wdev" del
		fi
	done

	# convert channel to frequency
	[ "$auto_channel" -gt 0 ] || freq="$(get_freq "$phy" "$channel" "$band")"

	[ -n "$country" ] && {
		iw reg get | grep -q "^country $country:" || {
			iw reg set "$country"
			sleep 1
		}
	}
	if [ "$is_sphy_mband" -eq 1 ]; then
		hostapd_conf_file="/var/run/hostapd-${phy}_band${device:11:1}.conf"
	else
		hostapd_conf_file="/var/run/hostapd-$phy.conf"
	fi

	no_ap=1
	macidx=0
	staidx=0

	[ -n "$chanbw" ] && {
		for file in /sys/kernel/debug/ieee80211/$phy/ath9k*/chanbw /sys/kernel/debug/ieee80211/$phy/ath5k/bwmode; do
			[ -f "$file" ] && echo "$chanbw" > "$file"
		done
	}

	set_default rxantenna 0xffffffff
	set_default txantenna 0xffffffff
	set_default distance 0
	set_default antenna_gain 0

	[ "$txantenna" = "all" ] && txantenna=0xffffffff
	[ "$rxantenna" = "all" ] && rxantenna=0xffffffff

	iw phy "$phy" set antenna $txantenna $rxantenna >/dev/null 2>&1
	iw phy "$phy" set antenna_gain $antenna_gain >/dev/null 2>&1
	iw phy "$phy" set distance "$distance" >/dev/null 2>&1

	if [ -n "$txpower" ]; then
		iw phy "$phy" set txpower fixed "${txpower%%.*}00"
	else
		iw phy "$phy" set txpower auto
	fi

	[ -n "$frag" ] && iw phy "$phy" set frag "${frag%%.*}"
	[ -n "$rts" ] && iw phy "$phy" set rts "${rts%%.*}"

	has_ap=0
	hostapd_ctrl=
	ap_ifname=
	hostapd_noscan=
	for_each_interface "ap" mac80211_check_ap

	rm -f "$hostapd_conf_file"

	for_each_interface "sta adhoc mesh" mac80211_set_noscan
	[ "$has_ap" -gt 0 ] && mac80211_hostapd_setup_base "$phy" "$device"

	if [ $multiple_bssid -eq 1 ] && [ "$has_ap" -gt 1 ]; then
		max_bssid_ind=0
		local iter=$((has_ap-1))
		while [ "$iter" -gt 0 ]
		do
			max_bssid_ind=$((max_bssid_ind+1))
			iter=$((iter >> 1))
		done

		max_bssid=$((1 << max_bssid_ind))
	fi

	mac80211_prepare_iw_htmode


	NEWAPLIST=
	for_each_interface "ap" mac80211_prepare_vif ${device} ${multiple_bssid}
	uci -q -P /var/state set wireless.${device}.aplist="${NEWAPLIST}"

	NEW_MD5=$(test -e "${hostapd_conf_file}" && md5sum ${hostapd_conf_file})
	OLD_MD5=$(uci -q -P /var/state get wireless._${phy}.md5)
	if [ "${NEWAPLIST}" != "${OLDAPLIST}" ]; then
		mac80211_vap_cleanup hostapd "${OLDAPLIST}"
	fi

	NEWSTALIST=
	NEWUMLIST=
	for_each_interface "sta adhoc mesh monitor" mac80211_prepare_vif ${device}

	[ -n "${NEWAPLIST}" ] && mac80211_iw_interface_add "$phy" "${NEWAPLIST%% *}" __ap
	local add_ap=0
	local primary_ap=${NEWAPLIST%% *}
	[ "$is_sphy_mband" -eq 1 ] && {
		if [ "$mld_vaps_count" -gt 1 ] && [ "$radio_up_count" -gt 1 ]; then
			hostapd_add_bss=0
		else
			hostapd_add_bss=1
		fi
	}
	[ -n "$hostapd_ctrl" ] && {
		local no_reload=1
		if [ -n "$(ubus list | grep hostapd.$primary_ap)" ]; then
			no_reload=0
			[ "${NEW_MD5}" = "${OLD_MD5}" ] || {
				ubus call hostapd.$primary_ap reload
				no_reload=$?
				if [ "$no_reload" != "0" ]; then
					mac80211_vap_cleanup hostapd "${OLDAPLIST}"
					mac80211_vap_cleanup wpa_supplicant "$(uci -q -P /var/state get wireless.${device}.splist)"
					mac80211_vap_cleanup none "$(uci -q -P /var/state get wireless.${device}.umlist)"
					mac80211_iw_interface_add "$phy" "${NEWAPLIST%% *}" __ap
					for_each_interface "sta adhoc mesh monitor" mac80211_prepare_vif
				fi
			}
		fi
		if [ "$no_reload" != "0" ]; then
			add_ap=1
			#ubus wait_for hostapd
			#local hostapd_res="$(ubus call hostapd config_add "{\"iface\":\"$primary_ap\", \"config\":\"${hostapd_conf_file}\"}")"
			touch /var/run/hostapd-$device-updated-cfg
			#ret="$?"
			#[ "$ret" != 0 -o -z "$hostapd_res" ] && {
			#	wireless_setup_failed HOSTAPD_START_FAILED
			#	return
			#}
			#wireless_add_process "$(jsonfilter -s "$hostapd_res" -l 1 -e @.pid)" "/usr/sbin/hostapd" 1 1
		fi
		hostapd_cfg_updated=$(ls /var/run/hostapd-*-updated-cfg | wc -l)
		if [ "$hostapd_cfg_updated" = "$radio_up_count" ]; then
			bands_info=$(ls /var/run/hostapd*updated-cfg | grep -o band.)
			for __band in $bands_info
			do
				append  config_files /var/run/hostapd-phy${phy#phy}_${__band}.conf
			done
			#MLO vaps, single instance of hostapd is started
			/usr/sbin/hostapd -B -P /var/run/wifi-$device.pid $config_files
		fi

		if [ -z "$is_sphy_mband" ] || [ "$hostapd_add_bss" -eq 1 ]; then
			/usr/sbin/hostapd -B -P /var/run/wifi-$device.pid $hostapd_conf_file
		fi
	}
	uci -q -P /var/state set wireless.${device}.aplist="${NEWAPLIST}"
	uci -q -P /var/state set wireless.${device}.md5="${NEW_MD5}"

	for_each_interface "ap" mac80211_setup_vif

	for_each_interface "sta adhoc mesh monitor" mac80211_setup_vif

	uci -q -P /var/state set wireless.${device}.splist="${NEWSPLIST}"
	uci -q -P /var/state set wireless.${device}.umlist="${NEWUMLIST}"

	local foundvap
	local dropvap=""
	for oldvap in $OLDSPLIST; do
		foundvap=0
		for newvap in $NEWSPLIST; do
			[ "$oldvap" = "$newvap" ] && foundvap=1
		done
		[ "$foundvap" = "0" ] && dropvap="$dropvap $oldvap"
	done
	[ -n "$dropvap" ] && mac80211_vap_cleanup wpa_supplicant "$dropvap"
	wireless_set_up

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

	if [[ ! -z "$ap_ifname" && ! -z "$sta_ifname" && ! -z "$hostapd_conf_file" ]]; then
                [ -f "/lib/apsta_mode.sh" ] && {
                       if [ $sta_started -eq 1 ]; then
                               if ([ $sta_vaps_count -gt 0 ] && [ $sta_radio -gt 0 ]); then
                                       sta_radio_band_idx=$(ls /var/run/wpa_supplicant-*-updated-cfg | awk '{ print $1 }' | cut -f2 -d"-" | awk '{print substr($0,length,1)}')
                                       for i in $sta_radio_band_idx
                                       do
                                               if [ -e "/var/run/hostapd-${phy}_band${i}.conf" ]; then
                                                       intf_name=$(cat /var/run/hostapd-${phy}_band${i}.conf | grep -w "interface" | cut -f2 -d "=")
                                                       if ! [[ "$apifs" =~ "$intf_name" ]]; then
                                                               append apifs $intf_name
                                                       fi
                                               fi
                                       done
                                       if [ -n "$apifs" ]; then
                                               ap_ifname="$apifs"
                                       fi
                               fi
                                . /lib/apsta_mode.sh "$sta_ifname" "$ap_ifname" "$hostapd_conf_file" "$phy"
                                echo "$!" >> /tmp/apsta_mode.pid
                       fi
                }
        fi

	[ -f "/lib/performance.sh" ] && {
		. /lib/performance.sh
	}
	for_each_interface "ap mesh" mac80211_set_fq_limit
	#Check if ppe_ds_enable is set and then update the bondif
	if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
		mac80211_update_bondif
	fi


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
	json_get_vars phy
	json_select ..
	[ -n "$phy" ] || {
		echo "Bug: PHY is undefined for device '$1'"
		return 1
	}
	device=$1
	mac80211_interface_cleanup "$phy" "$1"
	uci -q -P /var/state revert wireless.${device}
}

mac80211_update_mld_iface_config() {
	vif_name=$1
	mld_name=$2
	local _ifaces
	local _iface
	# Get the following from section wifi-mld
	config_get mld_ssid "$mld_name" ssid
	config_get mld_encryption "$mld_name" encryption
	config_get mld_key "$mld_name" key
	config_get mld_sae "$mld_name" sae_pwe

	json_get_keys _ifaces interfaces
	json_select interfaces
	for _iface in $_ifaces; do
		json_select "$_iface"
		json_select config
		json_get_vars mld
		if [[ "$mld" == "$mld_name" ]]; then
			if [ -n "$mld_ssid" ]; then
				json_add_string "ssid" "$mld_ssid"
				uci_set wireless "$vif_name" ssid "$mld_ssid"
			fi
			if [ -n "$mld_encryption" ]; then
				json_add_string "encryption" "$mld_encryption"
				uci_set wireless "$vif_name" encryption "$mld_encryption"
			fi
			if [ -n "$mld_key" ]; then
				json_add_string "key" "$mld_key"
				uci_set wireless "$vif_name" key "$mld_key"
			fi
			if [ -n "$mld_sae" ]; then
				json_add_int "sae_pwe" "$mld_sae"
				uci_set wireless "$vif_name" sae_pwe "$mld_sae"
			fi
		fi
		json_select ..
		json_select ..
	done
	uci commit wireless
	json_select ..
}


mac80211_update_mld_configs() {
	local iflist
	config_load wireless

	mac80211_update_mld_cfg() {
		append iflist $1
	}
	config_foreach mac80211_update_mld_cfg wifi-iface

	for name in $iflist
	do
		config_get mld_name $name mld
		config_get ml_device $name device
		config_get ht_mode $ml_device htmode
		if ([ -n "$ht_mode" ] && [[ $ht_mode == "EHT"* ]]  && [ -n "$mld_name" ]); then
			mac80211_update_mld_iface_config $name $mld_name
		fi
	done
}

mac80211_derive_ml_info() {
        local _mlds
        local _devices_up
        local _ifaces
        config_load wireless
        mld_vaps_count=0
        radio_up_count=0

        mac80211_get_wifi_mlds() {
                append _mlds $1
        }
        config_foreach mac80211_get_wifi_mlds wifi-mld

        if [ -z "$_mlds" ]; then
                return
        fi

        mac80211_get_wifi_ifaces() {
                config_get iface_mode $1 mode
                if [ -n "$iface_mode" ]; then
                        case "$iface_mode" in
                                ap) append _ifaces $1 ;;
                                sta) append _staifaces $1  ;;
                        esac
                fi
        }
        config_foreach mac80211_get_wifi_ifaces wifi-iface

        for _mld in $_mlds
        do
                for _ifname in $_ifaces
                do
                        config_get mld_name $_ifname mld
                        config_get mldevice $_ifname device
                        config_get ht_mode  $mldevice htmode

                        if ! [[ "$mldevices" =~ "$mldevice" ]]; then
                                append mldevices $mldevice
                        fi

                        if [ -n "$ht_mode" ] && [[ $ht_mode == "EHT"* ]] && \
                           [ -n "$mld_name" ] &&  [ "$_mld" = "$mld_name" ]; then
                                mld_vaps_count=$((mld_vaps_count+1))
                        fi
                done
                for _staifname in $_staifaces
                do
                        config_get mld_name $_staifname mld
                        config_get mldevice $_staifname device
                        if ! [[ "$sta_mldevices" =~ "$mldevice" ]]; then
                                append sta_mldevices $mldevice
                        fi
                        if [ -n "$mld_name" ] &&  [ "$_mld" = "$mld_name" ]; then
                                sta_vaps_count=$((sta_vaps_count+1))
                        fi
                done
        done

        for mldev in $mldevices
        do
                # Length of radio name should be 12 in order to ensure only single wiphy wifi-devices are taken into account
                if [ ${#mldev} -ne 12 ]; then
                        continue;
                fi

                config_get disabled "$mldev" disabled

                if [ -z "$disabled" ] || [ "$disabled" -eq 0 ]; then
                        radio_up_count=$((radio_up_count+1))
                fi
        done

        for sta_mld in $sta_mldevices
        do
                if [ ${#sta_mld} -ne 12 ]; then
                        continue;
                fi
                config_get disabled "$sta_mld" disabled

                if [ -z "$disabled" ] || [ "$disabled" -eq 0 ]; then
                        sta_radio=$((sta_radio+1))
                fi
        done

}

mac80211_export_mld_info() {
	if [ -f $MLD_VAP_DETAILS ]; then
		source $MLD_VAP_DETAILS
		radio_up_count=$radio_up_count
		mld_vaps_count=$mld_vaps_count
		sta_radio=$sta_radio
		sta_vaps_count=$sta_vaps_count
	else
		mac80211_derive_ml_info
	fi
	mac80211_update_mld_configs
}

mac80211_get_mld_idx() {
	mld_name=$1
	config_load wireless
	mac80211_get_wifi_mlds() {
		append _mlds $1
	}
	config_foreach mac80211_get_wifi_mlds wifi-mld

	if [ -z "$_mlds" ]; then
		return
	fi

	index=0
	for _mld in $_mlds
	do
		if [ "$mld_name" == "$_mld" ]; then
			echo $index
			return
		else
			index=$((index+1))
		fi
	done
	return
}

get_sta_freq_list() {

	phy=$1
	sta_freq=$2

	hw_indices=$(iw phy ${phy} info | grep -e "channel list" | cut -d' ' -f 2)

	if [ -z $hw_indices ]; then
		#non-single wiphy arch doesn't need freq list
		return
	fi

	for i in $hw_indices
	do
		#fetch hw idx channels from phy info
		hw_nchans=$(iw phy ${phy} info | awk -v p1="$i channel list" -v p2="$((i+1)) channel list"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f')

		for _b in `iw phy $phy info | grep 'Band ' | cut -d' ' -f 2`; do
			expr="iw phy ${phy} info | awk  '/Band ${_b}/{ f = 1; next } /Band /{ f = 0 } f'"
			expr_freq="$expr | awk '/Frequencies/,/valid /f'"
			band_freq=$(eval ${expr_freq} | awk '{ print $2 }' | sed -e "s/\[//g" | sed -e "s/\]//g")

			# band_freq list has the sta freq in it
			if [[ "$band_freq" =~ "${sta_freq}" ]]; then
				sta_chan=$(eval $expr_freq | grep -E -m1 "(\* ${sta_freq:-....} MHz${sta_freq:+|\\[$sta_freq\\]})" | grep MHz | awk '{print $4}' | sed -e "s/\[//g" | sed -e "s/\]//g")

				#fetch band channels from phy info
				band_nchans=$(echo $(eval ${expr_freq} | awk '{ print $4 }' | sed -e "s/\[//g" | sed -e "s/\]//g") | tr -d ' ')
				hw_chans=$(echo $hw_nchans | tr -d ' ')

				#check if the list is present in band info
				if echo "$band_nchans" | grep -q "${hw_chans}";
				then
					found=false
					for chan in $hw_nchans
					do
						if [[ "$chan" == "$sta_chan" ]]; then
							found=true
						fi
					done
					if [[ "$found" == "true" ]]; then
						sta_freq_list=""
						for chidx in ${hw_nchans}; do
							frqs=$(eval $expr_freq | grep -E -m1 "(\* ${chidx:-....} MHz${chidx:+|\\[$chidx\\]})" | grep MHz | awk '{print $2}')
							sta_freq_list="${sta_freq_list}${frqs} "
							done
							echo $sta_freq_list
					fi
				fi
			else
				continue;
			fi
		done
	done
}

add_driver mac80211
