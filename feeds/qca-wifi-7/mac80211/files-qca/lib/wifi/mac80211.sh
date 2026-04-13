#!/bin/sh
. /lib/netifd/netifd-wireless.sh
. /lib/netifd/wireless/mac80211.sh

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
		append band_num $band
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

mac80211_validate_num_channels() {
	dev=$1
	n_hw_idx=$2
	sub_matched=0
	i=0

	while [ $i -lt $n_hw_idx ]; do

		start_freq=$(iw phy ${dev} info | awk -v p1="Idx $i" -v p2="Radio's valid interface combinations"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f'| cut -d " " -f 3)
		end_freq=$(iw phy ${dev} info | awk -v p1="Idx $i" -v p2="Radio's valid interface combinations"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f'| cut -d " " -f 6)
		start_freq=$((start_freq+10))
		end_freq=$((end_freq-10))
		first_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)

		if [ $end_chan = 177 ] && [ $_mode_band = "5g" ]; then
			sub_matched=$((sub_matched+1))
			append chans $first_chan
		fi
		if [ $end_chan = 233 ] && [ $_mode_band = "6g" ]; then
			sub_matched=$((sub_matched+1))
			append chans $first_chan
		fi
		if [ $end_chan = 64 ] && [ $_mode_band = "5g" ]; then
			sub_matched=$((sub_matched+1))
			append chans $first_chan
		fi
		if [ $end_chan = 93 ] && [ $_mode_band = "6g" ]; then
			sub_matched=$((sub_matched+1))
			append chans $first_chan
		fi

		i=$((i+1))
	done

	if [ $sub_matched -gt 1 ]; then
		echo "$chans"
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
		start_freq=$(iw phy ${dev} info | awk -v p1="Idx $i" -v p2="Radio's valid interface combinations"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f'| cut -d " " -f 3)
		end_freq=$(iw phy ${dev} info | awk -v p1="Idx $i" -v p2="Radio's valid interface combinations"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f'| cut -d " " -f 6)
		start_freq=$((start_freq+10))
		end_freq=$((end_freq-10))
		first_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		if [ "$end_chan" = "14" ] && [ "$_mode_band" = "2g" ]; then
			match_found=1
			break;
		fi
		if [ "$first_chan" -le "$chan" ] && [ "$end_chan" = "64" ] && [ "$end_chan" -ge "$chan" ] && [ "$_mode_band" = "5g" ]; then
			match_found=1
			break;
		fi
		if [ "$first_chan" -le "$chan" ] && [ "$end_chan" = "177" ] && [ "$end_chan" -ge "$chan" ] && [ "$_mode_band" = "5g" ]; then
			match_found=1
			break;
		fi
		if [ "$first_chan" -le "$chan" ] && [ "$end_chan" = "93" ] && [ "$end_chan" -ge "$chan" ] && [ "$_mode_band" = "6g" ]; then
			match_found=1
			break;
		fi
		if [ "$first_chan" -le "$chan" ] && [ "$end_chan" = "233" ] && [ "$end_chan" -ge "$chan" ] && [ "$_mode_band" = "6g" ]; then
			match_found=1
			break;
		fi

		i=$((i+1))
	done

	if [ "$match_found" -eq "1" ]; then
		echo "$first_chan-$end_chan";
	else
		echo ""
	fi
}

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

generate_5g_6g_split_phy_config() {
	splitphy=1
	for chan in ${need_extraconfig}
	do
		if [ ${_mode_band} == '5g' ]; then
			if [ $chan -eq 100 ]; then
				chan=149
			fi
		else
			if [ $chan -eq 2 ]; then
				chan=49
			else
				chan=197
			fi
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
		set wireless.default_${name}.ssid=OpenWrt
	EOF
		if [ ${_mode_band} == '5g'  ]; then
			uci set wireless.default_${name}.encryption=none
		else
			uci -q batch <<-EOF
			set wireless.default_${name}.encryption=sae
			set wireless.default_${name}.sae_pwe=1
			set wireless.default_${name}.key=0123456789
		EOF
		fi

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
}

detect_mac80211() {
	devidx=0
	config_load wireless
	config_foreach check_devidx wifi-device

	json_load_file /etc/board.json

	for _dev in /sys/class/ieee80211/*; do
		[ -e "$_dev" ] || continue

		dev="${_dev##*/}"

		band_num=""
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
		no_hw_idx=$(iw phy ${dev} info | grep -e "Idx" | wc -l)

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
			_band_num=$(eval echo $band_num | awk -v I=$mode_bandidx '{print $I}')
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
				expr="iw phy ${dev} info | awk  '/Band ${_band_num}/{ f = 1; next } /Band /{ f = 0 } f'"
			else
				name="radio${devidx}"
				expr="iw phy ${dev} info"
			fi

			expr_freq="$expr | awk '/Frequencies/,/valid /f'"
			if [ $no_hw_idx -gt $total_bands ]; then
				need_extraconfig=$(mac80211_validate_num_channels $dev $no_hw_idx)
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

			if ([ ${_mode_band} == '5g' ] || [ ${_mode_band} == '6g' ]) && [ -n "$need_extraconfig" ]; then
				generate_5g_6g_split_phy_config
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
					set wireless.default_${name}.ssid=OpenWrt
			EOF
				if [ ${_mode_band} == '6g'  ]; then
					uci -q batch <<-EOF
						set wireless.default_${name}.encryption=sae
						set wireless.default_${name}.sae_pwe=1
						set wireless.default_${name}.key=0123456789
				EOF
				else
					uci -q set wireless.default_${name}.encryption=none
				fi
				uci -q commit wireless
			fi
			bandidx=$(($bandidx + 1))
		done
		devidx=$(($devidx + 1))
	done
}
