# netifd wireless implements _wdev_prepare_channel
# which parses the band and hwmode config options before a drv_* function is called
# for now, we override these with desired vals to avoid modifying netifd-wireless
morse_band_override(){
	band=s1g
	hwmode=a
}

# Unlike the other morse functions, this doesn't override but instead
# just adds some extra config options before the network definition.
morse_wpa_supplicant_prepare_interface() {
	local _ifname=$1
	_wpa_supplicant_common $_ifname

	json_get_vars sae_pwe multi_ap
	json_get_values sae_group_list sae_group

	[ "${multi_ap:=0}" -lt 1 ] && set_default sae_pwe 1

	local pmf=
	[ "${multi_ap:=0}" -eq 1 ] && pmf=2

	cat >> "$_config" <<EOF
ctrl_interface=/var/run/wpa_supplicant_s1g
${sae_pwe:+sae_pwe=$sae_pwe}
${sae_group_list:+sae_groups=$sae_group_list}
${owe_group:+owe_group=$owe_group}
${pmf:+pmf=$pmf}
${vendor_keep_alive_offload:+vendor_keep_alive_offload=$vendor_keep_alive_offload}
EOF
	return 0
}

morse_override_wpa3_encryption() {
	json_get_vars encryption
	[ $encryption == "wpa3" ] &&  {
		wpa_pairwise=CCMP
		wpa_cipher=CCMP
	}
}

morse_override_hostapd_set_bss_options() {
	local var="$1"
	local phy="$2"
	local vif="$3"

	wireless_vif_parse_encryption
	morse_override_wpa3_encryption

	local bss_conf bss_md5sum ft_key
	local wep_rekey wpa_group_rekey wpa_pair_rekey wpa_master_rekey wpa_key_mgmt

	json_get_vars \
		wep_rekey wpa_group_rekey wpa_pair_rekey wpa_master_rekey wpa_strict_rekey \
		wpa_disable_eapol_key_retries tdls_prohibit \
		maxassoc max_inactivity disassoc_low_ack isolate auth_cache \
		wps_pushbutton wps_label ext_registrar wps_pbc_in_m1 wps_ap_setup_locked \
		wps_independent wps_device_type wps_device_name wps_manufacturer wps_pin \
		macfilter ssid utf8_ssid wmm uapsd hidden short_preamble rsn_preauth \
		iapp_interface eapol_version dynamic_vlan ieee80211w nasid \
		acct_secret acct_port acct_interval \
		bss_load_update_period chan_util_avg_period sae_require_mfp \
		multi_ap multi_ap_backhaul_ssid multi_ap_backhaul_key skip_inactivity_poll \
		ppsk airtime_bss_weight airtime_bss_limit airtime_sta_weight \
		multicast_to_unicast_all per_sta_vif \
		eap_server eap_user_file ca_cert server_cert private_key private_key_passwd server_id \
		vendor_elements fils ocv \
		wps_virtual_push_button \
		beacon_int

	json_get_values sae_group_list sae_group
	json_get_values owe_group_list owe_group

	set_default fils 0
	set_default isolate 0
	set_default maxassoc 0
	set_default max_inactivity 0
	set_default short_preamble 1
	set_default disassoc_low_ack 1
	set_default skip_inactivity_poll 0
	set_default hidden 0
	set_default wmm 1
	set_default uapsd 1
	set_default wpa_disable_eapol_key_retries 0
	set_default tdls_prohibit 0
	set_default eapol_version $((wpa & 1))
	set_default acct_port 1813
	set_default bss_load_update_period 60
	set_default chan_util_avg_period 600
	set_default utf8_ssid 1
	set_default multi_ap 0
	set_default ppsk 0
	set_default airtime_bss_weight 0
	set_default airtime_bss_limit 0
	set_default eap_server 0
	set_default wpa_group_rekey 604800
	set_default beacon_int 100

	/sbin/hostapd_s1g -vfils || fils=0

	append bss_conf "ctrl_interface=/var/run/hostapd_s1g"
	if [ "$isolate" -gt 0 ]; then
		append bss_conf "ap_isolate=$isolate" "$N"
	fi
	if [ "$maxassoc" -gt 0 ]; then
		append bss_conf "max_num_sta=$maxassoc" "$N"
	fi
	if [ "$max_inactivity" -gt 0 ]; then
		append bss_conf "ap_max_inactivity=$max_inactivity" "$N"
	fi

	[ "$airtime_bss_weight" -gt 0 ] && append bss_conf "airtime_bss_weight=$airtime_bss_weight" "$N"
	[ "$airtime_bss_limit" -gt 0 ] && append bss_conf "airtime_bss_limit=$airtime_bss_limit" "$N"
	json_for_each_item append_airtime_sta_weight airtime_sta_weight

	append bss_conf "bss_load_update_period=$bss_load_update_period" "$N"
	append bss_conf "chan_util_avg_period=$chan_util_avg_period" "$N"
	append bss_conf "disassoc_low_ack=$disassoc_low_ack" "$N"
	append bss_conf "skip_inactivity_poll=$skip_inactivity_poll" "$N"
	append bss_conf "preamble=$short_preamble" "$N"
	append bss_conf "wmm_enabled=$wmm" "$N"
	append bss_conf "ignore_broadcast_ssid=$hidden" "$N"
	append bss_conf "uapsd_advertisement_enabled=$uapsd" "$N"
	append bss_conf "utf8_ssid=$utf8_ssid" "$N"
	append bss_conf "multi_ap=$multi_ap" "$N"
	append bss_conf "beacon_int=$beacon_int" "$N"
	[ -n "$vendor_elements" ] && append bss_conf "vendor_elements=$vendor_elements" "$N"

	[ "$tdls_prohibit" -gt 0 ] && append bss_conf "tdls_prohibit=$tdls_prohibit" "$N"

	[ "$wpa" -gt 0 ] && {
		[ -n "$wpa_group_rekey"  ] && append bss_conf "wpa_group_rekey=$wpa_group_rekey" "$N"
		[ -n "$wpa_pair_rekey"   ] && append bss_conf "wpa_ptk_rekey=$wpa_pair_rekey"    "$N"
		[ -n "$wpa_master_rekey" ] && append bss_conf "wpa_gmk_rekey=$wpa_master_rekey"  "$N"
		[ -n "$wpa_strict_rekey" ] && append bss_conf "wpa_strict_rekey=$wpa_strict_rekey" "$N"
	}

	[ -n "$nasid" ] && append bss_conf "nas_identifier=$nasid" "$N"

	[ -n "$acct_interval" ] && \
		append bss_conf "radius_acct_interim_interval=$acct_interval" "$N"
	json_for_each_item append_acct_server acct_server
	json_for_each_item append_radius_acct_req_attr radius_acct_req_attr

	case "$auth_type" in
		sae|owe|eap2|eap192)
			set_default ieee80211w 2
			set_default sae_require_mfp 1
		;;
		psk-sae|eap-eap2)
			set_default ieee80211w 1
			set_default sae_require_mfp 1
		;;
	esac
	[ -n "$sae_require_mfp" ] && append bss_conf "sae_require_mfp=$sae_require_mfp" "$N"

	local vlan_possible=""

	case "$auth_type" in
		none|owe)
			json_get_vars owe_transition_bssid owe_transition_ssid owe_transition_ifname

			[ -n "$owe_transition_ssid" ] && append bss_conf "owe_transition_ssid=\"$owe_transition_ssid\"" "$N"
			[ -n "$owe_transition_bssid" ] && append bss_conf "owe_transition_bssid=$owe_transition_bssid" "$N"
			[ -n "$owe_transition_ifname" ] && append bss_conf "owe_transition_ifname=$owe_transition_ifname" "$N"

			[ -n "$owe_group_list" ] && append bss_conf "owe_groups=$owe_group_list" "$N"

			wps_possible=1
			# Here we make the assumption that if we're in open mode
			# with WPS enabled, we got to be in unconfigured state.
			wps_not_configured=1
		;;
		psk|sae|psk-sae)
			json_get_vars key wpa_psk_file
			if [ "$auth_type" = "psk" ] && [ "$ppsk" -ne 0 ] ; then
				json_get_vars auth_secret auth_port
				set_default auth_port 1812
				json_for_each_item append_auth_server auth_server
				append bss_conf "macaddr_acl=2" "$N"
				append bss_conf "wpa_psk_radius=2" "$N"
			elif [ ${#key} -eq 64 ]; then
				append bss_conf "wpa_psk=$key" "$N"
			elif [ ${#key} -ge 8 ] && [ ${#key} -le 63 ]; then
				append bss_conf "wpa_passphrase=$key" "$N"
				# Workaround for SW-12231 (dpp hostapd_s1g bug)
				append bss_conf "sae_password=${key}" "$N"
			elif [ -n "$key" ] || [ -z "$wpa_psk_file" ]; then
				wireless_setup_vif_failed INVALID_WPA_PSK
				return 1
			fi
			[ -z "$wpa_psk_file" ] && set_default wpa_psk_file /var/run/hostapd-$ifname.psk
			[ -n "$wpa_psk_file" ] && {
				[ -e "$wpa_psk_file" ] || touch "$wpa_psk_file"
				append bss_conf "wpa_psk_file=$wpa_psk_file" "$N"
			}
			[ "$eapol_version" -ge "1" -a "$eapol_version" -le "2" ] && append bss_conf "eapol_version=$eapol_version" "$N"

			[ -n "$sae_group_list" ] && append bss_conf "sae_groups=$sae_group_list" "$N"

			set_default dynamic_vlan 0
			vlan_possible=1
			wps_possible=1
		;;
		eap|eap2|eap-eap2|eap192)
			json_get_vars \
				auth_server auth_secret auth_port \
				dae_client dae_secret dae_port \
				dynamic_ownip ownip radius_client_addr \
				eap_reauth_period request_cui \
				erp_domain mobility_domain \
				fils_realm fils_dhcp

			# radius can provide VLAN ID for clients
			vlan_possible=1

			# This is the normal OpenWrt behaviour, but currently hostapd_s1g doesn't
			# understand dynamic_ownip so we avoid setting it by default.
			# set_default dynamic_ownip 1

			# legacy compatibility
			[ -n "$auth_server" ] || json_get_var auth_server server
			[ -n "$auth_port" ] || json_get_var auth_port port
			[ -n "$auth_secret" ] || json_get_var auth_secret key

			[ "$fils" -gt 0 ] && {
				set_default erp_domain "$mobility_domain"
				set_default erp_domain "$(echo "$ssid" | md5sum | head -c 8)"
				set_default fils_realm "$erp_domain"

				append bss_conf "erp_send_reauth_start=1" "$N"
				append bss_conf "erp_domain=$erp_domain" "$N"
				append bss_conf "fils_realm=$fils_realm" "$N"
				append bss_conf "fils_cache_id=$(echo "$fils_realm" | md5sum | head -c 4)" "$N"

				[ "$fils_dhcp" = "*" ] && {
					json_get_values network network
					fils_dhcp=
					for net in $network; do
						fils_dhcp="$(ifstatus "$net" | jsonfilter -e '@.data.dhcpserver')"
						[ -n "$fils_dhcp" ] && break
					done

					[ -z "$fils_dhcp" -a -n "$network_bridge" -a -n "$network_ifname" ] && \
						fils_dhcp="$(udhcpc -B -n -q -s /lib/netifd/dhcp-get-server.sh -t 1 -i "$network_ifname" 2>/dev/null)"
				}
				[ -n "$fils_dhcp" ] && append bss_conf "dhcp_server=$fils_dhcp" "$N"
			}

			set_default auth_port 1812
			set_default dae_port 3799
			set_default request_cui 0

			[ "$eap_server" -eq 0 ] && json_for_each_item append_auth_server auth_server

			[ "$request_cui" -gt 0 ] && append bss_conf "radius_request_cui=$request_cui" "$N"
			[ -n "$eap_reauth_period" ] && append bss_conf "eap_reauth_period=$eap_reauth_period" "$N"

			[ -n "$dae_client" -a -n "$dae_secret" ] && {
				append bss_conf "radius_das_port=$dae_port" "$N"
				append bss_conf "radius_das_client=$dae_client $dae_secret" "$N"
			}
			json_for_each_item append_radius_auth_req_attr radius_auth_req_attr

			if [ -n "$ownip" ]; then
				append bss_conf "own_ip_addr=$ownip" "$N"
			elif [ "$dynamic_ownip" -gt 0 ]; then
				append bss_conf "dynamic_own_ip_addr=$dynamic_ownip" "$N"
			fi

			[ -n "$radius_client_addr" ] && append bss_conf "radius_client_addr=$radius_client_addr" "$N"
			append bss_conf "eapol_key_index_workaround=1" "$N"
			append bss_conf "ieee8021x=1" "$N"

			[ "$eapol_version" -ge "1" -a "$eapol_version" -le "2" ] && append bss_conf "eapol_version=$eapol_version" "$N"
		;;
		wep)
			local wep_keyidx=0
			json_get_vars key
			hostapd_append_wep_key bss_conf
			append bss_conf "wep_default_key=$wep_keyidx" "$N"
			[ -n "$wep_rekey" ] && append bss_conf "wep_rekey_period=$wep_rekey" "$N"
		;;
	esac

	case "$auth_type" in
		none|owe|psk|sae|psk-sae|wep)
			json_get_vars \
			auth_server auth_port auth_secret \
			ownip radius_client_addr

			[ -n "$auth_server" ] &&  {
				set_default auth_port 1812

				json_for_each_item append_auth_server auth_server
				[ -n "$ownip" ] && append bss_conf "own_ip_addr=$ownip" "$N"
				[ -n "$radius_client_addr" ] && append bss_conf "radius_client_addr=$radius_client_addr" "$N"
				append bss_conf "macaddr_acl=2" "$N"
			}
		;;
	esac

	local auth_algs="$((($auth_mode_shared << 1) | $auth_mode_open))"
	append bss_conf "auth_algs=${auth_algs:-1}" "$N"
	append bss_conf "wpa=$wpa" "$N"
	[ -n "$wpa_pairwise" ] && append bss_conf "wpa_pairwise=$wpa_pairwise" "$N"

	set_default wps_pushbutton 0
	set_default wps_label 0
	set_default wps_pbc_in_m1 0
	set_default wps_virtual_push_button 0

	config_methods=
	[ "$wps_pushbutton" -gt 0 ] && append config_methods push_button
	[ "$wps_label" -gt 0 ] && append config_methods label
	[ "$wps_virtual_push_button" -gt 0 ] && append config_methods virtual_push_button

	# WPS not possible on Multi-AP backhaul-only SSID
	[ "$multi_ap" = 1 ] && wps_possible=

	[ -n "$wps_possible" -a -n "$config_methods" ] && {
		set_default ext_registrar 0
		set_default wps_device_type "6-0050F204-1"
		set_default wps_device_name "OpenWrt AP"
		set_default wps_manufacturer "www.openwrt.org"
		set_default wps_independent 1

		wps_state=2
		[ -n "$wps_not_configured" ] && wps_state=1

		[ "$ext_registrar" -gt 0 -a -n "$network_bridge" ] && append bss_conf "upnp_iface=$network_bridge" "$N"

		append bss_conf "eap_server=1" "$N"
		[ -n "$wps_pin" ] && append bss_conf "ap_pin=$wps_pin" "$N"
		append bss_conf "wps_state=$wps_state" "$N"
		append bss_conf "device_type=$wps_device_type" "$N"
		append bss_conf "device_name=$wps_device_name" "$N"
		append bss_conf "manufacturer=$wps_manufacturer" "$N"
		append bss_conf "config_methods=$config_methods" "$N"
		append bss_conf "wps_independent=$wps_independent" "$N"
		[ -n "$wps_ap_setup_locked" ] && append bss_conf "ap_setup_locked=$wps_ap_setup_locked" "$N"
		[ "$wps_pbc_in_m1" -gt 0 ] && append bss_conf "pbc_in_m1=$wps_pbc_in_m1" "$N"
		[ "$multi_ap" -gt 0 ] && [ -n "$multi_ap_backhaul_ssid" ] && {
			append bss_conf "multi_ap_backhaul_ssid=\"$multi_ap_backhaul_ssid\"" "$N"
			if [ -z "$multi_ap_backhaul_key" ]; then
				:
			elif [ ${#multi_ap_backhaul_key} -lt 8 ]; then
				wireless_setup_vif_failed INVALID_WPA_PSK
				return 1
			elif [ ${#multi_ap_backhaul_key} -eq 64 ]; then
				append bss_conf "multi_ap_backhaul_wpa_psk=$multi_ap_backhaul_key" "$N"
			else
				append bss_conf "multi_ap_backhaul_wpa_passphrase=$multi_ap_backhaul_key" "$N"
			fi
		}
	}

	append bss_conf "ssid=$ssid" "$N"
	# AJ 26/05/2023: I've commented out the bridge= setting as it is causing our standard 
	# bridged interface configuration to not function correctly. Further investigation will be required here
	# as this setting is not any different to what the system does anyway if it's not set, and the documentation
	# indicates it is optional...
	#[ -n "$network_bridge" ] && append bss_conf "bridge=$network_bridge${N}wds_bridge=" "$N"
	[ -n "$iapp_interface" ] && {
		local ifname
		network_get_device ifname "$iapp_interface" || ifname="$iapp_interface"
		append bss_conf "iapp_interface=$ifname" "$N"
	}

	json_get_vars time_advertisement time_zone wnm_sleep_mode wnm_sleep_mode_no_keys bss_transition
	set_default bss_transition 0
	set_default wnm_sleep_mode 1
	set_default wnm_sleep_mode_no_keys 0

	[ -n "$time_advertisement" ] && append bss_conf "time_advertisement=$time_advertisement" "$N"
	[ -n "$time_zone" ] && append bss_conf "time_zone=$time_zone" "$N"
	if [ "$wnm_sleep_mode" -eq "1" ]; then
		append bss_conf "wnm_sleep_mode=1" "$N"
		[ "$wnm_sleep_mode_no_keys" -eq "1" ] && append bss_conf "wnm_sleep_mode_no_keys=1" "$N"
	fi
	[ "$bss_transition" -eq "1" ] && append bss_conf "bss_transition=1" "$N"

	json_get_vars ieee80211k rrm_neighbor_report rrm_beacon_report rnr
	set_default ieee80211k 0
	set_default rnr 0
	if [ "$ieee80211k" -eq "1" ]; then
		set_default rrm_neighbor_report 1
		set_default rrm_beacon_report 1
	else
		set_default rrm_neighbor_report 0
		set_default rrm_beacon_report 0
	fi

	[ "$rrm_neighbor_report" -eq "1" ] && append bss_conf "rrm_neighbor_report=1" "$N"
	[ "$rrm_beacon_report" -eq "1" ] && append bss_conf "rrm_beacon_report=1" "$N"
	[ "$rnr" -eq "1" ] && append bss_conf "rnr=1" "$N"

	json_get_vars ftm_responder stationary_ap lci civic
	set_default ftm_responder 0
	if [ "$ftm_responder" -eq "1" ]; then
		set_default stationary_ap 0
		iw phy "$phy" info | grep -q "ENABLE_FTM_RESPONDER" && {
			append bss_conf "ftm_responder=1" "$N"
			[ "$stationary_ap" -eq "1" ] && append bss_conf "stationary_ap=1" "$N"
			[ -n "$lci" ] && append bss_conf "lci=$lci" "$N"
			[ -n "$civic" ] && append bss_conf "civic=$civic" "$N"
		}
	fi

	if [ "$wpa" -ge "1" ]; then
		json_get_vars ieee80211r
		set_default ieee80211r 0

		if [ "$ieee80211r" -gt "0" ]; then
			json_get_vars mobility_domain ft_psk_generate_local ft_over_ds reassociation_deadline

			set_default mobility_domain "$(echo "$ssid" | md5sum | head -c 4)"
			set_default ft_over_ds 0
			set_default reassociation_deadline 1000

			case "$auth_type" in
				psk|sae|psk-sae)
					set_default ft_psk_generate_local 1
				;;
				*)
					set_default ft_psk_generate_local 0
				;;
			esac

			[ -n "$network_ifname" ] && append bss_conf "ft_iface=$network_ifname" "$N"
			append bss_conf "mobility_domain=$mobility_domain" "$N"
			append bss_conf "ft_psk_generate_local=$ft_psk_generate_local" "$N"
			append bss_conf "ft_over_ds=$ft_over_ds" "$N"
			append bss_conf "reassociation_deadline=$reassociation_deadline" "$N"

			if [ "$ft_psk_generate_local" -eq "0" ]; then
				json_get_vars r0_key_lifetime r1_key_holder pmk_r1_push
				json_get_values r0kh r0kh
				json_get_values r1kh r1kh

				set_default r0_key_lifetime 10000
				set_default pmk_r1_push 0

				[ -n "$r0kh" -a -n "$r1kh" ] || {
					ft_key=`echo -n "$mobility_domain/${auth_secret:-${key}}" | md5sum | awk '{print $1}'`

					set_default r0kh "ff:ff:ff:ff:ff:ff,*,$ft_key"
					set_default r1kh "00:00:00:00:00:00,00:00:00:00:00:00,$ft_key"
				}

				[ -n "$r1_key_holder" ] && append bss_conf "r1_key_holder=$r1_key_holder" "$N"
				append bss_conf "r0_key_lifetime=$r0_key_lifetime" "$N"
				append bss_conf "pmk_r1_push=$pmk_r1_push" "$N"

				for kh in $r0kh; do
					append bss_conf "r0kh=${kh//,/ }" "$N"
				done
				for kh in $r1kh; do
					append bss_conf "r1kh=${kh//,/ }" "$N"
				done
			fi
		fi
		if [ "$fils" -gt 0 ]; then
			json_get_vars fils_realm
			set_default fils_realm "$(echo "$ssid" | md5sum | head -c 8)"
		fi

		append bss_conf "wpa_disable_eapol_key_retries=$wpa_disable_eapol_key_retries" "$N"

		hostapd_append_wpa_key_mgmt
		[ -n "$wpa_key_mgmt" ] && append bss_conf "wpa_key_mgmt=$wpa_key_mgmt" "$N"
	fi

	if [ "$wpa" -ge "2" ]; then
		if [ -n "$network_bridge" -a "$rsn_preauth" = 1 ]; then
			set_default auth_cache 1
			append bss_conf "rsn_preauth=1" "$N"
			append bss_conf "rsn_preauth_interfaces=$network_bridge" "$N"
		else
			case "$auth_type" in
			sae|psk-sae|owe)
				set_default auth_cache 1
			;;
			*)
				set_default auth_cache 0
			;;
			esac
		fi

		append bss_conf "okc=$auth_cache" "$N"
		[ "$auth_cache" = 0 -a "$fils" = 0 ] && append bss_conf "disable_pmksa_caching=1" "$N"

		# RSN -> allow management frame protection
		case "$ieee80211w" in
			[012])
				json_get_vars ieee80211w_mgmt_cipher ieee80211w_max_timeout ieee80211w_retry_timeout
				append bss_conf "ieee80211w=$ieee80211w" "$N"
				[ "$ieee80211w" -gt "0" ] && {
					if [ "$auth_type" = "eap192" ]; then
						append bss_conf "group_mgmt_cipher=BIP-GMAC-256" "$N"
					else
						append bss_conf "group_mgmt_cipher=${ieee80211w_mgmt_cipher:-AES-128-CMAC}" "$N"
					fi
					[ -n "$ieee80211w_max_timeout" ] && \
						append bss_conf "assoc_sa_query_max_timeout=$ieee80211w_max_timeout" "$N"
					[ -n "$ieee80211w_retry_timeout" ] && \
						append bss_conf "assoc_sa_query_retry_timeout=$ieee80211w_retry_timeout" "$N"
				}
			;;
		esac
	fi

	_macfile="/var/run/hostapd-$ifname.maclist"
	case "$macfilter" in
		allow)
			append bss_conf "macaddr_acl=1" "$N"
			append bss_conf "accept_mac_file=$_macfile" "$N"
			# accept_mac_file can be used to set MAC to VLAN ID mapping
			vlan_possible=1
		;;
		deny)
			append bss_conf "macaddr_acl=0" "$N"
			append bss_conf "deny_mac_file=$_macfile" "$N"
		;;
		*)
			_macfile=""
		;;
	esac

	[ -n "$_macfile" ] && {
		json_get_vars macfile
		json_get_values maclist maclist

		rm -f "$_macfile"
		(
			for mac in $maclist; do
				echo "$mac"
			done
			[ -n "$macfile" -a -f "$macfile" ] && cat "$macfile"
		) > "$_macfile"
	}

	json_get_vars iw_enabled iw_internet iw_asra iw_esr iw_uesa iw_access_network_type
	json_get_vars iw_hessid iw_venue_group iw_venue_type iw_network_auth_type
	json_get_vars iw_roaming_consortium iw_domain_name iw_anqp_3gpp_cell_net iw_nai_realm
	json_get_vars iw_anqp_elem iw_qos_map_set iw_ipaddr_type_availability iw_gas_address3
	json_get_vars iw_venue_name iw_venue_url

	set_default iw_enabled 0
	if [ "$iw_enabled" = "1" ]; then
		append bss_conf "interworking=1" "$N"
		set_default iw_internet 1
		set_default iw_asra 0
		set_default iw_esr 0
		set_default iw_uesa 0

		append bss_conf "internet=$iw_internet" "$N"
		append bss_conf "asra=$iw_asra" "$N"
		append bss_conf "esr=$iw_esr" "$N"
		append bss_conf "uesa=$iw_uesa" "$N"

		[ -n "$iw_access_network_type" ] && \
			append bss_conf "access_network_type=$iw_access_network_type" "$N"
		[ -n "$iw_hessid" ] && append bss_conf "hessid=$iw_hessid" "$N"
		[ -n "$iw_venue_group" ] && \
			append bss_conf "venue_group=$iw_venue_group" "$N"
		[ -n "$iw_venue_type" ] && append bss_conf "venue_type=$iw_venue_type" "$N"
		[ -n "$iw_network_auth_type" ] && \
			append bss_conf "network_auth_type=$iw_network_auth_type" "$N"
		[ -n "$iw_gas_address3" ] && append bss_conf "gas_address3=$iw_gas_address3" "$N"

		json_for_each_item append_iw_roaming_consortium iw_roaming_consortium
		json_for_each_item append_iw_anqp_elem iw_anqp_elem
		json_for_each_item append_iw_nai_realm iw_nai_realm
		json_for_each_item append_iw_venue_name iw_venue_name
		json_for_each_item append_iw_venue_url iw_venue_url

		iw_domain_name_conf=
		json_for_each_item append_iw_domain_name iw_domain_name
		[ -n "$iw_domain_name_conf" ] && \
			append bss_conf "domain_name=$iw_domain_name_conf" "$N"

		iw_anqp_3gpp_cell_net_conf=
		json_for_each_item append_iw_anqp_3gpp_cell_net iw_anqp_3gpp_cell_net
		[ -n "$iw_anqp_3gpp_cell_net_conf" ] && \
			append bss_conf "anqp_3gpp_cell_net=$iw_anqp_3gpp_cell_net_conf" "$N"
	fi

	set_default iw_qos_map_set 0,0,2,16,1,1,255,255,18,22,24,38,40,40,44,46,48,56
	case "$iw_qos_map_set" in
		*,*);;
		*) iw_qos_map_set="";;
	esac
	[ -n "$iw_qos_map_set" ] && append bss_conf "qos_map_set=$iw_qos_map_set" "$N"

	local hs20 disable_dgaf osen anqp_domain_id hs20_deauth_req_timeout \
		osu_ssid hs20_wan_metrics hs20_operating_class hs20_t_c_filename hs20_t_c_timestamp \
		hs20_t_c_server_url
	json_get_vars hs20 disable_dgaf osen anqp_domain_id hs20_deauth_req_timeout \
		osu_ssid hs20_wan_metrics hs20_operating_class hs20_t_c_filename hs20_t_c_timestamp \
		hs20_t_c_server_url

	set_default hs20 0
	set_default disable_dgaf $hs20
	set_default osen 0
	set_default anqp_domain_id 0
	set_default hs20_deauth_req_timeout 60
	if [ "$hs20" = "1" ]; then
		append bss_conf "hs20=1" "$N"
		append_hs20_icons
		append bss_conf "disable_dgaf=$disable_dgaf" "$N"
		append bss_conf "osen=$osen" "$N"
		append bss_conf "anqp_domain_id=$anqp_domain_id" "$N"
		append bss_conf "hs20_deauth_req_timeout=$hs20_deauth_req_timeout" "$N"
		[ -n "$osu_ssid" ] && append bss_conf "osu_ssid=$osu_ssid" "$N"
		[ -n "$hs20_wan_metrics" ] && append bss_conf "hs20_wan_metrics=$hs20_wan_metrics" "$N"
		[ -n "$hs20_operating_class" ] && append bss_conf "hs20_operating_class=$hs20_operating_class" "$N"
		[ -n "$hs20_t_c_filename" ] && append bss_conf "hs20_t_c_filename=$hs20_t_c_filename" "$N"
		[ -n "$hs20_t_c_timestamp" ] && append bss_conf "hs20_t_c_timestamp=$hs20_t_c_timestamp" "$N"
		[ -n "$hs20_t_c_server_url" ] && append bss_conf "hs20_t_c_server_url=$hs20_t_c_server_url" "$N"
		json_for_each_item append_hs20_oper_friendly_name hs20_oper_friendly_name
		json_for_each_item append_hs20_conn_capab hs20_conn_capab
		json_for_each_item append_osu_provider osu_provider
		json_for_each_item append_operator_icon operator_icon
	fi

	if [ "$eap_server" = "1" ]; then
		append bss_conf "eap_server=1" "$N"
		append bss_conf "eap_server_erp=1" "$N"
		[ -n "$eap_user_file" ] && append bss_conf "eap_user_file=$eap_user_file" "$N"
		[ -n "$ca_cert" ] && append bss_conf "ca_cert=$ca_cert" "$N"
		[ -n "$server_cert" ] && append bss_conf "server_cert=$server_cert" "$N"
		[ -n "$private_key" ] && append bss_conf "private_key=$private_key" "$N"
		[ -n "$private_key_passwd" ] && append bss_conf "private_key_passwd=$private_key_passwd" "$N"
		[ -n "$server_id" ] && append bss_conf "server_id=$server_id" "$N"
	fi

	set_default multicast_to_unicast_all 0
	if [ "$multicast_to_unicast_all" -gt 0 ]; then
		append bss_conf "multicast_to_unicast=$multicast_to_unicast_all" "$N"
	fi

	set_default per_sta_vif 0
	if [ "$per_sta_vif" -gt 0 ]; then
		append bss_conf "per_sta_vif=$per_sta_vif" "$N"
	fi

	json_get_values opts hostapd_bss_options
	for val in $opts; do
		append bss_conf "$val" "$N"
	done

	json_get_vars $TX_Q_CONFIGS
	set_default tx_queue_data3_aifs 7
	set_default tx_queue_data3_cwmin 15
	set_default tx_queue_data3_cwmax 1023
	set_default tx_queue_data3_burst 15.0
	
	set_default tx_queue_data2_aifs 3
	set_default tx_queue_data2_cwmin 15
	set_default tx_queue_data2_cwmax 1023
	set_default tx_queue_data2_burst 15.0

	set_default tx_queue_data1_aifs 2
	set_default tx_queue_data1_cwmin 7
	set_default tx_queue_data1_cwmax 15
	set_default tx_queue_data1_burst 15.0

	set_default tx_queue_data0_aifs 2
	set_default tx_queue_data0_cwmin 3
	set_default tx_queue_data0_cwmax 7
	set_default tx_queue_data0_burst 15.0

	append bss_conf "$N# TX queue configs" "$N"
	for opt in $TX_Q_CONFIGS; do
		local val=$(eval "echo \${$opt}")
		append bss_conf "$opt=$val" "$N"
	done 
	append bss_conf "# End of TX queue configs" "$N"
	append bss_conf "$N"

	json_get_vars $WMM_AC_CONFIGS
	set_default tx_queue_data3_aifs 7
	set_default tx_queue_data3_cwmin 15
	set_default tx_queue_data3_cwmax 1023
	set_default tx_queue_data3_burst 15.0
	
	set_default tx_queue_data2_aifs 3
	set_default tx_queue_data2_cwmin 15
	set_default tx_queue_data2_cwmax 1023
	set_default tx_queue_data2_burst 15.0

	set_default tx_queue_data1_aifs 2
	set_default tx_queue_data1_cwmin 7
	set_default tx_queue_data1_cwmax 15
	set_default tx_queue_data1_burst 15.0

	set_default tx_queue_data0_aifs 2
	set_default tx_queue_data0_cwmin 3
	set_default tx_queue_data0_cwmax 7
	set_default tx_queue_data0_burst 15.0

	set_default wmm_ac_bk_aifs 7
	set_default wmm_ac_bk_cwmin 4
	set_default wmm_ac_bk_cwmax 10
	set_default wmm_ac_bk_txop_limit 469
	set_default wmm_ac_bk_acm 0

	set_default wmm_ac_be_aifs 3
	set_default wmm_ac_be_cwmin 4
	set_default wmm_ac_be_cwmax 10
	set_default wmm_ac_be_txop_limit 469
	set_default wmm_ac_be_acm 0

	set_default wmm_ac_vi_aifs 2
	set_default wmm_ac_vi_cwmin 3
	set_default wmm_ac_vi_cwmax 4
	set_default wmm_ac_vi_txop_limit 469
	set_default wmm_ac_vi_acm 0

	set_default wmm_ac_vo_aifs 2
	set_default wmm_ac_vo_cwmin 2
	set_default wmm_ac_vo_cwmax 3
	set_default wmm_ac_vo_txop_limit 469
	set_default wmm_ac_vo_acm 0

	append bss_conf "# WMM parameters" "$N"
	for opt in $WMM_AC_CONFIGS; do
		local val=$(eval "echo \${$opt}")
		append bss_conf "$opt=$val" "$N"
	done 
	append bss_conf "# End of WMM parameters" "$N"
	append bss_conf "$N"


	append "$var" "$bss_conf" "$N"
	return 0
}

morse_override_wpa_supplicant_add_network() {
	local ifname="$1"
	local freq="$2"
	local htmode="$3"
	local noscan="$4"
	local enable_sgi="$5"

	_wpa_supplicant_common "$1"
	wireless_vif_parse_encryption
	morse_override_wpa3_encryption

	json_get_vars \
		ssid bssid key \
		basic_rate mcast_rate \
		ieee80211w ieee80211r fils ocv \
		multi_ap dpp \
		beacon_int

	# Note that owe_group is a list in UCI, but the wpa supplicant conf file
	# (unlike the hostapd one!) expects a single group rather than a list
	# of preferences.
	if json_is_a owe_group array
	then
		json_select owe_group
		json_get_var owe_group 1
		json_select ..
	fi

	[ -n "$ocv" ] && append bss_conf "ocv=$ocv" "$N"

	case "$auth_type" in
		sae|owe|eap2|eap192)
			set_default ieee80211w 2
		;;
		psk-sae)
			set_default ieee80211w 1
		;;
	esac

	set_default ieee80211r 0
	set_default multi_ap 0

	local key_mgmt='NONE'
	local network_data=
	local T="	"

	local scan_ssid="scan_ssid=1"
	local freq wpa_key_mgmt

	[ "$_w_mode" = "adhoc" ] && {
		append network_data "mode=1" "$N$T"
		[ -n "$freq" ] && wpa_supplicant_set_fixed_freq "$freq" "$htmode"
		[ "$noscan" = "1" ] && append network_data "noscan=1" "$N$T"

		scan_ssid="scan_ssid=0"

		[ "$_w_driver" = "nl80211" ] ||	append wpa_key_mgmt "WPA-NONE"

		[ -n "$channel" ] && append network_data "channel=$channel" "$N$T"
		[ -n "$op_class" ] && append network_data "op_class=$op_class" "$N$T"
		[ -n "$country" ] && append network_data "country=\"$country\"" "$N$T"
		[ -n "$s1g_prim_chwidth" ] && append network_data "s1g_prim_chwidth=$s1g_prim_chwidth" "$N$T"
		[ -n "$s1g_prim_1mhz_chan_index" ] && append network_data "s1g_prim_1mhz_chan_index=$s1g_prim_1mhz_chan_index" "$N$T"

		# Some preliminary testing shows our chip only seems to be able to support an open adhoc network
		# This also appears to be backed up by the work to the bring up scripts used by software
		# So for now, let's force none as an auth and warn.
		case "$auth_type" in
			none)
			;;

			*)
				auth_type="none"
				echo "adhoc currently only supports open networks. Forcing auth_type=none!"
			;;
		esac
		set_default beacon_int 100
	}

	[ "$_w_mode" = "mesh" ] && {
		json_get_vars mesh_id dtim_period encryption
		[ -n "$mesh_id" ] && ssid="${mesh_id}"
		[ -n "$mesh_max_peer_links" ] && append mesh_data "max_peer_links=${mesh_max_peer_links}" "$N"
		[ -n "$mesh_plink_timeout" ] && append mesh_data "mesh_max_inactivity=${mesh_plink_timeout}" "$N"
		[ -n "$mesh_fwding" ] && append mesh_data "mesh_fwding=${mesh_fwding}" "$N"

		append network_data "mode=5" "$N$T"
		[ -n "$channel" ] && append network_data "channel=$channel" "$N$T"
		[ -n "$op_class" ] && append network_data "op_class=$op_class" "$N$T"
		[ -n "$country" ] && append network_data "country=\"$country\"" "$N$T"
		[ -n "$s1g_prim_chwidth" ] && append network_data "s1g_prim_chwidth=$s1g_prim_chwidth" "$N$T"
		[ -n "$s1g_prim_1mhz_chan_index" ] && append network_data "s1g_prim_1mhz_chan_index=$s1g_prim_1mhz_chan_index" "$N$T"
		[ -n "$dtim_period" ] && append network_data "dtim_period=$dtim_period" "$N$T"

		[ -n "$mesh_rssi_threshold" ] && append network_data "mesh_rssi_threshold=${mesh_rssi_threshold}" "$N$T"
		[ -n "$mesh_hwmp_rootmode" ] && append network_data "dot11MeshHWMPRootMode=${mesh_hwmp_rootmode}" "$N$T"
		[ -n "$mesh_gate_announcements" ] && append network_data "dot11MeshGateAnnouncements=${mesh_gate_announcements}" "$N$T"
		[ -n "$mbca_config" ] && append network_data "mbca_config=${mbca_config}" "$N$T"
		[ -n "$mbca_min_beacon_gap_ms" ] && append network_data "mbca_min_beacon_gap_ms=${mbca_min_beacon_gap_ms}" "$N$T"
		[ -n "$mbca_tbtt_adj_interval_sec" ] && append network_data "mbca_tbtt_adj_interval_sec=${mbca_tbtt_adj_interval_sec}" "$N$T"
		[ -n "$mesh_beacon_timing_report_int" ] && append network_data "dot11MeshBeaconTimingReportInterval=${mesh_beacon_timing_report_int}" "$N$T"
		[ -n "$mbss_start_scan_duration_ms" ] && append network_data "mbss_start_scan_duration_ms=${mbss_start_scan_duration_ms}" "$N$T"
		[ -n "$mesh_beacon_less_mode" ] && append network_data "mesh_beaconless_mode=${mesh_beacon_less_mode}" "$N$T"
		[ -n "$mesh_dynamic_peering" ] && append network_data "mesh_dynamic_peering=${mesh_dynamic_peering}" "$N$T"
		[ -n "$mesh_rssi_margin" ] && append network_data "mesh_rssi_margin=${mesh_rssi_margin}" "$N$T"
		[ -n "$mesh_blacklist_timeout" ] && append network_data "mesh_blacklist_timeout=${mesh_blacklist_timeout}" "$N$T"

		[ "$encryption" = "none" -o -z "$encryption" ] || append wpa_key_mgmt "SAE"
		scan_ssid=""
		set_default beacon_int 1000

		[ "$enable_sgi" = 0 ] && append network_data "disable_s1g_sgi=1" "$N$T"
	}

	[ "$multi_ap" = 1 -a "$_w_mode" = "sta" ] && append network_data "multi_ap_backhaul_sta=1" "$N$T"

	[ -n "$ocv" ] && append network_data "ocv=$ocv" "$N$T"

	case "$auth_type" in
		none) ;;
		owe)
			hostapd_append_wpa_key_mgmt
			key_mgmt="$wpa_key_mgmt"

			[ -n "$owe_group" ] && append network_data "owe_group=$owe_group" "$N$T"
		;;
		wep)
			local wep_keyidx=0
			hostapd_append_wep_key network_data
			append network_data "wep_tx_keyidx=$wep_keyidx" "$N$T"
		;;
		wps)
			key_mgmt='WPS'
		;;
		psk|sae|psk-sae)
			local passphrase

			if [ "$_w_mode" != "mesh" ]; then
				hostapd_append_wpa_key_mgmt
			fi

			key_mgmt="$wpa_key_mgmt"

			if [ "$_w_mode" = "mesh" ] || [ "$auth_type" = "sae" ]; then
				passphrase="sae_password=\"${key}\""
			else
				if [ ${#key} -eq 64 ]; then
					passphrase="psk=${key}"
				else
					passphrase="psk=\"${key}\""
				fi
			fi
			append network_data "$passphrase" "$N$T"
		;;
		eap|eap2|eap192)
			hostapd_append_wpa_key_mgmt
			key_mgmt="$wpa_key_mgmt"

			json_get_vars eap_type identity anonymous_identity ca_cert ca_cert_usesystem

			[ "$fils" -gt 0 ] && append network_data "erp=1" "$N$T"
			if [ "$ca_cert_usesystem" -eq "1" -a -f "/etc/ssl/certs/ca-certificates.crt" ]; then
				append network_data "ca_cert=\"/etc/ssl/certs/ca-certificates.crt\"" "$N$T"
			else
				[ -n "$ca_cert" ] && append network_data "ca_cert=\"$ca_cert\"" "$N$T"
			fi
			[ -n "$identity" ] && append network_data "identity=\"$identity\"" "$N$T"
			[ -n "$anonymous_identity" ] && append network_data "anonymous_identity=\"$anonymous_identity\"" "$N$T"
			case "$eap_type" in
				tls)
					json_get_vars client_cert priv_key priv_key_pwd
					# When PKCS#12/PFX file (.p12/.pfx) is used, client_cert should be commented out
					if [ -n "$priv_key" ]; then
						case "$priv_key" in
							*".p12")
								;;
							*".pfx")
								;;
							*)
								append network_data "client_cert=\"$client_cert\"" "$N$T"
								;;
						esac
					fi
					append network_data "private_key=\"$priv_key\"" "$N$T"
					append network_data "private_key_passwd=\"$priv_key_pwd\"" "$N$T"

					json_get_vars subject_match
					[ -n "$subject_match" ] && append network_data "subject_match=\"$subject_match\"" "$N$T"

					json_get_values altsubject_match altsubject_match
					if [ -n "$altsubject_match" ]; then
						local list=
						for x in $altsubject_match; do
							append list "$x" ";"
						done
						append network_data "altsubject_match=\"$list\"" "$N$T"
					fi

					json_get_values domain_match domain_match
					if [ -n "$domain_match" ]; then
						local list=
						for x in $domain_match; do
							append list "$x" ";"
						done
						append network_data "domain_match=\"$list\"" "$N$T"
					fi

					json_get_values domain_suffix_match domain_suffix_match
					if [ -n "$domain_suffix_match" ]; then
						local list=
						for x in $domain_suffix_match; do
							append list "$x" ";"
						done
						append network_data "domain_suffix_match=\"$list\"" "$N$T"
					fi
				;;
				fast|peap|ttls)
					json_get_vars auth password ca_cert2 ca_cert2_usesystem client_cert2 priv_key2 priv_key2_pwd
					set_default auth MSCHAPV2

					if [ "$auth" = "EAP-TLS" ]; then
						if [ "$ca_cert2_usesystem" -eq "1" -a -f "/etc/ssl/certs/ca-certificates.crt" ]; then
							append network_data "ca_cert2=\"/etc/ssl/certs/ca-certificates.crt\"" "$N$T"
						else
							[ -n "$ca_cert2" ] && append network_data "ca_cert2=\"$ca_cert2\"" "$N$T"
						fi
						append network_data "client_cert2=\"$client_cert2\"" "$N$T"
						append network_data "private_key2=\"$priv_key2\"" "$N$T"
						append network_data "private_key2_passwd=\"$priv_key2_pwd\"" "$N$T"
					else
						append network_data "password=\"$password\"" "$N$T"
					fi

					json_get_vars subject_match
					[ -n "$subject_match" ] && append network_data "subject_match=\"$subject_match\"" "$N$T"

					json_get_values altsubject_match altsubject_match
					if [ -n "$altsubject_match" ]; then
						local list=
						for x in $altsubject_match; do
							append list "$x" ";"
						done
						append network_data "altsubject_match=\"$list\"" "$N$T"
					fi

					json_get_values domain_match domain_match
					if [ -n "$domain_match" ]; then
						local list=
						for x in $domain_match; do
							append list "$x" ";"
						done
						append network_data "domain_match=\"$list\"" "$N$T"
					fi

					json_get_values domain_suffix_match domain_suffix_match
					if [ -n "$domain_suffix_match" ]; then
						local list=
						for x in $domain_suffix_match; do
							append list "$x" ";"
						done
						append network_data "domain_suffix_match=\"$list\"" "$N$T"
					fi

					phase2proto="auth="
					case "$auth" in
						"auth"*)
							phase2proto=""
						;;
						"EAP-"*)
							auth="$(echo $auth | cut -b 5- )"
							[ "$eap_type" = "ttls" ] &&
								phase2proto="autheap="
							json_get_vars subject_match2
							[ -n "$subject_match2" ] && append network_data "subject_match2=\"$subject_match2\"" "$N$T"

							json_get_values altsubject_match2 altsubject_match2
							if [ -n "$altsubject_match2" ]; then
								local list=
								for x in $altsubject_match2; do
									append list "$x" ";"
								done
								append network_data "altsubject_match2=\"$list\"" "$N$T"
							fi

							json_get_values domain_match2 domain_match2
							if [ -n "$domain_match2" ]; then
								local list=
								for x in $domain_match2; do
									append list "$x" ";"
								done
								append network_data "domain_match2=\"$list\"" "$N$T"
							fi

							json_get_values domain_suffix_match2 domain_suffix_match2
							if [ -n "$domain_suffix_match2" ]; then
								local list=
								for x in $domain_suffix_match2; do
									append list "$x" ";"
								done
								append network_data "domain_suffix_match2=\"$list\"" "$N$T"
							fi
						;;
					esac
					append network_data "phase2=\"$phase2proto$auth\"" "$N$T"
				;;
			esac
			eap_type_data=$(echo $eap_type | tr 'a-z' 'A-Z')
			append network_data "eap=$eap_type_data" "$N$T"
		;;
	esac

	[ "$wpa_cipher" = GCMP ] && {
		append network_data "pairwise=GCMP" "$N$T"
		append network_data "group=GCMP" "$N$T"
	}

	[ "$wpa_cipher" = CCMP ] && append network_data "pairwise=CCMP" "$N$T"

	[ "$mode" = mesh ] || {
		case "$wpa" in
			1)
				[  ${multi_ap:=0} = 0 ] && append network_data "proto=WPA" "$N$T"
			;;
			2)
				[ ${multi_ap:=0}  = 0 ] && append network_data "proto=RSN" "$N$T"
			;;
		esac
	}

	case "$ieee80211w" in
		[012])
			[ "$wpa" -ge 2 ] && append network_data "ieee80211w=$ieee80211w" "$N$T"
		;;
	esac

	[ -n "$bssid" ] && append network_data "bssid=$bssid" "$N$T"
	[ -n "$beacon_int" ] && append network_data "beacon_int=$beacon_int" "$N$T"

	local bssid_blacklist bssid_whitelist
	json_get_values bssid_blacklist bssid_blacklist
	json_get_values bssid_whitelist bssid_whitelist

	[ -n "$bssid_blacklist" ] && append network_data "bssid_blacklist=$bssid_blacklist" "$N$T"
	[ -n "$bssid_whitelist" ] && append network_data "bssid_whitelist=$bssid_whitelist" "$N$T"

	[ -n "$basic_rate" ] && {
		local br rate_list=
		for br in $basic_rate; do
			wpa_supplicant_add_rate rate_list "$br"
		done
		[ -n "$rate_list" ] && append network_data "rates=$rate_list" "$N$T"
	}

	[ -n "$mcast_rate" ] && {
		local mc_rate=
		wpa_supplicant_add_rate mc_rate "$mcast_rate"
		append network_data "mcast_rate=$mc_rate" "$N$T"
	}

	json_get_vars raw_sta_priority
	[ -n "$raw_sta_priority" ] && append network_data "raw_sta_priority=$raw_sta_priority" "$N$T"

	json_get_vars cac
	[ -n "$cac" ] && append network_data "cac=$cac" "$N$T"

	json_get_vars twt wake_interval min_wake_duration setup_command
	twt_block=
	if [ "${twt:=0}" -eq "1" ]; then
		append twt_block "twt={" "$N$T$T"
		append twt_block "wake_interval=${wake_interval:=300000000}" "$N$T$T"
		append twt_block "min_wake_duration=${min_wake_duration:=65280}" "$N$T$T"
		append twt_block "setup_command=${setup_command:=0}" "$N$T$T"
		append twt_block "}" "$N$T"
	fi

	if [ "$multi_ap" -eq 1 ]; then
		echo "wps_priority=1" >> "$_config"
		echo "update_config=1"  >> "$_config"
		#Adding obtained wps network credentials here to make it persistent
		append network_data "proto=RSN" "$N$T"
		if [ -z "$ssid" ]; then
			echo "No creds in UCI to update network config"
		else
			wpa_add_network_block "multiap_bh" "$_config"
		fi
	elif [ "$dpp" = 1 ]; then
		echo "update_config=1"  >> "$_config"
		echo "ctrl_interface_group=0"  >> "$_config"
		echo "dpp_config_processing=0"  >> "$_config"
		echo "dpp_key=/etc/dpp_key.pem"  >> "$_config"
		echo "dpp_chirp_forever=1"  >> "$_config"

	else
		wpa_add_network_block "sta" "$_config"
	fi
	return 0
}


# $1: network type (sta, multiap_bh, ...)
# $2: output file
wpa_add_network_block()
{
	local network_type=$1
	local output_file=$2
	case "$network_type" in
		sta)
			cat >> "$output_file" <<EOF
$mesh_data
network={
	$scan_ssid
	ssid="$ssid"
	key_mgmt=$key_mgmt
	$network_data
	$twt_block
}
EOF
		;;
		multiap_bh)
			cat >> "$_config" <<EOF
network={
	$scan_ssid
	ssid="$ssid"
	key_mgmt=$key_mgmt
	$network_data
	mesh_fwding=1
}
EOF
	esac

}
