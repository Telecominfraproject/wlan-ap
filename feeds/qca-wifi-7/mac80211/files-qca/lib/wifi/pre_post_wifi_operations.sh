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

. /lib/netifd/netifd-wireless.sh
. /lib/netifd/wireless/mac80211.sh

append DRIVERS "mac80211"

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

update_primary_link()
{
	local mld_names
	local phy=$1
	config_load wireless

	mac80211_get_wifi_mlds() {
		append mld_names "$1"
	}
	config_foreach mac80211_get_wifi_mlds wifi-mld

	if [ -n "$mld_names" ]; then
		for mld_iface in $mld_names; do
			config_get mld_primary_link "$mld_iface" primary_link
			config_get mld_ifname "$mld_iface" ifname
			if [ -z "$mld_ifname" ]; then
				mld_ifname=$phy-$mld_iface
			fi
			if [ -n "$mld_primary_link" ]; then
				while true;
				do
					ifname_ap_state="$(hostapd_cli -i "$mld_ifname" status 2> /dev/null | grep state | cut -d'=' -f 2)"
					ifname_sta_state="$(wpa_cli -i "$mld_ifname" status 2> /dev/null | grep wpa_state | cut -d'=' -f 2)"
					if [ "$ifname_ap_state" = "ENABLED" ] || [ -n "$ifname_sta_state" ]; then
						if [ -f /sys/kernel/debug/ieee80211/phy"${phy#phy}"/netdev:"$mld_ifname"/primary_link ]; then
							echo "$mld_primary_link" > /sys/kernel/debug/ieee80211/phy"${phy#phy}"/netdev:"$mld_ifname"/primary_link
							break;
						fi
					fi
				done
			fi
		done
	fi
}

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

mlo_add_link() {
	local data
	local link
	local ssid
	local encryption
	local sae_pwe
	local key
	local channels
	local mld
	local iface_data
	local check_band result select_iface
	local hw_idx band check_disabled start_freq end_freq check_config

	if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ] || [ -z "$4" ]; then
		echo "command line inputs missing" > /dev/ttyMSM0
		echo "wifi mlo_add_link <phy> <band> <interface-name> <vap-mac-addr>" > /dev/ttyMSM0
		return
	fi

	mld_names=$(uci show wireless | grep "=wifi-mld"| cut -d "." -f 2 | cut -d "=" -f 1)
	for i in $mld_names; do
		ifname=$(uci get wireless.$i.ifname)
		[ -z "$ifname" ] && continue
		if [ "$ifname" = "$3" ]; then
			mld=$i
			break;
		fi
	done

	[ -n "$mld" ] || {
		echo "wrong interface name is given or mld doesn't found for given interface or ifname is not found in mld list" > /dev/ttyMSM0
		return
	}

	iface_data=$(uci show wireless | grep "'$mld'" | cut -d'.' -f2)

	case "$2" in
		2g)
		test_band=$(uci show wireless | grep "'$2'" | cut -d "." -f 2)
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		5g)
		test_band=$(uci show wireless | grep "'$2'" | cut -d "." -f 2)
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		5gl)
		test_band=$(uci show wireless | grep "'5g'" | cut -d "." -f 2)
		for iter in $test_band; do
			local_channel=$(uci show wireless.$iter.channel | cut -d "'" -f 2)
			if [ "$local_channel" -lt "65" ]; then
				test_band=$(echo $iter)
				break;
			fi
		done
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		5gh)
		test_band=$(uci show wireless | grep "'5g'" | cut -d "." -f 2)
		for iter in $test_band; do
			local_channel=$(uci show wireless.$iter.channel | cut -d "'" -f 2)
			if [ "$local_channel" -gt "65" ]; then
				test_band=$(echo $iter)
				break;
			fi
		done
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		6g)
		test_band=$(uci show wireless | grep "'$2'" | cut -d "." -f 2)
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		6gl)
		test_band=$(uci show wireless | grep "'6g'" | cut -d "." -f 2)
		for iter in $test_band; do
			local_channel=$(uci show wireless.$iter.channel | cut -d "'" -f 2)
			if [ "$local_channel" -lt "100" ]; then
				test_band=$(echo $iter)
				break;
			fi
		done
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		6gh)
		test_band=$(uci show wireless | grep "'6g'" | cut -d "." -f 2)
		for iter in $test_band; do
			local_channel=$(uci show wireless.$iter.channel | cut -d "'" -f 2)
			if [ "$local_channel" -gt "100" ]; then
				test_band=$(echo $iter)
				break;
			fi
		done
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		*) echo "wrong band is given" > /dev/ttyMSM0
		return;;
	esac

	link=$(uci show wireless | grep $channels | cut -d "." -f 2)
	[ -n "$link" ] || {
		echo "failed to find band number" > /dev/ttyMSM0
		return
	}
	hw_idx=$(uci show wireless.$link.radio | cut -d "'" -f 2)
	for iface in $iface_data; do
		check_band=$(uci show wireless.${iface}.device | awk -F"'" '{print $2}' | cut -d'.' -f2) 2> /dev/null
		check_disabled=$(uci show wireless.${iface}.disabled | cut -d "'" -f2) 2> /dev/null
		if [ "$check_band" = "$link" ] && [ "$check_disabled" = 0 ]; then
			echo "link is already present in the mld" > /dev/ttyMSM0
			return
		fi
		if [ "$check_band" = "$link" ] && [ "$check_disabled" = 1 ]; then
			uci set wireless.${iface}.disabled=0
			ssid=$(uci get wireless.${iface}.ssid)
			encryption=$(uci get wireless.${iface}.encryption)
			sae_pwe=$(uci get wireless.${iface}.sae_pwe)
			key=$(uci get wireless.${iface}.key)
			uci commit wireless
			select_iface=$(echo $iface)
			break;
		fi
	done
	if [ -z "$ssid" ]; then
		echo "Interface is already up or wireless config is not present or sequence remove link then add link is not followed" > /dev/ttyMSM0
		return
	fi
	echo 1 > /tmp/mlo_support.txt&
	band=$(echo $link | cut -d "_" -f 2)
	input_file=/var/run/hostapd-${1}_${band}.conf
	if [ ! -f $input_file ]; then
		input_file=/var/run/hostapd-${1}.${hw_idx}.conf
	fi

	check_config=$(cat $input_file | grep "$3$" | wc -l) 2> /dev/null

	if [ -f $input_file ] && [ "$check_config" -gt 0 ]; then
		# Use selected band interface hostapd config file to bring up interface.
		output_file=/tmp/hostapd-${link}.conf
		local interfaces=$(cat $input_file | grep -E 'interface=|bss=' | grep -v "interface=/" | cut -d "=" -f 2)
		local interface_channel iter_links found interface_freq interface_punct_bitmap partner_link default_link
		local found=0

		#Get interface channel and puncture bitmap information, helpful if channel switch happened.
		for iter in $interfaces; do
			partner_link=$(hostapd_cli -i $iter status | grep 'partner_link' | cut -d "[" -f 2 | cut -d "]" -f 1) 2> /dev/null
			default_link=$(hostapd_cli -i $iter status | grep 'link_id=' | cut -d "=" -f 2) 2> /dev/null
			iter_links=$(echo $partner_link $default_link)
			for iter_link in $iter_links; do
				interface_freq=$(hostapd_cli -i $iter -l $iter_link status | grep 'freq=' | cut -d "=" -f 2 | head -1) 2> /dev/null
				if [ "$interface_freq" -ge "$start_freq" ] && [ "$interface_freq" -le "$end_freq" ]; then
					interface_channel=$(hostapd_cli -i $iter -l $iter_link status | grep 'channel' | cut -d "=" -f 2 | head -1) 2> /dev/null
					if [ "$2" != "2g" ] && [ -n "$interface_channel" ]; then
						interface_punct_bitmap=$(hostapd_cli -i $iter -l $iter_link status | grep "punct_bitmap=" | cut -d "=" -f 2) 2> /dev/null
						echo "ru_punct_bitmap=$interface_punct_bitmap" >> $output_file
					fi
					found=1
					break;
				fi
			done
			if [ "$found" = 1 ]; then
				break;
			fi
		done

		#Generate temp hostapd config file.
		sed '/^interface=/,$d' "$input_file" > "$output_file"
		sed -n "/^bss=${3}$/,/^bssid=/p" "$input_file" >> "$output_file"
		sed -n "/^interface=${3}$/,/^bssid=/p" "$input_file" >> "$output_file"
		sed -i 's/^bss=/interface=/' "$output_file"

		echo "wpa_passphrase=$key" >> $output_file
		echo "ssid=$ssid" >> $output_file
		if [ $sae_pwe = 1 ]; then
			echo "sae_pwe=$sae_pwe" >> $output_file
		fi
		if [ $encryption = "sae" ]; then
			echo "wpa_key_mgmt=SAE" >> $output_file
		fi
		if [ $encryption = "sae-mixed" ]; then
			echo "wpa_key_mgmt=WPA-PSK SAE" >> $output_file
			echo "ieee80211w=1" >> $output_file
		fi

		echo "bssid=$4" >> $output_file
		echo "interface=$3" >> $output_file
		echo "bridge=br-lan" >> $output_file
		echo "wds_bridge=" >> $output_file
		echo "snoop_iface=br-lan" >> $output_file

		if [ "6g" = "$2" ]; then
			echo "mbssid=3" >> "$input_file"
			echo "mbssid_group_size=4" >> "$input_file"
		fi
		[ -n "$interface_channel" ] && echo "channel=$interface_channel" >> $output_file
		[ -n "$interface_punct_bitmap" ] && echo "ru_punct_bitmap=$interface_punct_bitmap" >> $output_file
		if [ -z "$hw_idx" ]; then
			result=$(hostapd_cli -i $3 mld_add_link bss_config=${1}:"$output_file")
		else
			result=$(hostapd_cli -i $3 mld_add_link bss_config=${1}.${hw_idx}:"$output_file")
		fi
		if [ "$result" != "OK" ]; then
			echo "failed to add the link" > /dev/ttyMSM0
			uci set wireless.${select_iface}.disabled=1
			uci commit wireless
			rm /tmp/mlo_support.txt 2>/dev/null
			rm "$output_file"
			return
		fi
		rm "$output_file"
	elif [ -f "$input_file" ]; then
		#Use partner interface hostapd config file to bring up interface.
		output_file=/tmp/hostapd-${link}.conf
		local interfaces=$(cat $input_file | grep -E 'interface=|bss=' | grep -v "interface=/" | cut -d "=" -f 2)
		local interface_channel iter_links found interface_freq interface_punct_bitmap partner_link default_link
		local found=0 partner_input_file

		#Get interface channel and puncture bitmap information, helpful if channel switch happened.
		for iter in $interfaces; do
			partner_link=$(hostapd_cli -i $iter status | grep 'partner_link' | cut -d "[" -f 2 | cut -d "]" -f 1) 2> /dev/null
			default_link=$(hostapd_cli -i $iter status | grep 'link_id=' | cut -d "=" -f 2) 2> /dev/null
			iter_links=$(echo $partner_link $default_link)
			for iter_link in $iter_links; do
				interface_freq=$(hostapd_cli -i $iter -l $iter_link status | grep 'freq=' | cut -d "=" -f 2 | head -1) 2> /dev/null
				if [ "$interface_freq" -ge "$start_freq" ] && [ "$interface_freq" -le "$end_freq" ]; then
					interface_channel=$(hostapd_cli -i $iter -l $iter_link status | grep 'channel' | cut -d "=" -f 2 | head -1) 2> /dev/null
					if [ "$2" != "2g" ] && [ -n "$interface_channel" ]; then
						interface_punct_bitmap=$(hostapd_cli -i $iter -l $iter_link status | grep "punct_bitmap=" | cut -d "=" -f 2) 2> /dev/null
						echo "ru_punct_bitmap=$interface_punct_bitmap" >> $output_file
					fi
					found=1
					break;
				fi
			done
			if [ "$found" = 1 ]; then
				break;
			fi
		done

		found=0

		#Get partner band hostapd config, to fetch the interface config
		for iface in $iface_data; do
			check_band=$(uci show wireless.${iface}.device | awk -F"'" '{print $2}' | cut -d'.' -f2) 2> /dev/null
			hw_idx=$(uci show wireless.$check_band.radio | cut -d "'" -f 2)
			band=$(echo $check_band | cut -d "_" -f 2)
			partner_input_file=/var/run/hostapd-${1}_${band}.conf
			if [ ! -f "$partner_input_file" ]; then
				partner_input_file=/var/run/hostapd-${1}.${hw_idx}.conf
			fi
			if [ -f "$partner_input_file" ]; then
				check_config=$(cat $partner_input_file | grep "$3$" | wc -l) 2> /dev/null
				if [ "$check_config" -gt 0 ]; then
					found=1
					break;
				fi
			fi
		done

		if [ "$found" = 0 ]; then
			echo "Not able to add new interface, failed to create config" > /dev/ttyMSM0
			uci set wireless.${select_iface}.disabled=1
			uci commit wireless
			rm "$output_file" 2>/dev/null
			rm /tmp/mlo_support.txt 2>/dev/null
			return
		fi

		#Generate temp hostapd config file.
		sed '/^interface=/,$d' "$input_file" > "$output_file"
		sed -n "/^bss=${3}$/,/^bssid=/p" "$partner_input_file" >> "$output_file"
		sed -n "/^interface=${3}$/,/^bssid=/p" "$partner_input_file" >> "$output_file"
		sed -i 's/^bss=/interface=/' "$output_file"

		echo "wpa_passphrase=$key" >> $output_file
		echo "ssid=$ssid" >> $output_file
		if [ $sae_pwe = 1 ]; then
			echo "sae_pwe=$sae_pwe" >> $output_file
		fi
		if [ $encryption = "sae" ]; then
			echo "wpa_key_mgmt=SAE" >> $output_file
		fi
		if [ $encryption = "sae-mixed" ]; then
			echo "wpa_key_mgmt=WPA-PSK SAE" >> $output_file
			echo "ieee80211w=1" >> $output_file
		fi

		echo "bssid=$4" >> $output_file
		echo "bridge=br-lan" >> $output_file
		echo "wds_bridge=" >> $output_file
		echo "snoop_iface=br-lan" >> $output_file

		if [ "6g" = "$2" ]; then
			echo "mbssid=3" >> "$input_file"
			echo "mbssid_group_size=4" >> "$input_file"
		fi
		[ -n "$interface_channel" ] && echo "channel=$interface_channel" >> $output_file
		[ -n "$interface_punct_bitmap" ] && echo "ru_punct_bitmap=$interface_punct_bitmap" >> $output_file
		if [ -z "$hw_idx" ]; then
			result=$(hostapd_cli -i $3 mld_add_link bss_config=${1}:"$output_file")
		else
			result=$(hostapd_cli -i $3 mld_add_link bss_config=${1}.${hw_idx}:"$output_file")
		fi
		if [ "$result" != "OK" ]; then
			echo "failed to add the link" > /dev/ttyMSM0
			uci set wireless.${select_iface}.disabled=1
			uci commit wireless
			rm "$output_file" 2>/dev/null
			rm /tmp/mlo_support.txt 2>/dev/null
			return
		fi
		rm "$output_file"
	else
		#Generate hostapd config in new link addition.
		ubus call network reload
		json_load "$(ubus_wifi_cmd "status" "${link}")"
		data=$(json_dump)
		data=$(echo "$data" | sed 's/.*\("config": { "path\)/\1/' | sed 's/}$//')
		data=$(echo "$data" | sed '$ s/..$/}/')
		data="{ $data"
		data=$(echo "$data" | sed -e 's/"interfaces": \[/"interfaces": { "0": /' -e 's/\} ]/} }/')
		start_string='"section"'
		end_string='"section": "${select_iface}"'
		start_index=$(echo "$data" | awk -v pat="$start_string" 'BEGIN{IGNORECASE=1} index($0,pat) {print index($0,pat)}')
		end_index=$(echo "$data" | awk -v pat="$end_string" 'BEGIN{IGNORECASE=1} index($0,pat) {print index($0,pat)}')
		m_data="${data:0:start_index}${data:end_index}"
		data="$m_data"
		data=$(echo "$data" | sed -e "s/\"section\": \"${select_iface}\"/\"bridge\": \"br-lan\", \"bridge_ifname\": \"br-lan\"/")
		data=$(echo "$data" | sed -e 's/\[\ ]/{ }/g' -e 's/"stations"/"stas"/g')
		json_select "${link}"
		_wdev_handler_1 "$data" "mac80211" "setup" "$link" 2> /dev/null
		json_select ..

		echo "bridge=br-lan" >>"$input_file"
		echo "wds_bridge=" >>"$input_file"
		echo "snoop_iface=br-lan" >>"$input_file"

		if [ "6g" = "$2" ]; then
			echo "mbssid=3" >> "$input_file"
			echo "mbssid_group_size=4" >> "$input_file"
		fi
		if [ -z "$hw_idx" ]; then
			result=$(hostapd_cli -i $3 mld_add_link bss_config=${1}:"$input_file")
		else
			result=$(hostapd_cli -i $3 mld_add_link bss_config=${1}.${hw_idx}:"$input_file")
		fi
		if [ "$result" != "OK" ]; then
			echo "failed to add the link" > /dev/ttyMSM0
			uci set wireless.${select_iface}.disabled=1
			uci commit wireless
			rm /tmp/mlo_support.txt 2>/dev/null
			return
		fi
	fi
	uci set wireless.${link}.disabled='0'
	uci commit wireless
	rm /tmp/mlo_support.txt 2>/dev/null
}

mlo_remove_link() {
	local link
	local mld mld_names ifname
	local iface_data
	local check_band
	local device_check
	local result
	local hw_idx band check_disabled start_freq end_freq check_device
	local link_id partner_link found default_link iter_links check_mld
	local found=0

	if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ] || [ -z "$4" ] || [ -z "$5" ]; then
		echo "command line inputs missing" > /dev/ttyMSM0
		echo "wifi mlo_remove_link <phyX> <band> <interface name> <link-id> <Tbtt-count (5-50)>" > /dev/ttyMSM0
		return
	fi

	mld_names=$(uci show wireless | grep "=wifi-mld"| cut -d "." -f 2 | cut -d "=" -f 1)
	for i in $mld_names; do
		ifname=$(uci get wireless.$i.ifname)
		[ -z "$ifname" ] && continue
		if [ "$ifname" = "$3" ]; then
			mld=$i
			break;
		fi
	done

	[ -n "$mld" ] || {
		echo "wrong interface name is given or mld doesn't found for given interface or ifname is not found in mld list" > /dev/ttyMSM0
		return
	}

	iface_data=$(uci show wireless | grep "'$mld'" | cut -d'.' -f2)
	[ -n "$iface_data" ] || {
		echo "link is not available to remove" > /dev/ttyMSM0
		return
	}

	case "$2" in
		2g)
		test_band=$(uci show wireless | grep "'$2'" | cut -d "." -f 2)
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		5g)
		test_band=$(uci show wireless | grep "'$2'" | cut -d "." -f 2)
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		5gl)
		test_band=$(uci show wireless | grep "'5g'" | cut -d "." -f 2)
		for iter in $test_band; do
			local_channel=$(uci show wireless.$iter.channel | cut -d "'" -f 2)
			if [ "$local_channel" -lt "65" ]; then
				test_band=$(echo $iter)
				break;
			fi
		done
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		5gh)
		test_band=$(uci show wireless | grep "'5g'" | cut -d "." -f 2)
		for iter in $test_band; do
			local_channel=$(uci show wireless.$iter.channel | cut -d "'" -f 2)
			if [ "$local_channel" -gt "65" ]; then
				test_band=$(echo $iter)
				break;
			fi
		done
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		6g)
		test_band=$(uci show wireless | grep "'$2'" | cut -d "." -f 2)
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		6gl)
		test_band=$(uci show wireless | grep "'6g'" | cut -d "." -f 2)
		for iter in $test_band; do
			local_channel=$(uci show wireless.$iter.channel | cut -d "'" -f 2)
			if [ "$local_channel" -lt "100" ]; then
				test_band=$(echo $iter)
				break;
			fi
		done
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		6gh)
		test_band=$(uci show wireless | grep "'6g'" | cut -d "." -f 2)
		for iter in $test_band; do
			local_channel=$(uci show wireless.$iter.channel | cut -d "'" -f 2)
			if [ "$local_channel" -gt "100" ]; then
				test_band=$(echo $iter)
				break;
			fi
		done
		radio_id=$(uci show wireless.$test_band.radio | cut -d "'" -f 2)
		start_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $3}')
		start_freq=$((start_freq+10))
		end_freq=$(iw $1 info | grep -A 2 "Idx $radio_id:" | grep "Frequency Range:" | awk '{print $6}')
		end_freq=$((end_freq-10))
		start_chan=$(mac80211_freq_to_channel $start_freq)
		end_chan=$(mac80211_freq_to_channel $end_freq)
		channels=$(echo $start_chan-$end_chan)
		;;
		*) echo "wrong band is given" > /dev/ttyMSM0
		return;;
	esac
	link=$(uci show wireless | grep $channels | cut -d "." -f 2)
	[ -n "$link" ] || {
		echo "failed to find band number" > /dev/ttyMSM0
		return
	}
	for i in $iface_data; do
		check_mld=$(uci show wireless.$i.mld | cut -d "'" -f 2)
		if [ "$check_mld" = "$mld" ]; then
			check_disabled=$(uci show wireless.$i.disabled | cut -d "'" -f 2) 2> /dev/null
			check_device=$(uci show wireless.$i.device | cut -d "'" -f 2)
			if [ "$check_disabled" = 1 ] && [ "$check_device" = "$link" ]; then
				echo "Interface is already disabled" > /dev/console
				return;
			fi
		fi
	done
	hw_idx=$(uci show wireless.$link.radio | cut -d "'" -f 2)
	band=$(echo $link | cut -d "_" -f 2)
	input_file=/var/run/hostapd-${1}_${band}.conf

	if [ ! -f $input_file ]; then
		input_file=/var/run/hostapd-${1}.${hw_idx}.conf
	fi

	if [ ! -f $input_file ]; then
		echo "Hostapd config file is not present link_id is not verifed, may cause issues if link_id is worng" > /dev/ttyMSM0
	fi

	partner_link=$(hostapd_cli -i $3 status | grep 'partner_link' | cut -d "[" -f 2 | cut -d "]" -f 1) 2> /dev/null
	default_link=$(hostapd_cli -i $3 status | grep 'link_id=' | cut -d "=" -f 2) 2> /dev/null
	iter_links=$(echo $partner_link $default_link)
	for iter_link in $iter_links; do
		interface_freq=$(hostapd_cli -i $3 -l $iter_link status | grep 'freq=' | cut -d "=" -f 2 | head -1) 2> /dev/null
		if [ "$interface_freq" -ge "$start_freq" ] && [ "$interface_freq" -le "$end_freq" ]; then
			link_id=$(($iter_link))
			found=1
			break;
		fi
	done

	if [ "$4" != "$link_id" ] || [ "$found" = 0 ]; then
		echo "Invalid link_id is given or band does not exists on interface $3" > /dev/ttyMSM0
		return;
	fi

	for iface in $iface_data; do
		check_mld=$(uci show wireless.${iface}.mld | cut -d "'" -f 2)
		check_band=$(uci show wireless.${iface}.device | awk -F"'" '{print $2}' | cut -d'.' -f2)
		if [ "$check_band" = "$link" ] && [ "$check_mld" = "$mld" ]; then
			result=$(hostapd_cli -i $3 -l $4 link_remove $5)
			if [ "$result" != "OK" ]; then
				echo "failed to remove the link" > /dev/ttyMSM0
				return
			fi
			uci set wireless.${iface}.disabled=1
			uci commit wireless
			device_check=$(uci show wireless | grep "device='$link")
			[ -z "$device_check" ] && uci set wireless.${link}.disabled='1' && uci commit wireless
			echo "link is removed" > /dev/ttyMSM0
			return
		fi
	done

	result=$(hostapd_cli -i $3 -l $4 link_remove $5)
	if [ "$result" != "OK" ]; then
		echo "failed to remove the link and wireless config for interface not found" > /dev/ttyMSM0
		return
	fi
}

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
		cmd="iw $phy service_class disable $svc_id"
		eval $cmd
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
	json_get_var per packet_error_rate
	json_get_var mcs_min mcs_min_threshold
	json_get_var mcs_max mcs_max_threshold
	json_get_var retries_thres retries_threshold

	[ -z "$min_thruput_rate" ] && min_thruput_rate='X'
	[ -z "$max_thruput_rate" ] && max_thruput_rate='X'
	[ -z "$burst_size" ] && burst_size='X'
	[ -z "$service_interval" ] && service_interval='X'
	[ -z "$delay_bound" ] && delay_bound='X'
	[ -z "$msdu_ttl" ] && msdu_ttl='X'
	[ -z "$msdu_rate_loss" ] && msdu_rate_loss='X'
	[ -z "$per" ] && per='X'
	[ -z "$mcs_min" ] && mcs_min='X'
	[ -z "$mcs_max" ] && mcs_max='X'
	[ -z "$retries_thres" ] && retries_thres='X'
	[ -z "$disable" ] && disable='0'

	if [ "$disable" -eq 0 ]; then
		cmd="iw $phy telemetry sla_thershold "$svc_id" "$min_thruput_rate" "$max_thruput_rate" "$burst_size" "$service_interval" "$delay_bound" "$msdu_ttl" "$msdu_rate_loss" "$per" "$mcs_min" "$mcs_max" "$retries_thres""
		echo "$svc_id" "$min_thruput_rate" "$max_thruput_rate" "$burst_size" "$service_interval" "$delay_bound" "$msdu_ttl" "$msdu_rate_loss" "$per" "$mcs_min" "$mcs_max" "$retries_thres"
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
		json_get_var per packet_error_rate
		json_get_var mcs_min mcs_min_threshold
		json_get_var mcs_max mcs_max_threshold
		json_get_var retries_thres retries_threshold
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

	cmd="iw $phy telemetry sla_detection_cfg num_packet 0 0 0 0 $delay_x_packet $ttl_drop_x_packet $msdu_loss_x_packet 0 0 0 0"
	eval $cmd
	cmd="iw $phy telemetry sla_detection_cfg per_second $min_throutput $max_throughput 0 0 0 0 0 $per $mcs_min $mcs_max $retries_thres"
	eval $cmd
	cmd="iw $phy telemetry sla_detection_cfg moving_avg 0 0 0 0 $delay_mov_avg 0 0 0 0 0 0"
	eval $cmd
	cmd="iw $phy telemetry sla_detection_cfg num_second 0 0 $burst_size $service_interval 0 $ttl_drop_x_sec $msdu_loss_x_sec 0 0 0 0"
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

pre_wifi_updown() {
	mac80211_update_mld_configs
	:
}

post_wifi_updown() {
	if [ -f "/lib/ftrace_enable_events.sh" ]; then
                sh /lib/ftrace_enable_events.sh
        fi
        if command -v udebug >/dev/null && ubus list udebug | grep -q .; then
                ubus call udebug set_config '{"service": {"hostapd": {"enabled": "1"}}}'
                udebug set_flag hostapd:wpa_nl_rx rx_frame=1
		ubus call udebug set_config '{"service": {"wpa_supplicant": {"enabled": "1","wpa_nl_rx": "1","wpa_nl_tx": "1"}}}'
        fi
	:
}

post_wifi_config() {
	post_mac80211 "update_pri_link"
	:
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
			rm -rf /tmp/*_freq_list 2>/dev/null
			if [ -f "$MLD_VAP_DETAILS" ]; then
				rm -rf $MLD_VAP_DETAILS
			fi
			if [ -f "/tmp/svc_configured" ]; then
				configure_service_class 0
				rm /tmp/svc_configured
			fi
			if [ -f "/tmp/apsta_mode.pid" ]; then
				pid=$(cat /tmp/apsta_mode.pid)
				kill -15 $pid 2>/dev/null
				rm /tmp/apsta_mode.pid 2>/dev/null
			fi
		;;
	esac
	return 0
}

set_primary_link() {
	PHY_PATH="/sys/kernel/debug/ieee80211"
	[ -d  "$PHY_PATH" ] && {
		for phy in $(ls $PHY_PATH 2>/dev/null); do
			update_primary_link "$phy"
		done
	}
}

post_mac80211() {
	local action=${1}

	case "${action}" in
		enable)
			if [ ! -f "/tmp/svc_configured" ]; then
				configure_service_class 1
				touch /tmp/svc_configured
				configure_telemetry_sla_samples
				configure_telemetry_sla_thersholds
				configure_telemetry_sla_detect
			fi
			set_primary_link
		;;
		update_pri_link)
			set_primary_link
		;;
	esac
	return 0
}
