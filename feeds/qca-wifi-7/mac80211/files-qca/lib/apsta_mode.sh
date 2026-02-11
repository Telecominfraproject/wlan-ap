#!bin/sh
#
# Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
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

. /lib/netifd/wireless/mac80211.sh

sta_intf="$1"
ap_intfs="$2"
hostapd_conf="$3"
dfs_log=1
phy="$4"
ml_link=""
ap_ht_capab=$(cat $hostapd_conf 2> /dev/null | grep ht_capab | grep -v vht | cut -d'=' -f 2)

# Hostapd VHT and HE calculations
hostapd_vht_he_eht_oper_chwidth() {
	local sta_width="$1"
	case $sta_width in
		"80")
			ap_vht_he_eht_oper_chwidth=1;;
		"160")
			ap_vht_he_eht_oper_chwidth=2;;
		"80+80")
			ap_vht_he_eht_oper_chwidth=3;;
		"320" )
			ap_vht_he_eht_oper_chwidth=9;;
		"20"|"40"|*)
			ap_vht_he_eht_oper_chwidth=0;;
	esac
}


hostapd_vht_he_eht_oper_centr_freq_seg0_idx() {
	local sta_width="$1"
	local sta_channel="$2"
	# Frequency is needed for specially handling 6 GHz channels
	local freq="$3"
	local intf="$4"

	case $ap_vht_he_eht_oper_chwidth in
		# 20/40 MHz chan width
		"0")
			case $sta_width in
			"20")
				ap_vht_he_eht_oper_centr_freq_seg0_idx=$sta_channel;;
			"40")
				if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
					case "$(( ($sta_channel / 4) % 2 ))" in
						1) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 2));;
						0) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 2));;
					esac
				elif [ $sta_channel -lt 7 ]; then
					ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 2))
				elif [ $sta_channel -lt 36 ]; then
					ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 2))
				else
					case "$(( ($sta_channel / 4) % 2 ))" in
						1) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 2));;
						0) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 2));;
					esac
				fi
			;;
			esac
		;;
		# 80 MHz chan width
		"1")
			if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
				case "$(( ($sta_channel / 4) % 4 ))" in
					0) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 6));;
					1) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 2));;
					2) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 2));;
					3) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 6));;
				esac
			elif [ $freq != 5935 ]; then
				case "$(( ($sta_channel / 4) % 4 ))" in
					1) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 6));;
					2) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 2));;
					3) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 2));;
					0) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 6));;
				esac
			fi
		;;
		# 160 MHz chan width
		"2")
			# Freq based division is required since for 5 GHz, no need to have seg0 for DFS channels
			if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
				case "$sta_channel" in
					1|5|9|13|17|21|25|29) ap_vht_he_eht_oper_centr_freq_seg0_idx=15;;
					33|37|41|45|49|53|57|61) ap_vht_he_eht_oper_centr_freq_seg0_idx=47;;
					65|69|73|77|81|85|89|93) ap_vht_he_eht_oper_centr_freq_seg0_idx=79;;
					97|101|105|109|113|117|121|125) ap_vht_he_eht_oper_centr_freq_seg0_idx=111;;
					129|133|137|141|145|149|153|157) ap_vht_he_eht_oper_centr_freq_seg0_idx=143;;
					161|165|169|173|177|181|185|189) ap_vht_he_eht_oper_centr_freq_seg0_idx=175;;
					193|197|201|205|209|213|217|221) ap_vht_he_eht_oper_centr_freq_seg0_idx=207;;
				esac
			elif [ $freq != 5935 ]; then
				case "$sta_channel" in
					36|40|44|48|52|56|60|64) ap_vht_he_eht_oper_centr_freq_seg0_idx=50;;
					100|104|108|112|116|120|124|128) ap_vht_he_eht_oper_centr_freq_seg0_idx=114;;
				esac
			fi
		;;
                "9" )
			if [ $freq -ge 5955 ] && [ $freq -le 7115 ]; then
				eht_oper_centr_freq_val=$(iw dev $intf info | grep $freq | awk '{print $9}')
				ap_vht_he_eht_oper_centr_freq_seg0_idx=$((($eht_oper_centr_freq_val - 5950) / 5))
			elif [ $freq -ge 5500 ] && [ $freq -le 5730 ]; then
				ap_vht_he_eht_oper_centr_freq_seg0_idx=130
			fi
	esac
}

# Hostapd HT40 secondary channel offset calculations
hostapd_ht40_mode() {
	local sta_channel="$1"

	# 5Ghz channels
	if [ $sta_channel -ge 36 ]; then
		case "$(( ($sta_channel / 4) % 2 ))" in
			1) if [ ! -z $ap_ht_mode ]; then
				ap_ht_mode="$(echo $ap_ht_mode | sed -e "s/HT40-/HT40+/g")"
			   else
				ap_ht_mode="[HT40+]"
			   fi
			;;
			0) if [ ! -z $ap_ht_mode ]; then
				ap_ht_mode="$(echo $ap_ht_mode | sed -e "s/HT40+/HT40-/g")"
			   else
				ap_ht_mode="[HT40-]"
			   fi
			;;
		esac
	else
		# 2.4Ghz channels
		if [ "$sta_channel" -lt 7 ]; then
			if [ ! -z $ap_ht_mode ]; then
				ap_ht_mode="$(echo $ap_ht_mode | sed -e "s/HT40-/HT40+/g")"
                        else
                                ap_ht_mode="[HT40+]"
                        fi
		else
			if [ ! -z $ap_ht_mode ]; then
				ap_ht_mode="$(echo $ap_ht_mode | sed -e "s/HT40+/HT40-/g")"
                        else
                                ap_ht_mode="[HT40-]"
                        fi
		fi
	fi
	#echo "Secondary offset is $ap_ht_mode" > /dev/ttyMSM0
	hostapd_cli -i $ap_intf $ml_link set ht_capab $ap_ht_mode 2> /dev/null
}

# Hostapd HT20 mode
hostapd_ht20_mode() {
        local ht_capab_20=$(echo $ap_ht_capab | sed -e 's/\(\[HT40*+*-*]\)//g')
        #echo "Setting HT capab $ht_capab_20" > /dev/ttyMSM0
        hostapd_cli -i $ap_intf $ml_link set ht_capab $ht_capab_20 2> /dev/null
}

get_ap_ht_capab() {
        sta_freq=$1
        sta_radio_band_idx=$(ls /var/run/wpa_supplicant-*-updated-cfg | awk '{ print $1 }' | cut -f2 -d"-" | awk '{print substr($0,length,1)}')
        for i in $sta_radio_band_idx
        do
                freq_list=$(cat /var/run/wpa_supplicant-radio${phy: -1}_band${i}-updated-cfg | grep freq_list | cut -d'=' -f 2)
                highest_freq=$(echo "$freq_list" | awk '{print $NF}')
                least_freq=$(echo "$freq_list" | cut -d ' ' -f1)
                if [ "$sta_freq" -ge "$least_freq" ] && [ "$sta_freq" -le "$highest_freq" ]; then
			hostapd_conf="/var/run/hostapd-${phy}_band$i.conf"
			ap_ht_capab=$(cat $hostapd_conf 2> /dev/null | grep ht_capab | grep -v vht | cut -d'=' -f 2)
                fi
        done
}

hostapd_set_op_class() {
    local freq=$1
    local channel=$2
    local width=$3
    local intf=$4
    local sec_channel=0
    local vht_he_opclass=0

    centre_freq=$(iw dev $intf info | grep $freq | awk '{print $9}')

    # Determine sec_channel based on ap_ht_mode if width is 40
    if [ "$width" -eq 40 ]; then
	    if [ "$centre_freq" -gt "$freq" ]; then
		    sec_channel=1
	    elif [ "$centre_freq" -lt "$freq" ]; then
		    sec_channel=-1
	    else
		    sec_channel=0
	    fi
    fi

    # 2.4 GHz frequency range
    if [ "$freq" -ge 2412 ] && [ "$freq" -le 2484 ]; then
        if [ "$width" -eq 40 ]; then
            case "$sec_channel" in
                1) op_class=83 ;;
                -1) op_class=84 ;;
                *) op_class=81 ;;
            esac
        elif [ "$width" -eq 20 ]; then
            if [ "$freq" -eq 2484 ]; then
                op_class=82
            else
                op_class=81
            fi
        fi
    fi

    # 5 GHz frequency range
    if [ "$freq" -ge 5180 ] && [ "$freq" -le 5885 ]; then
        case "$width" in
            80) vht_he_opclass=128 ;;  # 80 MHz
            160) vht_he_opclass=129 ;;  # 160 MHz
            "80+80") vht_he_opclass=130 ;;  # 80+80 MHz
            *)
        esac

        if [ "$freq" -ge 5180 ] && [ "$freq" -le 5240 ]; then
            if [ "$vht_he_opclass" -ne 0 ]; then
                op_class=$vht_he_opclass
            else
                case "$sec_channel" in
                    1) op_class=116 ;;
                    -1) op_class=117 ;;
                    *) op_class=115 ;;
                esac
            fi
        elif [ "$freq" -ge 5260 ] && [ "$freq" -le 5320 ]; then
            if [ "$vht_he_opclass" -ne 0 ]; then
                op_class=$vht_he_opclass
            else
                case "$sec_channel" in
                    1) op_class=119 ;;
                    -1) op_class=120 ;;
                    *) op_class=118 ;;
                esac
            fi
        elif [ "$freq" -ge 5745 ] && [ "$freq" -le 5885 ]; then
            if [ "$vht_he_opclass" -ne 0 ]; then
                op_class=$vht_he_opclass
            else
                case "$sec_channel" in
                    1) op_class=126 ;;
                    -1) op_class=127 ;;
                    *) op_class=125 ;;
                esac
            fi
        elif [ "$freq" -ge 5500 ] && [ "$freq" -le 5720 ]; then
            if [ "$vht_he_opclass" -ne 0 ] || [ "$width" -eq 320 ]; then
                op_class=$vht_he_opclass
            else
                case "$sec_channel" in
                    1) op_class=122 ;;
                    -1) op_class=123 ;;
                    *) op_class=121 ;;
                esac
            fi
        fi

    fi

    # 6 GHz frequency range
    if [ "$freq" -ge 5950 ] && [ "$freq" -le 7115 ]; then
        case "$width" in
            80) op_class=133 ;;  # 80 MHz
            160) op_class=134 ;;  # 160 MHz
            "80+80") op_class=135 ;;  # 80+80 MHz
            320) op_class=137 ;;  # 320 MHz
            *)
		    if [ "$sec_channel" -ne 0 ]; then
			    op_class=132
		    else
			    op_class=131
		    fi
		    ;;
        esac
    fi

    if [ "$freq" -eq 5950 ];then
	    op_class=136
    fi

	hostapd_cli -i $ap_intf $ml_link set op_class $op_class> /dev/null
}

# STA association is completed, hence adjusting hostapd running config
hostapd_adjust_config() {
        sta_freq=$1
        sta_channel=$(iw $phy channels | grep $sta_freq |  awk '{print $4}' |  sed -e "s/\[//g" | sed -e "s/\]//g")
        sta_width=$(iw dev $sta_intf info | grep $sta_freq | awk '{print $6}')
        wifi_gen=$(wpa_cli -i $sta_intf status 2> /dev/null | grep wifi_generation | cut -d'=' -f 2)
        ieee80211ac=$(wpa_cli -i $sta_intf status 2> /dev/null | grep ieee80211ac | cut -d'=' -f 2)
        ap_intf=$2
        punct_bitmap=$3
        wifi_6gband=$(hostapd_is_6ghz_band $sta_freq)
        wifi_5gband=$(hostapd_is_5ghz_band $sta_freq)

	if [ -z $ieee80211ac ]; then
		ieee80211ac=0
	fi


	hostapd_cli -i $ap_intf $ml_link set channel $sta_channel 2> /dev/null
	if [ "$wifi_5gband" == "true" ] || [ "$wifi_6gband" == "true" ]; then
		hostapd_cli -i $ap_intf $ml_link set hw_mode a 2> /dev/null
	else
		hostapd_cli -i $ap_intf $ml_link set hw_mode g 2> /dev/null
	fi

	get_ap_ht_capab $sta_freq

	ap_ht_mode=$(echo $ap_ht_capab | sed -n 's/.*\(\[HT40*+*-*]\).*/\1/p')
	#echo "Current AP HT capab $ap_ht_capab" > /dev/ttyMSM0

	if [ "$wifi_6gband" == "true" ]; then
		hostapd_cli -i $ap_intf $ml_link SET discard_6g_awgn_event 1
	fi

	if [ "$wifi_5gband" == "true" ]; then
		hostapd_cli -i $ap_intf $ml_link SET disable_csa_dfs 1
	fi

	if [ "$(wpa_cli -i $sta_intf signal_poll 2> /dev/null | grep WIDTH | cut -d'(' -f 2 | cut -d')' -f 1)" = "no HT" ]; then
		#echo "STA associated in No HT mode, downgrading AP as well" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link set ieee80211ac 0 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set ieee80211n 0 2> /dev/null
		# Set HT capab without HT40+/- config to set secondary channel 0
		hostapd_ht20_mode
		hostapd_cli -i $ap_intf $ml_link set ieee80211ax 0 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set ieee80211be 0 2> /dev/null

	 elif [ $wifi_gen == 6 ] || [ $wifi_gen == 7 ]; then
		#echo "STA associated in HE$sta_width mode, applying same config to AP" > /dev/ttyMSM0
                local ap_vht_he_eht_oper_chwidth
                local ap_vht_he_eht_oper_centr_freq_seg0_idx

                hostapd_vht_he_eht_oper_chwidth "$sta_width"
                hostapd_vht_he_eht_oper_centr_freq_seg0_idx "$sta_width" "$sta_channel" "$sta_freq" "$sta_intf"

		if [ $wifi_gen == 7 ]; then
			hostapd_cli -i $ap_intf $ml_link set ieee80211be 1 2> /dev/null
			hostapd_cli -i $ap_intf $ml_link set eht_oper_chwidth $ap_vht_he_eht_oper_chwidth
			hostapd_cli -i $ap_intf $ml_link set eht_oper_centr_freq_seg0_idx $ap_vht_he_eht_oper_centr_freq_seg0_idx
			hostapd_cli -i $ap_intf $ml_link set punct_bitmap $punct_bitmap
		fi
		hostapd_cli -i $ap_intf $ml_link set ieee80211ax 1 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set he_oper_chwidth $ap_vht_he_eht_oper_chwidth
		hostapd_cli -i $ap_intf $ml_link set he_oper_centr_freq_seg0_idx $ap_vht_he_eht_oper_centr_freq_seg0_idx

		# VHT Operations is not applicable for 6GHz interface
		if [ $wifi_6gband == false ]; then
			#echo "Interface in Wifi 6 gen, but its not a 6GHz interface" > /dev/ttyMSM0
			#echo "Set 802.11n, ac to 1" > /dev/ttyMSM0
			hostapd_cli -i $ap_intf $ml_link set ieee80211ac 1 2> /dev/nul
			hostapd_cli -i $ap_intf $ml_link set ieee80211n 1 2> /dev/null
			hostapd_cli -i $ap_intf $ml_link set vht_oper_chwidth $ap_vht_he_eht_oper_chwidth

			#echo "New vht_oper_chwidth is $ap_vht_he_eht_oper_chwidth" > /dev/ttyMSM0
			#echo "vht_oper_centr_freq_seg0_idx is $ap_vht_he_eht_oper_centr_freq_seg0_idx" > /dev/ttyMSM0
			hostapd_cli -i $ap_intf set vht_oper_centr_freq_seg0_idx $ap_vht_he_eht_oper_centr_freq_seg0_idx

			if [ $sta_width = "20" ]; then
				#echo "Setting HE20 mode to AP" > /dev/ttyMSM0
				hostapd_ht20_mode
			else
				hostapd_ht40_mode "$sta_channel"
			fi
		fi
	elif [ $wifi_gen == 5 -o $ieee80211ac == 1 ]; then
		#echo "STA associated in VHT$sta_width mode, applying same config to AP" > /dev/ttyMSM0
		local ap_vht_he_eht_oper_chwidth
		local ap_vht_he_eht_oper_centr_freq_seg0_idx

		hostapd_vht_he_eht_oper_chwidth "$sta_width"

		#echo "Set 802.11n & ac to 1" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link set ieee80211ac 1 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set ieee80211n 1 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set ieee80211ax 0 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set ieee80211be 0 2> /dev/null

		#echo "vht_oper_chwidth is $ap_vht_he_eht_oper_chwidth for VHT$sta_width mode" > /dev/ttyMSM0

		hostapd_vht_he_eht_oper_centr_freq_seg0_idx "$sta_width" "$sta_channel" "$sta_freq"

		#echo "New vht_oper_chwidth is $ap_vht_he_eht_oper_chwidth" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link set vht_oper_chwidth $ap_vht_he_eht_oper_chwidth

		#echo "vht_oper_centr_freq_seg0_idx is $ap_vht_he_eht_oper_centr_freq_seg0_idx" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link set vht_oper_centr_freq_seg0_idx $ap_vht_he_eht_oper_centr_freq_seg0_idx

		if [ $sta_width = "20" ]; then
                        #echo "Setting VHT20 mode to AP" > /dev/ttyMSM0
                        hostapd_ht20_mode
                else
                        hostapd_ht40_mode "$sta_channel"
                fi
	else
		#echo "STA associated in HT$sta_width mode, applying same config to AP" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link set ieee80211n 1
		hostapd_cli -i $ap_intf $ml_link set ieee80211ac 0
		hostapd_cli -i $ap_intf $ml_link set ieee80211ax 0
		hostapd_cli -i $ap_intf $ml_link set ieee80211be 0

		if [ $sta_width = "20" ]; then
                        #echo "Setting HT20 mode to AP" > /dev/ttyMSM0
                        hostapd_ht20_mode
                else
                        hostapd_ht40_mode "$sta_channel"
                fi
	fi
	hostapd_set_op_class $sta_freq $sta_channel $sta_width $sta_intf
	echo "STA associated in Channel $sta_channel, Width $sta_width MHz, Wifi Gen $wifi_gen, AP $ap_intf link $ml_link op_class $op_class" > /dev/ttyMSM0
}

get_link_ids() {
	ifname=$1
	link=$(iw dev $ifname info | grep link | cut -d':' -f 1 2> /dev/null  | cut -d ' ' -f 2)

	if [ -n "$link" ]; then
		echo "$link"
		return
	else
		#when interface is in disable state, we might not find link from iw, hence, use control
		#interface file names to find the link id

		ctrl_iface=$(ls /var/run/hostapd/${ifname}*)
		if [ -n "$ctrl_iface" ]; then
			def_ctrl_iface_path=$(ls /var/run/hostapd/${ifname}_* | head -n 1)
			#Try to return links only if has link control interface
			if [[ "$def_ctrl_iface_path" == *"link"* ]]; then
				links=$(ls /var/run/hostapd/${ifname}_* | awk '{print substr($0,length,1)}')
				if [ -n "$links" ]; then
					echo "$links"
					return
				fi
			fi
		fi
	fi
	echo ""
}

hostapd_is_6ghz_band() {
	local freq=$1
	if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
		echo true
	else
		echo false
	fi
}

hostapd_is_5ghz_band() {
	local freq=$1
	if [ $freq -gt 5170 ] && [ $freq -le 5925 ]; then
		echo true
	else
		echo false
	fi
}

is_apfreq_in_sta_freq_list() {
	ap_freq=$1
	found=0
	sta_freqlist="$2"
	for f in $sta_freqlist
	do
		if [[ "$ap_freq" == "$f" ]]; then
			found=1
		fi
	done
	echo $found
}

get_link_info() {
	ifname=$1
	freq=$2
	ap_freq=
	ml_link=""
	link_ids=$(get_link_ids $ifname)

	if [ -z "$link_ids" ]; then
		echo ""
		return
	fi

	for i in $link_ids
	do
		ap_freq=$(hostapd_cli -i $ifname -l $i status | grep -w freq | cut -d'=' -f 2)
		sta_freq_list="$(get_sta_freq_list $phy $freq)"
		is_freq_present=$(is_apfreq_in_sta_freq_list $ap_freq "$sta_freq_list")
		if [ $is_freq_present -eq 1 ]; then
			ml_link="-l $i"
			echo "$ml_link"
			return
		fi
	done
	echo "$ml_link"
}

hostapd_get_ap_status() {
	local ap_intf=$1
	link_ids=$(get_link_ids $ap_intf)

	if [ -n "$link_ids" ]; then
		for i in $link_ids
		do
			res=$(hostapd_cli -i $ap_intf -l $i status 2> /dev/null | grep state | cut -d'=' -f 2)
			if [ "$res" != "ENABLED" ]; then
				echo $res
				return
			fi
		done
		ap_status=$res
	else
		ap_status=$(hostapd_cli -i $ap_intf status 2> /dev/null | grep state | cut -d'=' -f 2)
	fi

	echo $ap_status
}

cat > /lib/repeater_6g_ch_sw.sh << EOF
#!/bin/sh

if [ "`echo $\2`" = "CTRL-EVENT-STARTED-CHANNEL-SWITCH" ] && [ "`echo $\1`" = $sta_intf ]; then
        wpa_cli -i $sta_intf disable 0
        sleep 2
        wpa_cli -i $sta_intf enable 0
fi
EOF
chmod 777 /lib/repeater_6g_ch_sw.sh
wpa_cli -i $sta_intf -a /lib/repeater_6g_ch_sw.sh &

cat > /lib/radar_detect.sh << EOF
#!/bin/sh

if [ "`echo $\2`" = "DFS-RADAR-DETECTED" ] && [ "`echo $\1`" = $sta_intf ]; then
        wpa_cli -i $sta_intf disable 0
fi
EOF
chmod 777 /lib/radar_detect.sh
wpa_cli -i $sta_intf -a /lib/radar_detect.sh &

echo "Checking wpa_state $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2)" > /dev/ttyMSM0

while true;
do
	for ap_intf in $ap_intfs
	do
		ctrl_intf=$(ls /var/run/hostapd/$ap_intf*)
		if [[ ! -e "/var/run/wpa_supplicant/$sta_intf" && -z "$ctrl_intf" ]]; then
			echo "AP+STA mode not running $ap_intf, exiting script" >> /tmp/apsta_debug.log
			failure=1
		else
			failure=0
		fi
	done

	if [ $failure -eq 1 ]; then
		exit
	fi

	wpa_status=$(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2)
	if [ "$wpa_status" = "DISCONNECTED"  -o  "$wpa_status" = "SCANNING" ]; then

		for ap_intf in $ap_intfs
		do
			#currently, hostapd_cli disable would disable all MLO vaps
			ap_status=$(hostapd_get_ap_status $ap_intf)
			if [ "$ap_status" == "FAIL" ]; then
				exit
			fi

			if [ "$ap_status" = "ENABLED" ]; then
				echo "wpa_s state: $wpa_status, stopping AP $ap_intf" > /dev/ttyMSM0

				ap_links=$(iw dev $ap_intf info | grep link | cut -d':' -f 1 2> /dev/null  | cut -d ' ' -f 2)
				if [ -n "$ap_links" ]; then
					for i in $ap_links
					do
						hostapd_cli -i $ap_intf -l $i disable
					done
				else
					hostapd_cli -i $ap_intf disable
				fi
			fi
		done
	fi

	ap_status=$(hostapd_get_ap_status $ap_intf)
	if [ $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2) = "COMPLETED" ] &&
	   [ "$ap_status" = "DISABLED" ]; then
		wpa_cli -i $sta_intf signal_poll
		wifi_gen=$(wpa_cli -i $sta_intf status 2> /dev/null | grep wifi_generation | cut -d'=' -f 2)
		if [ $wifi_gen -eq 7 ]; then
			sta_link_freqs=$(wpa_cli -i $sta_intf mlo_status | grep freq | cut -d'=' -f 2)
			sta_link_punct_bitmaps=$(wpa_cli -i $sta_intf mlo_signal_poll | grep PUNCT_BITMAP | cut -d'=' -f 2)

			#Backward compatability where STA MLO support is not there in EHT
			if [ -z "$sta_link_freqs"]; then
				sta_link_freqs=$(wpa_cli -i $sta_intf status 2> /dev/null | grep freq | cut -d'=' -f 2)
				sta_link_punct_bitmaps=$(wpa_cli -i $sta_intf signal_poll | grep PUNCT_BITMAP | cut -d'=' -f 2)
			fi

			index=1
			for freq in $sta_link_freqs
			do
				link_punct_bitmap=0
				if [ -n "$sta_link_punct_bitmaps" ]; then
					link_punct_bitmap=$(echo "$sta_link_punct_bitmaps" |  cut -d ' ' -f 2 | awk -v n="$index" 'NR == n {print}')
					if [ -z "$link_punct_bitmap"]; then
						link_punct_bitmap=0
					fi
				fi

				for ap_intf in $ap_intfs
				do
					ml_link=$(get_link_info $ap_intf $freq)
					echo link config command is $ml_link $freq $ap_intf  > /dev/console
					if [ -n "$ml_link" ]; then
						hostapd_adjust_config $freq $ap_intf $link_punct_bitmap
					fi
				done

				let index++
			done
		else
			sta_chan=$(iw $sta_intf info 2> /dev/null | grep channel | cut -d' ' -f 2)
			local sta_freq=$(wpa_cli -i $sta_intf status 2> /dev/null | grep freq | cut -d'=' -f 2)

			# workaround for upstream station mld failed to get sta freq list
			if [ $sta_freq -eq 0 ] && [ $wifi_gen -eq 6 ]; then
				sta_freq=$(wpa_cli -i $sta_intf mlo_status 2> /dev/null | grep freq | cut -d'=' -f 2)
				echo "sta freq $sta_freq" >> /tmp/apsta_debug.log
			fi

			wifi_6gband=$(hostapd_is_6ghz_band $sta_freq)

			link_punct_bitmap=0
			for ap_intf in $ap_intfs
			do
				hostapd_adjust_config $sta_freq $ap_intf $link_punct_bitmap
			done
		fi

		# workaround for sending 4addr packet to AP from repeater after association
		ip_addr="$(ifconfig | grep -A 1 'br-lan' | tail -1 | cut -d ':' -f 2 | cut -d ' ' -f 1)"
		arping "$ip_addr" -U -I br-lan -D -c 5
		#echo "Enabling below hostapd config:" > /dev/ttyMSM0

		ap_status=$(hostapd_get_ap_status $ap_intf)

		if [ $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2) = "COMPLETED" ] &&
		   [ "$ap_status" = "DISABLED" ]; then
			for ap_intf in $ap_intfs
			do
				ap_links=$(get_link_ids $ap_intf)
				if [ -n "$ap_links" ]; then
					for i in $ap_links
					do
						hostapd_cli -i $ap_intf -l $i enable
					done
				else
					hostapd_cli -i $ap_intf enable
				fi

				ap_status=$(hostapd_get_ap_status $ap_intf)
				# workaround for single instance hostapd not doing "enable" without "disable" call to deinit hapd driver
				if [ $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2) = "COMPLETED" ] &&
				   [ "$ap_status" = "DISABLED" ]; then
					if [ -n "$ap_links" ]; then
						for i in $ap_links
						do
							hostapd_cli -i $ap_intf -l $i disable
							sleep 1
							hostapd_cli -i $ap_intf -l $i enable
							sleep 4
						done
					else
						hostapd_cli -i $ap_intf disable
						sleep 1
						hostapd_cli -i $ap_intf enable
						sleep 4
					fi
				fi

				ap_status=$(hostapd_get_ap_status $ap_intf)
				if [ "$ap_status" = "DISABLED" -o "$ap_status" = "FAIL" ] &&
				   [ $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2) = "COMPLETED" ]; then
					echo "REPEATER AP $ap_intf failed bring-up, status $ap_status recovering" > /dev/ttyMSM0

					# Below log dump should be enable for debugging purposes.

					#logread > /tmp/logread_AP_failure.log
					#echo "Collect if any core present in /tmp/ and output of /tmp/logread_AP_failure.log" > /dev/console
					#echo "Hostapd enable failed, exiting" >> /tmp/apsta_debug.log
					#date >> /tmp/apsta_debug.log
					#iw dev >> /tmp/apsta_debug.log
					#iw dev $ap_intf info >> /tmp/apsta_debug.log
					#ap_link=$(get_link_ids $ap_intf)
					#if [ -n "$ap_link" ]; then
					#	for i in $ap_link
					#	do
					#		hostapd_cli -i $ap_intf -l $i status>> /tmp/apsta_debug.log
					#	done
					#else
					#	hostapd_cli -i $ap_intf status>> /tmp/apsta_debug.log
					#fi
					#wpa_cli -i $sta_intf signal_poll >> /tmp/apsta_debug.log
					#wpa_cli -i $sta_intf status >> /tmp/apsta_debug.log
					#wpa_cli -i $sta_intf mlo_status >> /tmp/apsta_debug.log
					#wpa_cli -i $sta_intf list_n >> /tmp/apsta_debug.log
					#wpa_cli -i $sta_intf all_bss >> /tmp/apsta_debug.log
					#date >> /tmp/apsta_debug.log

					wifi down
					sleep 4
					wifi up
				fi
			done
		fi
	fi
	usleep 100000
done &
