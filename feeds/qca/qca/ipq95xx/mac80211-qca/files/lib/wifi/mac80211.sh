#!/bin/sh

append DRIVERS "mac80211"

MLD_VAP_DETAILS="/lib/netifd/wireless/wifi_mld_cfg.config"

configure_service_param() {
	enable_service=$2
	phy=$3
	json_load "$1"
	json_get_var svc_id svc_id
	json_get_var disable disable

	[ -z "$disable" ] && disable='0'

	if [ $enable_service -eq 1 ] && [ "$disable" -eq 0 ]; then
		json_get_var min_thruput_rate min_thruput_rate
		json_get_var max_thruput_rate max_thruput_rate
		json_get_var burst_size burst_size
		json_get_var service_interval service_interval
		json_get_var delay_bound delay_bound
		json_get_var msdu_ttl msdu_ttl
		json_get_var priority priority
		json_get_var tid tid
		json_get_var msdu_rate_loss msdu_rate_loss
		json_get_var ul_service_interval ul_service_interval
		json_get_var ul_min_tput ul_min_tput
		json_get_var ul_max_latency ul_max_latency
		json_get_var ul_burst_size ul_burst_size
		json_get_var ul_ofdma_disable ul_ofdma_disable
		json_get_var ul_mu_mimo_disable ul_mu_mimo_disable

		cmd="iw $phy service_class create $svc_id "
		[ ! -z "$min_thruput_rate" ] && cmd=$cmd"min_tput $min_thruput_rate "
		[ ! -z "$max_thruput_rate" ] && cmd=$cmd"max_tput $max_thruput_rate "
		[ ! -z "$burst_size" ] && cmd=$cmd"burst_size $burst_size "
		[ ! -z "$service_interval" ] && cmd=$cmd"service_interval $service_interval "
		[ ! -z "$delay_bound" ] && cmd=$cmd"delay_bound $delay_bound "
		[ ! -z "$msdu_ttl" ] && cmd=$cmd"msdu_ttl $msdu_ttl "
		[ ! -z "$priority" ] && cmd=$cmd"priority $priority "
		[ ! -z "$tid" ] && cmd=$cmd"tid $tid "
		[ ! -z "$msdu_rate_loss" ] && cmd=$cmd"msdu_loss $msdu_rate_loss "
		[ ! -z "$ul_service_interval" ] && cmd=$cmd"ul_service_interval $ul_service_interval "
		[ ! -z "$ul_min_tput" ] && cmd=$cmd"ul_min_tput $ul_min_tput "
		[ ! -z "$ul_max_latency" ] && cmd=$cmd"ul_max_latency $ul_max_latency "
		[ ! -z "$ul_burst_size" ] && cmd=$cmd"ul_burst_size $ul_burst_size "
		[ ! -z "$ul_ofdma_disable" ] && cmd=$cmd"ul_ofdma_disable $ul_ofdma_disable "
		[ ! -z "$ul_mu_mimo_disable" ] && cmd=$cmd"ul_mu_mimo_disable $ul_mu_mimo_disable "

		eval $cmd
	elif [ $enable_service -eq 0 ]; then
		check_svc_id=$(iw $phy service_class view $svc_id | grep "Service ID" | cut -d ":" -f2)
		if [ ! -z "$check_svc_id" ] && [ $check_svc_id -eq $svc_id ]; then
			cmd="iw $phy service_class disable $svc_id"
			eval $cmd
		fi
	fi
}

configure_service_class() {
	PHY_PATH="/sys/kernel/debug/ieee80211"
	phy_present=false
	if [ -d  $PHY_PATH ]
	then
		for phy in $(ls $PHY_PATH 2>/dev/null); do
			dir_name="$PHY_PATH/$phy/ath12k*"
			for dir in $dir_name; do
				[ -d $dir ] && phy_present=true && break
			done
			[ $phy_present = true ] && break
		done
	fi
	[ $phy_present = false ] && return

	json_init
	json_set_namespace default_ns
	json_load_file /lib/wifi/sawf/def_service_classes.json
	json_select service_class
	json_get_keys svc_class_indexes
	svc_class_index=0
	enable_svc=$1

	svc_class_index_count=$(echo "$svc_class_indexes" | wc -w)
	while [ $svc_class_index -lt $svc_class_index_count ]
	do
		svc_class_json=$(jsonfilter -i /lib/wifi/sawf/def_service_classes.json -e "@.service_class[$svc_class_index]")
		configure_service_param "$svc_class_json" "$enable_svc" "$phy"
		svc_class_index=$((svc_class_index+1))
	done

	json_set_namespace default_ns
	json_load_file /lib/wifi/sawf/service_classes.json
	json_select service_class
	json_get_keys svc_class_indexes
	svc_class_index=0

	svc_class_index_count=$(echo "$svc_class_indexes" | wc -w)
	while [ $svc_class_index -lt $svc_class_index_count ]
	do
		svc_class_json=$(jsonfilter -i /lib/wifi/sawf/service_classes.json -e "@.service_class[$svc_class_index]")
		configure_service_param "$svc_class_json" "$enable_svc" "$phy"
		svc_class_index=$((svc_class_index+1))
	done
}

configure_sla_param() {
	json_load "$1"
	json_get_var svc_id svc_id
	json_get_var disable disable
	json_get_var min_thruput_rate min_thruput_rate
	json_get_var max_thruput_rate max_thruput_rate
	json_get_var burst_size burst_size
	json_get_var service_interval service_interval
	json_get_var delay_bound delay_bound
	json_get_var msdu_ttl msdu_ttl
	json_get_var msdu_rate_loss msdu_rate_loss

	[ -z "$min_thruput_rate" ] && min_thruput_rate='X'
	[ -z "$max_thruput_rate" ] && max_thruput_rate='X'
	[ -z "$burst_size" ] && burst_size='X'
	[ -z "$service_interval" ] && service_interval='X'
	[ -z "$delay_bound" ] && delay_bound='X'
	[ -z "$msdu_ttl" ] && msdu_ttl='X'
	[ -z "$msdu_rate_loss" ] && msdu_rate_loss='X'
	[ -z "$disable" ] && disable='0'

	if [ "$disable" -eq 0 ]; then
		cmd="iw $phy telemetry sla_thershold "$svc_id" "$min_thruput_rate" "$max_thruput_rate" "$burst_size" "$service_interval" "$delay_bound" "$msdu_ttl" "$msdu_rate_loss""
		echo "$svc_id" "$min_thruput_rate" "$max_thruput_rate" "$burst_size" "$service_interval" "$delay_bound" "$msdu_ttl" "$msdu_rate_loss"
		eval $cmd
	fi
}

configure_telemetry_sla_thersholds() {
	json_init
	json_set_namespace sla_ns
	json_load_file /lib/wifi/sawf/telemetry/sla.json
	json_select sla
	json_get_keys sla_indexes
	sla_index=0

	sla_index_count=$(echo "$sla_indexes" | wc -w)

	echo "SLA Count: $sla_index_count" > /dev/console

	while [ $sla_index -lt $sla_index_count ]
	do
		sla_json=$(jsonfilter -i /lib/wifi/sawf/telemetry/sla.json -e "@.sla[$sla_index]")
		configure_sla_param "$sla_json"

		sla_index=$(expr $sla_index + 1)
	done
}

configure_telemetry_sla_detect() {
	json_init
	json_load_file /lib/wifi/sawf/telemetry/sla_detect.json
	json_select x_packet
		json_get_var delay_x_packet delay
		json_get_var msdu_loss_x_packet msdu_loss
		json_get_var ttl_drop_x_packet ttl_drop
	json_select ..
	json_select 1_second
		json_get_var min_throutput min_throutput
		json_get_var max_throughput max_throughput
	json_select ..
	json_select mov_average
		json_get_var delay_mov_avg delay
	json_select ..
	json_select x_second
		json_get_var service_interval service_interval
		json_get_var burst_size burst_size
		json_get_var msdu_loss_x_sec msdu_loss
		json_get_var ttl_drop_x_sec ttl_drop
	json_select ..

	cmd="iw $phy telemetry sla_detection_cfg num_packet 0 0 0 0 $delay_x_packet $ttl_drop_x_packet $msdu_loss_x_packet"
	eval $cmd
	cmd="iw $phy telemetry sla_detection_cfg per_second $min_throutput $max_throughput 0 0 0 0 0"
	eval $cmd
	cmd="iw $phy telemetry sla_detection_cfg moving_avg 0 0 0 0 $delay_mov_avg 0 0"
	eval $cmd
	cmd="iw $phy telemetry sla_detection_cfg num_second 0 0 $burst_size $service_interval 0 $ttl_drop_x_sec $msdu_loss_x_sec"
	eval $cmd
}

configure_telemetry_sla_samples() {
	json_init
	json_load_file /lib/wifi/sawf/telemetry/config.json

# Parsing the moving average params
	json_get_var mavg_num_packet mavg_num_packet
	json_get_var mavg_num_window mavg_num_window

# Parsing the sla params
	json_get_var sla_num_packet sla_num_packet
	json_get_var sla_time_secs sla_time_secs

	iw $phy telemetry sla_samples_cfg "$mavg_num_packet" "$mavg_num_window" "$sla_num_packet" "$sla_time_secs"
}

update_mld_vap_details() {
	local _mlds
	local _devices_up
	local _ifaces
	config_load wireless
	mld_vaps_count=0
	radio_up_count=0
	sta_vaps_count=0
	sta_radio=0

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
			config_get mlcaps  $mldevice mlo_capable
			if ! [[ "$mldevices" =~ "$mldevice" ]]; then
				append mldevices $mldevice
			fi

			if [ -n "$mlcaps" ] && [ $mlcaps -eq 1 ] && \
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

	echo "radio_up_count=$radio_up_count mld_vaps_count=$mld_vaps_count" > $MLD_VAP_DETAILS
	echo "sta_radio=$sta_radio sta_vaps_count=$sta_vaps_count" >> $MLD_VAP_DETAILS
}

pre_wifi_updown() {
	:
}

post_wifi_updown() {
	:
}

check_mac80211_device() {
	local device="$1"
	local path="$2"
	local macaddr="$3"

	[ -n "$found" ] && return 0

	phy_path=
	config_get phy "$device" phy
	[ -n "$phy" ] && case "$phy" in
		phy*)
			[ -d /sys/class/ieee80211/$phy ] && \
				phy_path="$(iwinfo nl80211 path "$dev")"
		;;
		*)
			if json_is_a "$phy" object; then
				json_select "$phy"
				json_get_var phy_path path
				json_select ..
			elif json_is_a "${phy%.*}" object; then
				json_select "${phy%.*}"
				json_get_var phy_path path
				json_select ..
				phy_path="$phy_path+${phy##*.}"
			fi
		;;
	esac
	json_select ..
	[ -n "$phy_path" ] || config_get phy_path "$device" path
	[ -n "$path" -a "$phy_path" = "$path" ] && {
		found=1
		return 0
	}

	config_get dev_macaddr "$device" macaddr

	[ -n "$macaddr" -a "$dev_macaddr" = "$macaddr" ] && found=1

	return 0
}


__get_band_defaults() {
	local phy="$1"

	( iw phy "$phy" info; echo ) | awk '
BEGIN {
        bands = ""
}

($1 == "Band" || $1 == "") && band {
        if (channel) {
		mode="NOHT"
		if (ht) mode="HT20"
		if (vht && band != "1:") mode="VHT80"
		if (he) mode="HE80"
		if (he && band == "1:") mode="HE20"
		if (eht && band == "1:") mode="EHT20"
		if (eht && band == "2:") mode="EHT80"
		if (eht && band == "4:") mode="EHT160"
                sub("\\[", "", channel)
                sub("\\]", "", channel)
                bands = bands band channel ":" mode " "
        }
        band=""
}

$1 == "Band" {
        band = $2
        channel = ""
	vht = ""
	ht = ""
	he = ""
	eht = ""
}

$0 ~ "Capabilities:" {
	ht=1
}

$0 ~ "VHT Capabilities" {
	vht=1
}

$0 ~ "HE Iftypes" {
	he=1
}

$0 ~ "EHT Iftypes" {
	eht=1
}

$0 ~ "EHT MAC Capabilities (0x0000" {
	eht=0
}


$1 == "*" && $3 == "MHz" && $0 !~ /disabled/ && band && !channel {
        channel = $4
}

END {
        print bands
}'
}

get_band_defaults() {
	local phy="$1"

	for c in $(__get_band_defaults "$phy"); do
		local band="${c%%:*}"
		c="${c#*:}"
		local chan="${c%%:*}"
		c="${c#*:}"
		local mode="${c%%:*}"
		case "$band" in
			1) band=2g;;
			2) band=5g;;
			3) band=60g;;
			4) band=6g;;
			*) band="";;
		esac

		[ -n "$band" ] || continue

		append mode_band $band
		append channel $chan
		append htmode $mode
	done
}

check_devidx() {
	case "$1" in
	radio[0-9]*)
		local idx="${1#radio}"
		[ "$devidx" -ge "${1#radio}" ] && devidx=$((idx + 1))
		;;
	esac
}

pre_mac80211() {
	local action=${1}
	case "${action}" in
		disable)
                        has_updated_cfg=$(ls /var/run/hostapd-*-updated-cfg 2>/dev/null | wc -l)
                        if [ "$has_updated_cfg" -gt 0 ]; then
                                rm -rf /var/run/hostapd-*updated-cfg
                        fi
                        rm -rf /var/run/wpa_supplicant-*-updated-cfg  2>/dev/null
			if [ -f "$MLD_VAP_DETAILS" ]; then
				rm -rf $MLD_VAP_DETAILS
			fi
			sawf_supp="/sys/module/ath12k/parameters/sawf"
			if [ -f $sawf_supp ] && [ $(cat $sawf_supp) == "Y" ]; then
				configure_service_class 0
			fi
		;;
	esac
	return 0
}

post_mac80211() {
	local action=${1}

	case "${action}" in
		enable)
			sawf_supp="/sys/module/ath12k/parameters/sawf"
			if [ -f $sawf_supp ] && [ $(cat $sawf_supp) == "Y" ]; then
				configure_service_class 1
				configure_telemetry_sla_samples
				configure_telemetry_sla_thersholds
				configure_telemetry_sla_detect
			fi
		;;
	esac
	return 0
}

mac80211_validate_num_channels() {
	dev=$1
	n_hw_idx=$2
	efreq=$3
	match_found=0
	bandidx=$4
	sub_matched=0
	i=0

	#fetch the band channel list
	band_nchans=$(eval ${3} | awk '{ print $4 }' | sed -e "s/\[//g" | sed -e "s/\]//g")
	band_first_chan=$(echo $band_nchans | awk '{print $1}')

	#entire band channel list without any separator
	band_nchans=$(echo $band_nchans | tr -d ' ')

	while [ $i -lt $n_hw_idx ]; do

		#fetch the hw idx channel list
		hw_nchans=$(iw phy ${dev} info | awk -v p1="$i channel list" -v p2="$((i+1)) channel list"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f')
		first_chan=$(echo $hw_nchans | awk '{print $1}')
		hw_nchans=$(echo $hw_nchans | tr -d ' ')

		if [ "$band_nchans" = "$hw_nchans" ]; then
			match_found=1
		else
			#check if subchannels matches
			if echo "$band_nchans" | grep -q "${hw_nchans}";
			then
				sub_matched=$((sub_matched+1))
				append chans $first_chan
			fi
		fi
		i=$((i+1))
	done
	if [ $match_found -eq 0 ]; then
		if [ $sub_matched -gt 1 ]; then
                        echo "$chans"
		fi
	else
		echo ""
	fi
}

mac80211_get_channel_list() {
	dev=$1
	n_hw_idx=$2
	chan=$3
	i=0
	match_found=0

	while [ $i -lt $n_hw_idx ]; do
		hw_nchans=$(iw phy ${dev} info | awk -v p1="$i channel list" -v p2="$((i+1)) channel list"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f')
		first_chan=$(echo $hw_nchans | awk '{print $1}')
		higest_chan=$first_chan
		for chidx in $hw_nchans; do
			if [ $chidx -gt $higest_chan ]; then
				higest_chan=$chidx;
			fi
			if [ "$chidx" == "$chan" ]; then
				match_found=1
			fi
		done
		if [ $match_found -eq 1 ]; then
			break;
		fi
		i=$((i+1))
	done

	if [ $match_found -eq 1 ]; then
		echo "$first_chan-$higest_chan";
	else
		echo ""
	fi
}

detect_mac80211() {
	devidx=0
	config_load wireless
	config_foreach check_devidx wifi-device

	json_load_file /etc/board.json

	for _dev in /sys/class/ieee80211/*; do
		[ -e "$_dev" ] || continue

		dev="${_dev##*/}"

		mode_band=""
		channel=""
		htmode=""
		ht_capab=""
		bandidx=1
		mode_bandidx=1
		#Check the single wiphy support
		total_bands=$(iw phy ${dev} info | grep -E 'Band ' | wc -l)
		if [ $total_bands -gt 1 ]; then
			is_swiphy=1
		fi
		no_hw_idx=$(iw phy ${dev} info | grep -e "channel list" | wc -l)

		get_band_defaults "$dev"

		if [ $no_hw_idx -gt 0 ]; then
			iter=$no_hw_idx
		else
			iter=$total_bands
		fi

		uci -q set wireless.mac80211=smp_affinity
		uci -q set wireless.mac80211.enable_smp_affinity=1
		uci -q set wireless.mac80211.enable_color=1

		while [ $bandidx -le $iter ]
		do
			_mode_band=$(eval echo $mode_band | awk -v I=$mode_bandidx '{print $I}')
			_channel=$(eval echo $channel | awk -v I=$mode_bandidx '{print $I}')
			if [ $_mode_band == '6g' ]; then
				_channel=49
			elif [ $_mode_band == '2g' ]; then
				_channel=6
			fi
			_htmode=$(eval echo $htmode | awk -v I=$mode_bandidx '{print $I}')
			mode_bandidx=$(($mode_bandidx + 1))

			path="$(iwinfo nl80211 path "$dev")"
			macaddr="$(cat /sys/class/ieee80211/${dev}/macaddress)"

			# work around phy rename related race condition
			if ! [ -n "$path" ] || ! [ -n "$macaddr" ]; then
				bandidx=$((bandidx + 1))
				continue
			fi

			found=
			config_foreach check_mac80211_device wifi-device "$path" "$macaddr"
			if [ -n "$found" ]; then
				bandidx=$(($bandidx + 1))
				continue
			fi
			if [ $is_swiphy ]; then
				name=""radio$devidx\_band$(($bandidx - 1))""
				expr="iw phy ${dev} info | awk  '/Band ${bandidx}/{ f = 1; next } /Band /{ f = 0 } f'"
			else
				name="radio${devidx}"
				expr="iw phy ${dev} info"
			fi

			expr_freq="$expr | awk '/Frequencies/,/valid /f'"
			if [ $no_hw_idx -gt $total_bands ]; then
				need_extraconfig=$(mac80211_validate_num_channels $dev $no_hw_idx "$expr_freq")
				need_extraconfig=$(eval echo "${need_extraconfig}" | tr ' ' '\n' | sort -n)
			fi

			case "$dev" in
				phy*)
					if [ -n "$path" ]; then
						dev_id="set wireless.${name}.path='$path'"
					else
						dev_id="set wireless.${name}.macaddr='$macaddr'"
					fi
					;;
				*)
					dev_id="set wireless.${name}.phy='$dev'"
					;;
			esac

			# We may need to handle similar logic for 6g band in future if it has split phy.

			if [ ${_mode_band} == '5g' ] && [ -n "$need_extraconfig" ]; then
				splitphy=1
				for chan in ${need_extraconfig}
				do
					if [ $chan -eq 100 ]; then
						chan=149
					fi
					chan_list=$(mac80211_get_channel_list $dev $no_hw_idx $chan)
					uci -q batch <<-EOF
						set wireless.${name}=wifi-device
						set wireless.${name}.type=mac80211
						${dev_id}
						set wireless.${name}.channel=${chan}
						set wireless.${name}.channels=${chan_list}
						set wireless.${name}.band=${_mode_band}
						set wireless.${name}.htmode=$_htmode
						set wireless.${name}.disabled=1

						set wireless.default_${name}=wifi-iface
						set wireless.default_${name}.device=${name}
						set wireless.default_${name}.network=lan
						set wireless.default_${name}.mode=ap
						set wireless.default_${name}.ssid=prplOS
						set wireless.default_${name}.encryption=none

						set wireless.guest_${name}=wifi-iface
						set wireless.guest_${name}.device=${name}
						set wireless.guest_${name}.network=guest
						set wireless.guest_${name}.mode=ap
						set wireless.guest_${name}.ssid=prplOS-guest
						set wireless.guest_${name}.encryption=none
				EOF
					if [ $is_swiphy ] && [ $splitphy -gt 0 ]; then
						bandidx=$(($bandidx + 1))
						name=""radio$devidx\_band$(($bandidx - 1))""
						splitphy=$(($splitphy - 1))
					else
						name="radio${devidx}"
					fi
		                        case "$dev" in
						phy*)
							if [ -n "$path" ]; then
								dev_id="set wireless.${name}.path='$path'"
							else
								dev_id="set wireless.${name}.macaddr='$macaddr'"
							fi
							;;
						*)
							dev_id="set wireless.${name}.phy='$dev'"
							;;
					esac
					uci -q commit wireless
				done
			else
				chan_list=$(mac80211_get_channel_list $dev $no_hw_idx $_channel)
				uci -q batch <<-EOF
					set wireless.${name}=wifi-device
					set wireless.${name}.type=mac80211
					${dev_id}
					set wireless.${name}.channel=${_channel}
					set wireless.${name}.channels=${chan_list}
					set wireless.${name}.band=${_mode_band}
					set wireless.${name}.htmode=$_htmode
					set wireless.${name}.disabled=1

					set wireless.default_${name}=wifi-iface
					set wireless.default_${name}.device=${name}
					set wireless.default_${name}.network=lan
					set wireless.default_${name}.mode=ap
					set wireless.default_${name}.ssid=prplOS

					set wireless.guest_${name}=wifi-iface
					set wireless.guest_${name}.device=${name}
					set wireless.guest_${name}.network=guest
					set wireless.guest_${name}.mode=ap
					set wireless.guest_${name}.ssid=prplOS-guest
			EOF
				if [ ${_mode_band} == '6g'  ]; then
					uci -q batch <<-EOF
						set wireless.default_${name}.encryption=sae
						set wireless.default_${name}.sae_pwe=1
						set wireless.default_${name}.key=0123456789

						set wireless.guest_${name}.encryption=sae
						set wireless.guest_${name}.sae_pwe=1
						set wireless.guest_${name}.key=0123456789
				EOF
				else
					uci -q set wireless.default_${name}.encryption=none

					uci -q set wireless.guest_${name}.encryption=none
				fi
				uci -q commit wireless
			fi
			bandidx=$(($bandidx + 1))
		done
		devidx=$(($devidx + 1))
	done
}
