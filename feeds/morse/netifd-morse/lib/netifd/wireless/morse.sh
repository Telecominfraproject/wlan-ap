#!/bin/sh

. /lib/netifd/netifd-wireless.sh
. /lib/netifd/hostapd.sh
. /lib/netifd/morse/morse_overrides.sh
. /lib/netifd/morse/morse_utils.sh

# Morse interfaces are named wlanX on EAP112, not upstream's wlhX.
# Kept because the rest of this tree assumes wlan*: morse_utils.sh's
# morse_find_ifname globs /sys/class/net/wlan*, morse_get_old_ifname only
# matches wlan*, and feeds/morse/utils/morsecli deliberately does not set
# DEFAULT_INTERFACE_NAME so morse_cli's built-in default stays wlan0. This is
# the only place the prefix is defined - _find_free_ifname consumes it.
DEFAULT_MORSE_IFNAME="wlan"

echo "Configuring morse device"
device_section="$3"
init_wireless_driver "$@"

# These three lists must only name parameters that the loaded driver actually
# declares with module_param(). A name the driver does not know is not an error:
# the kernel just prints "morse: unknown parameter '<name>' ignored" once at
# insmod and the UCI option silently does nothing, which is easy to mistake for
# a working knob. On every driver upgrade, re-diff these lists against the
# driver's own module_param() declarations and check the boot log for
# "unknown parameter .* ignored".
MM_MOD_INT="watchdog_interval_secs max_rates max_rate_tries spi_clock_speed max_txq_len virtual_sta_max max_aggregation_count
			default_cmd_timeout_ms sdio_reset_time tx_max_power_mbm max_mc_frames duty_cycle_mode ocs_type fixed_mcs fixed_bw
			fixed_ss fixed_guard tx_status_lifetime_ms max_total_vendor_ie_bytes coredump_include coredump_method
			hw_reload_after_stop slow_clock_mode rsn_beacon_mode hw_scan_prim_deconstruct duty_cycle_probe_retry_threshold
			spi_inter_block_delay_bytes tx_queued_lifetime_ms skbq_refill_margin"
MM_MOD_BOOL="enable_mac80211_connection_monitor mcs10_mode enable_rts_8mhz
			enable_otp_check enable_survey enable_subbands enable_ps enable_trav_pilot
			enable_watchdog no_hwcrypt enable_arp_offload enable_dynamic_ps_offload
			enable_coredump enable_mbssid_ie enable_cts_to_self enable_airtime_fairness
			enable_twt enable_bcn_change_seq_monitor enable_dhcpc_offload enable_ibss_probe_filtering enable_auto_duty_cycle
			enable_auto_mpsw enable_mcast_whitelist log_modparams_on_boot enable_fixed_rate spi_use_edge_irq
			enable_sched_scan enable_1mhz_probes enable_ext_xtal_init
			enable_hw_scan enable_mcast_rate_control enable_mm_vendor_ie
			enable_page_slicing enable_pv1
			enable_short_bcn_as_dtim_override enable_hw_leds enable_pre_assoc_ps
			enable_sgi_rc"
MM_MOD_STRING="serial country debug_mask macaddr_octet mcs_mask dhcpc_lease_update_script
			fw_bin_file"
MM_MOD_UNKNOWN=
MOD_PARAMS=

TX_Q_CONFIGS=" tx_queue_data3_aifs tx_queue_data3_cwmin tx_queue_data3_cwmax tx_queue_data3_burst
			   tx_queue_data2_aifs tx_queue_data2_cwmin tx_queue_data2_cwmax tx_queue_data2_burst
			   tx_queue_data1_aifs tx_queue_data1_cwmin tx_queue_data1_cwmax tx_queue_data1_burst
			   tx_queue_data0_aifs tx_queue_data0_cwmin tx_queue_data0_cwmax tx_queue_data0_burst"
WMM_AC_CONFIGS="wmm_ac_bk_aifs wmm_ac_bk_cwmin wmm_ac_bk_cwmax wmm_ac_bk_txop_limit wmm_ac_bk_acm
				wmm_ac_be_aifs wmm_ac_be_cwmin wmm_ac_be_cwmax wmm_ac_be_txop_limit wmm_ac_be_acm
				wmm_ac_vi_aifs wmm_ac_vi_cwmin wmm_ac_vi_cwmax wmm_ac_vi_txop_limit wmm_ac_vi_acm
				wmm_ac_vo_aifs wmm_ac_vo_cwmin wmm_ac_vo_cwmax wmm_ac_vo_txop_limit wmm_ac_vo_acm "
RAW_BLOCK_CONFIGS="raw0_enabled raw0_start_time_us raw0_duration_us raw0_slots raw0_cross_slot
				raw0_max_beacon_spread raw0_nominal_stas_per_beacon raw0_start_offset raw0_period
				raw1_enabled raw1_start_time_us raw1_duration_us raw1_slots raw1_cross_slot
				raw1_max_beacon_spread raw1_nominal_stas_per_beacon raw1_start_offset raw1_period
				raw2_enabled raw2_start_time_us raw2_duration_us raw2_slots raw2_cross_slot
				raw2_max_beacon_spread raw2_nominal_stas_per_beacon raw2_start_offset raw2_period
				raw3_enabled raw3_start_time_us raw3_duration_us raw3_slots raw3_cross_slot
				raw3_max_beacon_spread raw3_nominal_stas_per_beacon raw3_start_offset raw3_period
				raw4_enabled raw4_start_time_us raw4_duration_us raw4_slots raw4_cross_slot
				raw4_max_beacon_spread raw4_nominal_stas_per_beacon raw4_start_offset raw4_period
				raw5_enabled raw5_start_time_us raw5_duration_us raw5_slots raw5_cross_slot
				raw5_max_beacon_spread raw5_nominal_stas_per_beacon raw5_start_offset raw5_period
				raw6_enabled raw6_start_time_us raw6_duration_us raw6_slots raw6_cross_slot
				raw6_max_beacon_spread raw6_nominal_stas_per_beacon raw6_start_offset raw6_period
				raw7_enabled raw7_start_time_us raw7_duration_us raw7_slots raw7_cross_slot
				raw7_max_beacon_spread raw7_nominal_stas_per_beacon raw7_start_offset raw7_period"

check_cac(){
	json_select config
	json_get_vars cac
	if [ "${cac:-0}" -gt 0 ]; then
		enable_cac=1
	fi
	json_select ..
}

check_powersave_iface() {
	local disabled powersave

	json_select config
	json_get_var disabled disabled

	if [ "$disabled" = 1 ]; then
		json_select ..
		return
	fi

	num_iface=$((num_iface + 1))

	json_get_var powersave powersave
	if [ "$powersave" = "1" ]; then
		ps_requested_by_iface=1
	fi

	json_select ..
}


get_sgi_config() {
	enable_sgi=1
	json_select config

	if json_is_a s1g_capab array
	then
		json_select s1g_capab
		idx=1
		while json_is_a ${idx} string
		do
			json_get_var capab $idx
			[ "${capab}" = "[SHORT-GI-NONE]" ] && enable_sgi=0
			idx=$(( idx + 1 ))
		done
		json_select ..
	fi

	json_select ..
}

# Determines if we should change the specified bcf because
# of the vfem_4v3 parameters (and reports errors if nonsense).
get_vfem_4v3_bcf() {
	local bcf="$1"

	boost_bcf="${bcf/.bin/_4v3.bin}"

	if ! gpioinfo -s --by-name MM_BOOST > /dev/null 2>&1; then
		echo "WARNING: Unable to set 4.3v vfem as no MM_BOOST gpio configured" >&2
	elif [ -z "$bcf" ]; then
		echo "WARNING: Unable to set 4.3v vfem if no explicit BCF" >&2
	elif ! [ -f "/lib/firmware/morse/$boost_bcf" ]; then
		echo "WARNING: Unable to set 4.3v vfem as $boost_bcf doesn't exist" >&2
	else
		echo "$boost_bcf"
		return 0
	fi
}

build_mod_params() {
	json_select config
	json_get_vars bcf vfem_4v3 firmware_type chan_test_mode

	if [ -z "$bcf" ]; then
		mm_sku=$(persistent_vars_storage.sh READ mm_sku 2> /dev/null | tr 'A-Z-' 'a-z_')
		if [ -n "$mm_sku" ]; then
			proposed_bcf="bcf_$mm_sku.bin"
			if [ -e "/lib/firmware/morse/$proposed_bcf" ]; then
				bcf="$proposed_bcf"
			fi
		fi
	fi

	# Remove 4v3 from BCF in case someone's tried to force it
	# (backwards compat?).
	bcf="${bcf/_4v3.bin/.bin}"
	if [ "$vfem_4v3" = 1 ]; then
		vfem_4v3_bcf=$(get_vfem_4v3_bcf "$bcf")
		if [ -n "$vfem_4v3_bcf" ]; then
			bcf="$vfem_4v3_bcf"
		else
			vfem_4v3=0
		fi
	fi

	case "$s1g_chzn" in
	80211_2020)
		DOT11AH_MOD_PARAMS="channelization_scheme=1"
		;;
	80211_2024)
		DOT11AH_MOD_PARAMS="channelization_scheme=2"
		;;
	80211_revmf)
		DOT11AH_MOD_PARAMS="channelization_scheme=3"
		;;
	esac

	# Without this, we try to re-attach to running firmware
	# which breaks some of our assumptions around bcf loading etc.
	MOD_PARAMS="$MOD_PARAMS reattach_hw=0"

	if [ -n "$bcf" ]; then
		MOD_PARAMS="$MOD_PARAMS bcf=$bcf"
	fi

	if [ "$vfem_4v3" = 1 ]; then
		MOD_PARAMS="$MOD_PARAMS enable_4v3_fem=1"
	fi

	for var in $MM_MOD_BOOL $MM_MOD_INT $MM_MOD_STRING; do
		json_get_var mm_mod_val "$var"
		[ -n "$mm_mod_val" ] && MOD_PARAMS="$MOD_PARAMS $var=$mm_mod_val"
	done

	case "$firmware_type" in
		''|softmac)
			;;
		fullmac)
			MOD_PARAMS="$MOD_PARAMS enable_wiphy=1"
			;;
		thin_lmac)
			MOD_PARAMS="$MOD_PARAMS thin_lmac=1"
			;;
		*)
			echo "WARNING: ignoring unknown firmware_type '$firmware_type'"
			;;
	esac

	json_select ..
	enable_cac=
	for_each_interface "ap" check_cac
	[ -n "$enable_cac" ] && MOD_PARAMS="$MOD_PARAMS enable_cac=$enable_cac"

	# Get the last three octets of the eth0 MAC address
	# to use as the default HaLow MAC address
	local ETH0_MAC_SUFFIX=`cat /sys/class/net/eth0/address | cut -d: -f4-`

	MOD_PARAMS="$MOD_PARAMS macaddr_suffix=$ETH0_MAC_SUFFIX"

	num_iface=0
	ps_requested_by_iface=0
	for_each_interface "ap sta adhoc mesh none monitor" check_powersave_iface

	# APP-4887: Keep powersave disabled by default on USB-based Morse devices to ensure the LED remains functional.
	is_usb=0
	case "$path" in *usb*) is_usb=1 ;; esac

	ps_disabled_by_modparam=0
	if [ "$num_iface" -gt 1 ]; then
		MOD_PARAMS="$MOD_PARAMS enable_ps=0"
		ps_disabled_by_modparam=1
		[ "$ps_requested_by_iface" -eq 1 ] && echo "WARNING: Power save feature is not supported in multi interface"
	# Upstream only drops power save on USB here. EAP112 is SPI and mains
	# powered, so keep it off as well; "option powersave 1" on a wifi-iface re-enables.
	elif [ "$ps_requested_by_iface" -eq 0 ]; then
		MOD_PARAMS="$MOD_PARAMS enable_ps=0"
		ps_disabled_by_modparam=1
	fi

	MOD_PARAMS=`echo $MOD_PARAMS | xargs`
}

# EAP112 helpers that the vendor handler does not have.

# Returns the wlanN the driver already owns so it can be reused
# instead of drifting to wlan1 on every reinsertion. Matches wlan* only.
morse_get_old_ifname()
{
	local _phy=$1
	local _oldifname="$(basename "/sys/class/ieee80211/${_phy}/device/net"/* 2>/dev/null)"

	if [[ "$_oldifname" == "wlan"* ]]; then
		echo "$_oldifname"
	else
		echo ""
	fi
}

# Publishes the chip HW revision through system.@system[0].notes.
set_chipid() {
	local _ifname=$1
	local state=$(cat /sys/class/net/${_ifname}/operstate 2>/dev/null)
	[ $? -ne 0 ] && return

	if [ "$state" == "down" ]; then
		ip link set ${_ifname} up
		[ $? -ne 0 ] && return
	fi
	local chip_revision=$(morse_cli -i ${_ifname} hw_version 2>/dev/null)
	[ $? -ne 0 ] && return
	if echo ${chip_revision} | grep -Fqe "HW Version:"; then
		chip_revision=${chip_revision##"HW Version: "}
		uci set system.@system[0].notes="${chip_revision}"
		uci commit system
	else
		return
	fi
	if [ "$state" == "down" ]; then
		ip link set ${_ifname} down
	fi
}

# Resolves the UCI network of a morse interface; used to hand a STA its bridge.
morse_ifname=
morse_network=

get_morse_iface() {
	local _ifname
	local _network
	config_get _ifname $1 ifname
	config_get _network $1 network
	[ "${_ifname}" == "${morse_ifname}" ] && morse_network=$_network
}

drv_morse_cleanup() {
	hostapd_common_cleanup
}

drv_morse_init_device_config() {
	hostapd_common_add_device_config

	config_add_string path phy 'macaddr:macaddr'
	config_add_string tx_burst
	config_add_int frag rts
	config_add_int op_class
	config_add_int txpower
	config_add_int s1g_chanbw
	config_add_boolean svt_restricted_mode
	config_add_int s1g_prim_chwidth
	config_add_string s1g_prim_1mhz_chan_index
	config_add_boolean ampdu
	config_add_int forced_listen_interval
	config_add_boolean noscan
	config_add_array s1g_capab
	config_add_array channels
	config_add_boolean vendor_keep_alive_offload
	config_add_boolean vfem_4v3
	config_add_string firmware_type
	config_add_string s1g_chzn

	#channel test mode parameters
	config_add_string chan_test_freq
	config_add_int chan_test_bw
	config_add_int chan_test_prim_chwidth
	config_add_int chan_test_prim_chan_index

	#module parameters
	config_add_string bcf  # handled separately due to boost bcf
	config_add_int $MM_MOD_INT
	config_add_boolean $MM_MOD_BOOL
	config_add_string $MM_MOD_STRING $MM_MOD_UNKNOWN
}


drv_morse_init_iface_config() {
	hostapd_common_add_bss_config
	config_add_string 'macaddr:macaddr' ifname
	config_add_boolean wds powersave enable
	config_add_string wds_bridge
	config_add_boolean wps_virtual_push_button
	config_add_boolean dpp_configurator_connectivity
	config_add_array sae_group
	config_add_array owe_group
	config_add_int maxassoc
	config_add_int max_listen_int
	config_add_int dtim_period
	config_add_int s1g_bss_color
	config_add_int start_disabled
	config_add_int sae_pwe
	config_add_string $TX_Q_CONFIGS
	config_add_string $WMM_AC_CONFIGS
	config_add_string ca_cert2
	config_add_string client_cert2
	config_add_string priv_key2
	config_add_string priv_key2_pwd
	config_add_string password

	#twt
	config_add_boolean twt
	config_add_string wake_interval
	config_add_int min_wake_duration setup_command

	#cac
	config_add_boolean cac

	#auth_retry_backoff
	config_add_string auth_retry_backoff

	#raw
	config_add_int raw_sta_priority
	config_add_int raw
	config_add_string $RAW_BLOCK_CONFIGS

	# mesh
	config_add_string mesh_id
	# When set, this node never initiates mesh peering by itself; peers must
	# be selected explicitly via
	#   wpa_cli_s1g -p /var/run/wpa_supplicant_s1g -i <ifname> mesh_peer_add <MAC>
	# Note: this only suppresses locally initiated peering. A remote node with
	# auto peering enabled can still bring the link up, so set it on both ends
	# when a fixed topology is required.
	config_add_boolean no_auto_peer

	#dpp
	config_add_boolean dpp

	#beacon interval
	config_add_int beacon_int
}

get_mesh11sd_config() {
	config_load mesh11sd
	var=

	json_select config

	config_get var mesh_params mesh_fwding
	json_add_boolean mesh_fwding $var

	config_get var mesh_params mesh_rssi_threshold
	json_add_int mesh_rssi_threshold $var

	config_get var mesh_params mesh_max_peer_links
	json_add_int mesh_max_peer_links $var

	config_get var mesh_params mesh_plink_timeout
	json_add_int mesh_plink_timeout $var

	config_get var mesh_params mesh_hwmp_rootmode
	json_add_int mesh_hwmp_rootmode $var

	config_get var mesh_params mesh_gate_announcements
	json_add_int mesh_gate_announcements $var

	config_get var mbca mbca_config
	json_add_int mbca_config $var

	config_get var mbca mbca_min_beacon_gap_ms
	json_add_int mbca_min_beacon_gap_ms $var

	config_get var mbca mbca_tbtt_adj_interval_sec
	json_add_int mbca_tbtt_adj_interval_sec $var

	config_get var mbca mesh_beacon_timing_report_int
	json_add_int mesh_beacon_timing_report_int $var

	config_get var mbca mbss_start_scan_duration_ms
	json_add_int mbss_start_scan_duration_ms $var

	config_get var mesh_beaconless mesh_beacon_less_mode
	json_add_int mesh_beacon_less_mode $var

	config_get var mesh_dynamic_peering enabled 0
	json_add_int mesh_dynamic_peering $var

	config_get var mesh_dynamic_peering mesh_rssi_margin
	json_add_int mesh_rssi_margin $var

	config_get var mesh_dynamic_peering mesh_blacklist_timeout
	json_add_int mesh_blacklist_timeout $var

	json_select ..
}

get_matter_config() {
	# No matter package installed: report matter as disabled.
	if [ ! -f /etc/config/matter ]; then
		json_select config
		json_add_int matter_enable 0
		json_select ..
		return
	fi
	config_load matter
	var=

	json_select config

	config_get var config enable
	json_add_int matter_enable $var

	config_get var config ble_proto
	json_add_string ble_proto "$var"

	config_get var config ble_uart_port
	json_add_string ble_uart_port "$var"

	json_select ..
}

is_module_loaded() {
	test -e "/sys/module/$1"
}

# Name of the phy the morse driver registered, if any.
morse_phy_name() {
	local p
	for p in /sys/class/ieee80211/*; do
		[ -e "$p" ] || continue
		case "$(readlink -f "$p/device/driver")" in
		*/morse_*)
			echo "${p##*/}"
			return 0
			;;
		esac
	done
}

# cfg80211 never reuses a phy index, so a reloaded driver comes back under a
# new name. Give the HaLow phy back the name it had before the reload.
morse_restore_phy_name() {
	local old="$1" new

	[ -n "$old" ] || return 0
	new="$(morse_phy_name)"
	[ -n "$new" ] && [ "$new" != "$old" ] || return 0
	[ -e "/sys/class/ieee80211/$old" ] && return 0
	iw phy "$new" set name "$old" || echo "Could not rename $new back to $old" >&2
}

change_module_parameters() {
	local module="$1"
	local module_config_file="$2"
	local params="$3"
	local extra_params=""

	if [ "$module" = morse ]; then
		# These are parameters that we use morse_cli to configure,
		# but because we have no way to revert back to the original
		# state any change requires us to reload the module.
		#
		# Therefore we store these as a comment in /etc/modules.d/morse
		# (and changing this comment will mean that we will decide
		# to reload the module; see use of cmp below).
		extra_params="# Extra params: forced_listen_interval=$forced_listen_interval"
	fi

	local proposed_module="$(mktemp)"
	cat > "$proposed_module" <<-END
	$extra_params
	$module $params
	END

	if cmp -s "$proposed_module" "$module_config_file"; then
		# Parameters didn't change; do nothing.
		rm "$proposed_module"
		return 1
	else
		mv "$proposed_module" "$module_config_file"
		return 0
	fi
}

morse_set_chan_test_freq() {
	local ifname="$1"
	local freq="$2"
	local bw="$3"
	local prim_chwidth="$4"
	local prim_chan_index="$5"

	if [ -z "$freq" ]; then
		echo "Channel test frequency parameter missing" >&2
		return 1
	fi

	set_default bw 1
	set_default prim_chwidth 1
	set_default prim_chan_index 0

	morse_cli -i "$ifname" channel -c "$freq" -o "$bw" -p "$prim_chwidth" -n "$prim_chan_index" || {
		echo "Failed to set channel test frequency parameters via morse_cli" >&2
		return 1
	}

	return 0
}

drv_morse_setup() {
	morse_band_override
	json_select config
	json_get_vars \
		phy macaddr path \
		svt_restricted_mode \
		txpower \
		frag rts htmode \
		ampdu \
		forced_listen_interval \
		firmware_type \
		chan_test_mode
	json_get_vars country s1g_chzn op_class channel \
		s1g_chanbw s1g_prim_chwidth s1g_prim_1mhz_chan_index
	json_get_values basic_rate_list basic_rate

	# --- CE SKU normalisation ---------------------------------------------------
	# CE is the model suffix for the ETSI variant of this board, not a country
	# code. Nothing in the HaLow stack knows it: the regulatory CSV has no CE row,
	# and no board config carries a .regdom_CE section (the ETSI file carries
	# .regdom_EU and .regdom_GB). A CE that reaches the driver makes the firmware
	# report "Country code CE not found in BCF" and the chip probes with no board
	# config, while _get_regulatory finds no row and leaves has_chan_info=0, so
	# every interface is skipped with "requires valid country/channel setup".
	# The equivalence CE == EU == ETSI is already relied on elsewhere in the
	# product, so collapse it here, before any consumer sees it.
	#
	# Two things this has to do, and both are load bearing:
	#   * Rewrite the JSON config, not only the shell variable. build_mod_params
	#     reads every module parameter back out of the JSON with json_get_var, so
	#     a shell-only fix would still hand country=CE to insmod. Overwriting an
	#     existing key this way is the same thing morse_set_chan_info does for
	#     channel and op_class.
	#   * Normalise mm_region identically, below. A unit built as the ETSI variant
	#     can have CE in persistent storage; comparing that against a normalised
	#     EU would trip the region lock and refuse to configure the radio at all -
	#     turning this fix into a worse regression than the bug.
	if [ "$country" = "CE" ]; then
		country="EU"
		json_add_string country "$country"
	fi

	json_select ..

	# Country translation: Morse Micro BCF and regdb do not have VN regulation.
	# Fall back to US operation for VN so driver loads US BCF and channel plan.
	if [ "$country" = "VN" ]; then
		country="US"
		json_select config
		json_add_string country "US"
		json_select ..
	fi

	# --- channelization scheme must match the regulatory data generation -------
	# dot11ah takes a channelization_scheme module parameter, and
	# build_mod_params below only passes one when s1g_chzn is set. Nothing in this
	# tree sets it for Europe: the only writer is
	# netifd-morse/etc/uci-defaults/99-migrate-morse-wifi-version, and that is
	# gated on country = AU (plus /sysupgrade.tgz and morse_wifi_version < 2). So
	# an EU device loads dot11ah with no parameter and gets the driver's built-in
	# default, which is scheme 3 (IEEE802.11-REVmf) - observed directly in dmesg:
	#   "Country DE with channelization scheme 3 (IEEE802.11-REVmf) is not
	#    supported"
	#   "Failed to set regulatory to country DE, staying in EU"
	#
	# That is the wrong generation for this tree. morse-regdb here is pinned to
	# v2.1.1, whose channel numbering is the 802.11-2020 one - EU 1 MHz is
	# s1g_op_class 6 / global_op_class 66 in artefacts/repo_channels.csv - and
	# which carries no `channelizations` column at all, i.e. one channelization per
	# country (see the header discussion in morse_utils.sh). Upstream can default
	# to REVmf because upstream pairs it with regdb v2.7.0, which does carry that
	# column. Leaving the two mismatched means netifd resolves a channel with
	# 802.11-2020 numbering while the driver interprets that number under REVmf.
	#
	# 80211_2020 is the same value upstream's own migration writes for pre-existing
	# configurations, which is the strongest available confirmation that it is the
	# value that goes with v2.1.1 data. Applied as a default only, so an explicit
	# s1g_chzn on the wifi-device still wins - including one pushed from the cloud.
	#
	# Not written back to the JSON: build_mod_params reads s1g_chzn as a shell
	# variable in a case statement, and s1g_chzn is deliberately not in MM_MOD_*,
	# so it is never fetched with json_get_var. _get_regulatory takes it as
	# want_chzn, where chzn_ok() short-circuits to true whenever the
	# `channelizations` column is absent - so on v2.1.1 this cannot change which
	# CSV row is selected.
	#
	# Revisit together with any morse-regdb upgrade: moving to v2.7.0 is what makes
	# the newer schemes selectable per country.
	#
	# Scoped to EU/GB on purpose. EU is the only region with evidence of breakage
	# (the dmesg pair above), and US and JP are in service on the driver's REVmf
	# default today. Applying this everywhere would change how the driver maps
	# channel numbers for them too, with no evidence that they are wrong and no way
	# to check it here - the driver source is fetched at build time and is not in
	# the tree. If US/JP turn out to need the same alignment, widen this case; do
	# not widen it speculatively. `country` has already been normalised above, so
	# the CE model suffix is covered by the EU label.
	case "$country" in
		EU|GB)
			[ -z "$s1g_chzn" ] && s1g_chzn=80211_2020
			;;
	esac

	mm_region=$(persistent_vars_storage.sh READ mm_region 2> /dev/null)
	mm_region_unlocked=$(persistent_vars_storage.sh READ mm_region_unlocked 2> /dev/null)
	[ "$mm_region" = "CE" ] && mm_region="EU"
	if [ "$mm_region_unlocked" != 1 -a -n "$mm_region" -a "$mm_region" != "$country" ]; then
		echo "Cannot set country=$country as device is locked to $mm_region" >&2
		wireless_set_retry 0
		return 1
	fi

	build_mod_params

	# SPI clock is NOT set as a module parameter here. It now comes from the device
	# tree: the mm6108 node under &spi2 in the EAP112 device tree declares
	# spi-max-frequency = <50000000>, which is the highest value mm6108-2.0.1
	# accepts (MAX_SUPPORTED_SPI_CLK_SPEED). An older copy of this file used to force
	# spi_clock_speed=52000000 whenever a country was configured, which was needed
	# only because the EAP112 device tree had no morse node at all. Passing it now
	# would override the device tree and log "Overriding the device tree
	# MaxClockSpeed" on every probe. If bring-up shows a lower clock is
	# needed, change spi-max-frequency in the device tree, not here.

	# The module parameters live in /etc/modules.d, which kmodloader reads at
	# boot, so a normal boot already loads the driver with them and it is only
	# reloaded when one of them changes.
	local morse_module_config_file="/etc/modules.d/morse"
	local morse_test_module_config_file="/etc/modules.d/morse-test-driver"
	local dot11ah_module_config_file="/etc/modules.d/dot11ah"

	if [ -f "$morse_module_config_file" -a -f "$morse_test_module_config_file" ]; then
		echo "Both morse-test-driver and morse in /etc/modules.d; refusing to configure" >&2
		wireless_set_retry 0
		return 1
	elif [ -f "$morse_test_module_config_file" ]; then
		# If test module config file is available, use the test mode driver.
		morse_module_config_file="$morse_test_module_config_file"
	fi

	local inserted_module=0
	# Only reload the driver when a module parameter changed. country is one, so
	# a country or BCF change reloads; a channel change does not.
	# change_module_parameters returns 0 (and rewrites the file) when the
	# parameters differ, 1 when they are unchanged.
	local morse_params_changed=0 dot11ah_params_changed=0
	change_module_parameters morse "$morse_module_config_file" "$MOD_PARAMS" && morse_params_changed=1
	change_module_parameters dot11ah "$dot11ah_module_config_file" "$DOT11AH_MOD_PARAMS" && dot11ah_params_changed=1

	if [ "$morse_params_changed" = 1 ] || [ "$dot11ah_params_changed" = 1 ] || ! is_module_loaded morse; then
		local _old_phy="$(morse_phy_name)"
		is_module_loaded morse && rmmod morse
		is_module_loaded dot11ah && rmmod dot11ah
		insmod dot11ah $DOT11AH_MOD_PARAMS
		_bcf_probe_mark=$(dmesg | wc -l)
		insmod morse $MOD_PARAMS
		inserted_module=1
		# Give the phy time to appear and let 21-morse-bcf-select re-point
		# bcf_default.bin at the region BCF.
		sleep 5
		# Two-stage BCF: this load used the build-time bcf_default.bin (the US
		# file), so a non-US country makes the chip enter failsafe. Reload once so
		# the re-pointed link takes effect. Only lines after $_bcf_probe_mark are
		# examined, so the retry runs at most once.
		if dmesg | tail -n +$((_bcf_probe_mark + 1)) | grep -q "failsafe mode"; then
			rmmod morse
			insmod morse $MOD_PARAMS
			sleep 2
		fi
		morse_restore_phy_name "$_old_phy"
	fi
	# don't do iw reg set as in mac80211; set via modparam

	if ! find_phy; then
		echo "Could not find PHY for device '$1'" >&2
		wireless_set_retry 0
		return 1
	fi

	# Record the chip revision, and free the wlanN the driver already owns so
	# _find_free_ifname can reuse wlan0 instead of drifting on each reinsertion.
	oldifname=$(morse_get_old_ifname "$phy")
	[ -n "$oldifname" ] && echo "oldifname:$oldifname"
	set_chipid $oldifname
	[ -n "$oldifname" ] && iw dev $oldifname del

	json_add_object data
	json_add_string phy "$phy"
	json_close_object

	rm -f /var/run/hostapd-$phy*.conf

	wireless_set_data phy="$phy"

	[ -z "$(uci -q -P /var/state show wireless._${phy})" ] && uci -q -P /var/state set wireless._${phy}=phy

	morse_interface_cleanup ${phy}

	uci -q -P /var/state set wireless._${phy}.umlist=""
	uci -q -P /var/state set wireless._${phy}.aplist=""
	uci -q -P /var/state set wireless._${phy}.splist=""

	set_default rts 1000
	iw phy "$phy" set rts "${rts%%.*}"

	if [ -n "$txpower" ]; then
		iw phy "$phy" set txpower limit "${txpower%%.*}00"
	else
		iw phy "$phy" set txpower auto
	fi


	[ -n "$frag" ] && iw phy "$phy" set frag "${frag%%.*}"


	# Figure out chan info (e.g. freq/bandwidth) from regulatory info,
	# and determine appropriate primary channel index/width if not set.
	has_chan_info=0
	if morse_set_chan_info; then
		has_chan_info=1
	fi

	interface_count=0
	interface_first=
	# This also creates ifnames_$mode for all mentioned modes
	# (space separated 'list' of interface names).
	for_each_interface "ap sta adhoc mesh none monitor" morse_iface_bringup

	# These vars are used to track the interfaces we've brought up.
	# Even though we haven't yet started a service, we set them here
	# in case our script breaks (this means we'll have some chance
	# of correctly cleaning them up).
	uci -q -P /var/state set wireless._${phy}.aplist="${ifnames_ap}"
	uci -q -P /var/state set wireless._${phy}.splist="${ifnames_sta} ${ifnames_mesh} ${ifnames_adhoc}"
	uci -q -P /var/state set wireless._${phy}.umlist="${ifnames_none} ${ifnames_monitor}"

	if [ -n "$ifnames_ap" ]; then
		local hostapd_conf_file="/var/run/hostapd-$phy.conf"
		morse_hostapd_conf_setup "$phy" "$hostapd_conf_file"

		# Because MBSSID IEs are not supported, we provide hostapd
		# with multiple config files so it brings up entirely separate
		# APs (with separate beacons). However, each config file has the
		# "same" set of basic info (see morse_hostapd_conf_setup) to avoid
		# confusion.
		hostapd_conf_files=
		for_each_interface "ap" morse_setup_ap "$hostapd_conf_file"
		# hostapd_s1g is run from "$hostapd_conf_files" (one config per BSS).
		# Overwrite the device-level template with the PRIMARY BSS config so that
		# /var/run/hostapd-<phy>.conf is a full config, matching what mac80211 leaves
		# behind (hostapd-phy0.conf). iwinfo reads exactly that path via
		# nl80211_hostapd_query, which the dot11ah backend uses for ssid and
		# encryption; with only the template (no ssid=) or no file at all,
		# "iwinfo wlan2" reported "ESSID: unknown / Encryption: unknown" while the AP
		# was actually up (hostapd_cli_s1g showed state=ENABLED).
		# The 1.1x handler fed a single hostapd-<phy>.conf to hostapd_s1g, so iwinfo worked there;
		# upstream 2.0.1 split it per BSS and deleted the template.
		# NOTE: only the primary BSS is represented here. Additional HaLow APs on the
		# same phy keep their own hostapd-<phy>-<ifname>.conf and iwinfo will not show
		# their ssid - EAP112 ships a single HaLow AP, so that is not exercised.
		cp "${hostapd_conf_files%% *}" "$hostapd_conf_file"
		morse_run_hostapd
	fi

	if [ -n "$ifnames_sta" ]; then
		get_matter_config
		get_sgi_config
		json_select config
		json_get_vars vendor_keep_alive_offload matter_enable
		json_select ..

		for_each_interface "sta" morse_setup_sta
	fi

	if [ -n "$ifnames_mesh" ]; then
		get_mesh11sd_config
		get_sgi_config
		json_select config
		json_get_vars mesh_max_peer_links mesh_plink_timeout mesh_hwmp_rootmode mesh_gate_announcements mesh_fwding mesh_rssi_threshold mbca_config mbca_min_beacon_gap_ms mbca_tbtt_adj_interval_sec mesh_beacon_timing_report_int mbss_start_scan_duration_ms mesh_beacon_less_mode mesh_dynamic_peering mesh_rssi_margin mesh_blacklist_timeout
		json_select ..

		for_each_interface "mesh" morse_setup_mesh
	fi

	for_each_interface "adhoc" morse_setup_adhoc
	for_each_interface "monitor" morse_setup_monitor
	for_each_interface "none" morse_setup_none

	# Ideally, this would also be in the hostapd/wpa_supplicant config,
	# but for now they don't have support so we use morse_cli.
	set_default ampdu 1

	# There will only be an ifname if at least one interface is brought up.
	# If no interfaces, it doesn't matter if we don't set these
	# (since they won't be used).
	if [ -n "$ifnames" ]; then
		ifname="${ifnames%% *}"

		if [ "$ampdu" = 1 ]; then
			morse_cli -i $ifname ampdu enable
		else
			morse_cli -i $ifname ampdu disable
		fi

		# Ignore non-WLAN energy for CCA.
		morsectrl -i $ifname edconfig energy ignore || logger -t morse "edconfig energy ignore failed"

		if [ -n "$forced_listen_interval" ]
		then
			# 802.11ah supports listen intervals beyond 65535 by
			# using the first two bits as a scale factor.
			# We calculate this transformation here to keep the UI/config simple.
			local max_val=16383
			local scale_factor
			local unscaled_interval
			if [ "$forced_listen_interval" -gt $((1000 * $max_val)) ]; then
				scale_factor=3
				unscaled_interval=$(($forced_listen_interval / 10000))
			elif [ "$forced_listen_interval" -gt $((10 * $max_val)) ]; then
				scale_factor=2
				unscaled_interval=$(($forced_listen_interval / 1000))
			elif [ "$forced_listen_interval" -gt $max_val ]; then
				scale_factor=1
				unscaled_interval=$(($forced_listen_interval / 10))
			else
				scale_factor=0
				unscaled_interval="$forced_listen_interval"
			fi

			morse_cli -i $ifname li $unscaled_interval $scale_factor
		fi

		if [ "$chan_test_mode" = "1" ]; then
			json_select config
			json_get_vars chan_test_freq chan_test_bw chan_test_prim_chwidth chan_test_prim_chan_index chan_test_mode
			json_select ..
			morse_set_chan_test_freq "$ifname" "$chan_test_freq" "$chan_test_bw" "$chan_test_prim_chwidth" "$chan_test_prim_chan_index"
		fi
	fi

	wireless_set_up
	morse_service_restart
}

drv_morse_teardown() {
	if json_is_a data object
	then
		json_select data
		json_get_vars phy
		json_select ..
	fi

	if [ -z "$phy" ]; then
		json_select config
		json_get_vars path
		json_select ..
		if [ -z "$path" ]; then
			echo "Could not find phy from data, nor could find device path from device configuration." >&2
			return 1;
		fi
		phy=$(iwinfo nl80211 phyname "path=$path")
		if [ -z "$phy" ]; then
			echo "Could not find phy from device path." >&2
			return 1;
		fi
	fi

	rm -f /var/run/hostapd-$phy*.conf

	morse_interface_cleanup "$phy"
	uci -q -P /var/state revert wireless._${phy}

	#Set mesh11sd to disabled
	uci set mesh11sd.setup.enabled='0'
	uci commit mesh11sd
}

morse_iface_create() {
	local auto_channel_only=0
	if [ "$svt_restricted_mode" != 1 ] && [ "$country" = EU -o "$country" = GB ]; then
		auto_channel_only=1
	fi

	if [ "$interface_count" -ge 2 ]; then
		return 2
	fi

	if [ "$auto_channel" -gt 0 -a "$interface_count" -ge 1 ]; then
		return 7
	fi

	if [ -z "$interface_first" ]; then
		interface_first="$mode"
	else
		case "$mode" in
			sta)
				if [ "$interface_first" != "ap" ]; then
					return 3
				fi
				;;
			ap)
				case "$interface_first" in
					ap|mesh|sta)
						;;
					*)
						return 3
						;;
				esac
				;;
			mesh)
				if [ "$interface_first" != "ap" ]; then
					return 3
				fi
				;;
			*)
				return 3
				;;
		esac
	fi

	case "$mode" in
		ap)
			if [ "$firmware_type" = fullmac ]; then
				return 11
			fi
			if [ "$auto_channel_only" = 1 ]; then
				[ "$auto_channel" = 0 ] && return 8

				if ! service smart_manager info > /dev/null; then
					return 10
				fi

				# If we don't have a DCS config section, it will default to enabled if channel=auto.
				# However, if it is there, is must be enabled.
				if uci get "smart_manager.${device_section}_dcs" > /dev/null 2>&1; then
					if [ "$(uci get smart_manager.${device_section}_dcs.enabled 2> /dev/null)" != 1 ]; then
						return 10
					fi
				fi
			fi

			[ "$has_chan_info" != 1 ] && return 6
			morse_iw_interface_add "$phy" "$ifname" "$macaddr" __ap || return 1
		;;

		sta)
			if [ "$firmware_type" = thin_lmac ]; then
				return 11
			fi
			[ -z "$country" ] && return 5
			[ "$wds" -gt 0 ] && wdsflag="4addr on"
			morse_iw_interface_add "$phy" "$ifname" "$macaddr" managed "$wdsflag" || return 1

			if [ "$wds" -gt 0 ]; then
				iw dev "$ifname" set 4addr on
			else
				iw dev "$ifname" set 4addr off
			fi

			# When power save was disabled by mod param, skip iw powersave.
			if [ "$ps_disabled_by_modparam" -eq 0 ]; then
				# Keep powersave disabled by default for USB mode (APP-3745)
				[ "$is_usb" -eq 1 ] && set_default powersave 0 || set_default powersave 1
				[ "$powersave" -gt 0 ] && powersave="on" || powersave="off"
				iw dev "$ifname" set power_save "$powersave"
			fi
		;;

		mesh)
			if [ "$firmware_type" = thin_lmac -o "$firmware_type" = fullmac ]; then
				return 11
			fi
			[ "$has_chan_info" != 1 ] && return 6
			[ "$auto_channel_only" = 1 ] && return 9
			[ "$auto_channel" -gt 0 ] && return 4

			morse_iw_interface_add "$phy" "$ifname" "$macaddr" mp || return 1
		;;

		adhoc)
			if [ "$firmware_type" = thin_lmac -o "$firmware_type" = fullmac ]; then
				return 11
			fi
			[ "$has_chan_info" != 1 ] && return 6
			[ "$auto_channel_only" = 1 ] && return 9
			[ "$auto_channel" -gt 0 ] && return 4

			morse_iw_interface_add "$phy" "$ifname" "$macaddr" adhoc || return 1
		;;

		monitor)
			if [ "$firmware_type" = thin_lmac -o "$firmware_type" = fullmac ]; then
				return 11
			fi
			[ "$has_chan_info" != 1 ] && return 6
			morse_iw_interface_add "$phy" "$ifname" "$macaddr" monitor || return 1

			# Unlike other modes, there's no wpa_supplicant/hostapd here, so we are
			# responsible for bring-up.
			ip link set "$ifname" up
			# We need morse0 to dump the packets from.
			ip link set morse0 up
		;;

		*)
			morse_iw_interface_add "$phy" "$ifname" "$macaddr" managed || return 1

			# Unlike other modes, there's no wpa_supplicant/hostapd here, so we are
			# responsible for bring-up.
			ip link set $ifname up
		;;
	esac

	interface_count=$((interface_count + 1))
}

morse_iface_bringup() {
	local iface_index=$1
	json_select config
	json_get_vars ifname mode ssid wds powersave macaddr enable wpa_psk_file vlan_file
	set_default wds 0

	[ -z "$ifname" ] && ifname="$(_find_free_ifname "$DEFAULT_MORSE_IFNAME")"

	json_add_string ifname "$ifname"
	json_add_string phy "$phy"

	[ -n "$macaddr" ] || {
		macaddr="$(morse_generate_mac $phy)"
		macidx="$(($macidx + 1))"
	}

	if [ "$chan_test_mode" = "1" ]; then
		case "$mode" in
			ap|sta)
				if [ "$interface_count" -ge 1 ]; then
					echo "wifi-iface $iface_index mode=$mode ignored; chan_test_mode permits a single interface"
					json_select ..
					return
				fi
				;;
			*)
				echo "wifi-iface $iface_index mode=$mode ignored; chan_test_mode supports only ap/sta modes"
				json_select ..
				return
				;;
		esac
	fi

	json_add_string macaddr "$macaddr"

	morse_iface_create
	case "$?" in
		2)
			echo "wifi-iface $iface_index mode=$mode ignored; at most two interfaces are supported"
			;;
		3)
			echo "wifi-iface $iface_index mode=$mode ignored; can't coexist interface with mode=$interface_first"
			;;
		4)
			echo "wifi-iface $iface_index mode=$mode ignored; cannot have channel=auto in this mode on wifi-device"
			;;
		5)
			echo "wifi-iface $iface_index mode=$mode ignored; requires country to be set on wifi-device"
			;;
		6)
			echo "wifi-iface $iface_index mode=$mode ignored; requires valid country/channel setup on wifi-device"
			;;
		7)
			echo "wifi-iface $iface_index mode=$mode ignored; if using channel=auto, only a single interface is supported"
			;;
		8)
			echo "wifi-iface $iface_index mode=$mode ignored; EU/GB must have channel set to auto due to regulatory restrictions"
			;;
		9)
			echo "wifi-iface $iface_index mode=$mode ignored; EU/GB cannot use mesh/adhoc interfaces due to regulatory restrictions"
			;;
		10)
			echo "wifi-iface $iface_index mode=$mode ignored; EU/GB must have smart_manager installed and active for dynamic channel selection (DCS)"
			;;
		11)
			echo "wifi-iface $iface_index mode=$mode ignored; firmware_type cannot be "$firmware_type" for this mode"
			;;
		0)
			# This helps us track if we've managed to successfully create
			# the interface. This means subsequent steps will ignore this interface
			# if _created is not set.
			json_add_string _created 1
			append ifnames_$mode "$ifname"
			append ifnames "$ifname"
			;;
		*)
			echo "wifi-iface $iface_index mode=$mode could not be created"
			;;
	esac

	json_select ..

}

_find_free_ifname()
{
	local prefix=$1
	local idx=0

	while [ -e "/sys/class/net/$prefix$idx" ]
	do
		idx="$(( idx + 1 ))"
	done

	echo "$prefix$idx"
}

# These functions exist as we do not have another way to restart or notify
# services that want to know when the halow interfaces have been restarted.
# The built in OpenWrt hostapd has ubus hooks that use ubus mechanisms to
# notify init/procd.
morse_service_stop() {
	# Only touch the services that are installed.
	local s
	for s in smart_manager dppd; do
		[ -x "/etc/init.d/$s" ] && "/etc/init.d/$s" stop >/dev/null 2>&1
	done
	return 0
}

morse_service_restart() {
	#  The service is restarted instead of started for a specific case: If both
	# wireless and smart_manager have configuration changes, smart_manager may
	# be reloaded after the morse_service_stop has run but before
	# morse_service_restart. There is potential for it to come up too early and
	# communicate with the old hostapd, which is what this function is here to
	# prevent. We still need to reload smart_manager on smart_manager only
	# configuration changes, so we don't disable that trigger. This is not
	# perfect.

	# Only touch the services that are installed.
	local s
	for s in smart_manager dppd; do
		[ -x "/etc/init.d/$s" ] && "/etc/init.d/$s" restart >/dev/null 2>&1
	done
	return 0
}

morse_setup_ap() {
	local hostapd_conf_file=$1
	local iface_index=$2
	json_select config
	json_get_vars _created ifname phy macaddr vif_txpower
	json_select ..

	if [ "$_created" != 1 ]; then
		# This wifi-iface failed earlier in morse_iface_bringup.
		return
	fi

	[ -n "$vif_txpower" ] && iw dev "$ifname" set txpower fixed "${vif_txpower%%.*}00"

	if [ "$iface_index" != 0 ] && [ "$interface_first" = ap ]; then
		morse_hostapd_add_bss "$hostapd_conf_file" "$phy" "$ifname" "$macaddr" secondary
	else
		morse_hostapd_add_bss "$hostapd_conf_file" "$phy" "$ifname" "$macaddr" primary
	fi

	wireless_add_vif "$iface_index" "$ifname"
}

morse_run_hostapd() {
	/usr/sbin/hostapd_s1g -t -B -s ${hostapd_conf_files}

	# prplmesh is looking for /var/morse/hostapd_s1g_multiap.conf as hostapd conf file.
	# So, we add a symlink from the actual conf file for prplmesh.
	if [ "$multi_ap" -gt 0 ]; then
		mkdir -p /var/morse
		ln -sf "${hostapd_conf_files%% *}" /var/morse/hostapd_s1g_multiap.conf
	fi
}

morse_setup_sta() {
	local iface_index=$1

	json_select config
	json_get_vars _created ifname

	if [ "$_created" != 1 ]; then
		# This wifi-iface failed earlier in morse_iface_bringup.
		return
	fi

	morse_wpa_supplicant_add $ifname 1 $matter_enable || failed=1
	json_select ..

	[ -n "$failed" ] || wireless_add_vif "$iface_index" "$ifname"
}

morse_setup_mesh() {
	local iface_index=$1

	json_select config
	json_get_vars _created ifname

	if [ "$_created" != 1 ]; then
		# This wifi-iface failed earlier in morse_iface_bringup.
		json_select ..
		return
	fi

	morse_wpa_supplicant_add $ifname 1 0 || failed=1
	json_select ..

	[ -n "$failed" ] || wireless_add_vif "$iface_index" "$ifname"

	#Set mesh11sd to enabled
	uci set mesh11sd.setup.enabled='1'
	uci commit mesh11sd

}

morse_setup_adhoc() {
	local iface_index=$1

	wireless_vif_parse_encryption

	json_select config
	json_get_vars _created ifname

	if [ "$_created" != 1 ]; then
		# This wifi-iface failed earlier in morse_iface_bringup.
		json_select ..
		return
	fi

	morse_wpa_supplicant_add $ifname 1 0 || failed=1

	json_select ..

	[ -n "$failed" ] || wireless_add_vif "$iface_index" "$ifname"
}

morse_setup_monitor() {
	local iface_index=$1

	json_select config
	json_get_vars _created ifname
	json_select ..

	if [ "$_created" != 1 ]; then
		# This wifi-iface failed earlier in morse_iface_bringup.
		return
	fi

	center_freq=
	_get_regulatory "$country" "$channel" "$s1g_chanbw" "$op_class" "$s1g_chzn"
	if [ $? -ne 0 ]; then
		echo "Couldn't find regulatory data for $country with ch=$channel bw=$s1g_chanbw op=$op_class chzn=$s1g_chzn" >&2
		return
	fi
	# MHz to kHz without the decimal part. awk instead of bc: nothing in the
	# image provides bc.
	center_freq=$(awk -v f="$center_freq" 'BEGIN { printf "%g\n", f * 1000 }')
	morse_cli -i $ifname channel -c $center_freq ${s1g_chanbw:+-o $s1g_chanbw} ${s1g_prim_chwidth:+-p $(( s1g_prim_chwidth + 1 ))} ${s1g_prim_1mhz_chan_index:+-n $s1g_prim_1mhz_chan_index}

	wireless_add_vif "$iface_index" "$ifname"
}

morse_setup_none() {
	local iface_index=$1

	json_select config
	json_get_vars _created ifname
	json_select ..

	if [ "$_created" != 1 ]; then
		# This wifi-iface failed earlier in morse_iface_bringup.
		return
	fi

	wireless_add_vif "$iface_index" "$ifname"

}

morse_vap_cleanup() {
	local service="$1"
	local vaps="$2"

	for wdev in $vaps; do
		[ "$service" != "none" ] && kill_wait $service &> /dev/null
		ip link set dev "$wdev" down 2>/dev/null
		#for monitor mode, we needed to bring up the morse0 interface,
		#so if we are removing a (the) monitor interface, bring down morse0
		[ -n "$(iw dev $wdev info | grep "type monitor")" ] && ip link set dev morse0 down
		iw dev "$wdev" del 2>/dev/null
	done
}

morse_interface_cleanup() {
	local phy="$1"
	morse_service_stop

	# Removing an interface mid-scan wedges the chip: rm_if and the scan's
	# cfg_scan(false) both wait on a firmware command while holding mors->lock,
	# and every command afterwards fails with -110. Abort scans first.
	local _wdev
	for _wdev in $(_list_phy_interfaces "$phy"); do
		iw dev "$_wdev" scan abort >/dev/null 2>&1
	done

	morse_vap_cleanup hostapd_s1g "$(uci -q -P /var/state get wireless._${phy}.aplist)"
	morse_vap_cleanup wpa_supplicant_s1g "$(uci -q -P /var/state get wireless._${phy}.splist)"
	morse_vap_cleanup none "$(uci -q -P /var/state get wireless._${phy}.umlist)"
}

#################################################
#
#      generic s1g helpers
#
#################################################

morse_set_chan_info() {
	center_freq=
	_get_regulatory "$country" "$channel" "$s1g_chanbw" "$op_class" "$s1g_chzn"
	if [ $? -ne 0 ]; then
		echo "Couldn't find regulatory data for $country with ch=$channel bw=$s1g_chanbw op=$op_class chzn=$s1g_chzn" >&2
		return 1
	fi

	json_select config
	json_get_vars s1g_prim_1mhz_chan_index s1g_prim_chwidth

	if [ -n "$s1g_chanbw" ] && [ -n "$s1g_prim_chwidth" ] && [ "$s1g_prim_chwidth" -gt "$s1g_chanbw" ]; then
		s1g_prim_chwidth=
		echo "s1g_prim_chwidth incorrectly set for bw=$s1g_chanbw, using default"
	fi

	if [ -n "$s1g_chanbw" ] && [ -n "$s1g_prim_1mhz_chan_index" ] && [ "$s1g_prim_1mhz_chan_index" -ge "$s1g_chanbw" ]; then
		s1g_prim_1mhz_chan_index=
		echo "s1g_prim_1mhz_chan_index incorrectly set for bw=$s1g_chanbw, using default"
	fi

	#If bw config is empty the chwidth and chanindex are set to defaults.
	#In case of STA, where bw config is empty these configs are omitted and not configured to wpa_supplicant

	if [ -z "$s1g_prim_chwidth" ]; then
		if [ "$s1g_chanbw" = 4 -o "$s1g_chanbw" = 8 ]; then
			s1g_prim_chwidth=2
		else
			s1g_prim_chwidth=1
		fi
	fi

	s1g_prim_chwidth=$(( s1g_prim_chwidth - 1 ))

	set_default s1g_prim_1mhz_chan_index auto
	if [ "$s1g_prim_1mhz_chan_index" = "auto" ]; then
		s1g_prim_1mhz_chan_index=$(( (s1g_chanbw - 1) / 2 ))
	fi

	json_add_string channel "$channel"
	json_add_string freq "$center_freq"
	json_add_string op_class "$op_class"
	json_add_string s1g_prim_1mhz_chan_index "$s1g_prim_1mhz_chan_index"
	json_add_int s1g_prim_chwidth "$s1g_prim_chwidth"

	json_select ..

	return 0
}


#################################################
#
#      hostapd helpers
#
#################################################


morse_hostapd_conf_setup() {
	local phy=$1
	local hostapd_conf_file=$2
	json_select config
	json_get_vars noscan
	json_get_vars s1g_prim_chwidth s1g_prim_1mhz_chan_index op_class dtim_period
	json_get_vars freq
	json_get_values channel_list channels tx_burst

	if json_is_a s1g_capab array
	then
		json_select s1g_capab
		idx=1
		while json_is_a ${idx} string
		do
			json_get_var capab $idx
			[ -z "$s1g_capab" ] && s1g_capab=$capab || s1g_capab="$s1g_capab,$capab"
			idx=$(( idx + 1 ))
		done
		json_select ..
	fi

	#auto_channel preloaded before drv_ called
	[ "$auto_channel" -gt 0 ] && channel=acs_survey

	[ "$auto_channel" -gt 0 ] && json_get_vars acs_exclude_dfs
	[ -n "$acs_exclude_dfs" ] && [ "$acs_exclude_dfs" -gt 0 ] &&
		append base_cfg "acs_exclude_dfs=1" "$N"

	[ "$auto_channel" = 0 ] && [ -z "$channel_list" ] && \
		channel_list="$channel"

	set_default noscan 0

	[ "$noscan" -gt 0 ] && hostapd_noscan=1
	[ "$tx_burst" = 0 ] && tx_burst=

	if [ "$band" = "s1g" ]; then
		append base_cfg "ieee80211ah=1" "$N"

		set_default s1g_capab "[SHORT-GI-ALL]"

	fi

	json_get_vars country_ie doth
	[ -z "$country_ie" ] && json_add_boolean country_ie '0'
	[ -z "$doth" ] && json_add_boolean doth '0'

	hostapd_prepare_device_config "$hostapd_conf_file" nl80211
	cat >> "$hostapd_conf_file" <<EOF
${channel:+channel=$channel}
${channel_list:+chanlist=$channel_list}
${op_class:+op_class=$op_class}
${s1g_capab:+s1g_capab=$s1g_capab}
${s1g_prim_chwidth:+s1g_prim_chwidth=$s1g_prim_chwidth}
${s1g_prim_1mhz_chan_index:+s1g_prim_1mhz_chan_index=$s1g_prim_1mhz_chan_index}
${hostapd_noscan:+noscan=1}
${tx_burst:+tx_queue_data2_burst=$tx_burst}
$base_cfg

# anti-clogging does not work and creates issues in high station count networks (100+)
# This effectively disables it
anti_clogging_threshold=99999

EOF
	json_select ..
}


morse_hostapd_add_bss() {
	local _hostapd_conf_file="$1"
	local _phy="$2"
	local _ifname="$3"
	local _macaddr="$4"
	local _role="$5"

	hostapd_cfg=
	append hostapd_cfg "interface=$_ifname" "$N"

	json_select config

	morse_override_hostapd_set_bss_options hostapd_cfg "$_phy" "$vif" || {
		json_select ..
		return 1
	}
	json_get_vars wds wds_bridge sae_pwe dtim_period max_listen_int start_disabled dpp_configurator_connectivity raw s1g_bss_color

	# Without an explicit wds_bridge, put WDS stations into the bridge of the
	# network this AP belongs to; otherwise their traffic is not forwarded.
	local network_config network_values
	json_get_values network_values network
	network_config=$(echo "$network_values" | cut -d' ' -f1)
	if [ "${wds:-0}" -gt 0 ] && [ -z "$wds_bridge" ]; then
		wds_bridge="${network_config%%[0-9]*}"
	fi

	set_default raw 0
	raw_block=
	if [ "$_role" = primary ]; then
		# RAWs are not supported for non-primary APs.
		[ "$raw" -gt 0 ] && morse_hostapd_add_raw
	fi
	json_select ..

	set_default wds 0
	set_default start_disabled 0
	set_default sae_pwe 1
	# This controls whether DPP is advertised in the beacon. It does _not_ enable DPP,
	# and hostapd's DPP push button support is available regardless (even if a separate
	# dpp configurator like morse_dppd is not running).
	set_default dpp_configurator_connectivity 1

	if [ "$wds" -gt 0 ]; then
		append hostapd_cfg "wds_sta=1" "$N"
		[ -n "$wds_bridge" ] && append hostapd_cfg "wds_bridge=$wds_bridge" "$N"
	fi

	[ "$start_disabled" -eq 1 ] && append hostapd_cfg "start_disabled=1" "$N"

	local hostapd_ifname_conf_file=/var/run/hostapd-$_phy-$_ifname.conf
	cat "$hostapd_conf_file" - > "$hostapd_ifname_conf_file" <<EOF
$hostapd_cfg
bssid=$_macaddr
${dtim_period:+dtim_period=$dtim_period}
${max_listen_int:+max_listen_interval=$max_listen_int}
${sae_pwe:+sae_pwe=$sae_pwe}
${dpp_configurator_connectivity:+dpp_configurator_connectivity=$dpp_configurator_connectivity}
${raw:+raw=$raw}
${s1g_bss_color:+s1g_bss_color=$s1g_bss_color}
$raw_block
EOF

	append hostapd_conf_files "$hostapd_ifname_conf_file"
}

morse_hostapd_add_raw() {
	local enabled start_time_us duration_us slots cross_slot \
		  max_beacon_spread nominal_stas_per_beacon
	#Clear previous raw_block
	raw_block=""
	local max_raw=8 idx=-1
	while [ "$idx" -lt "$max_raw" ]; do
		idx=$(( idx + 1 ))
		json_get_var enabled                    "raw${idx}_enabled"
		# If not enabled, skip this RAW definition
		[ "$enabled" != 1 ] && continue

		json_get_var start_time_us              "raw${idx}_start_time_us"
		json_get_var duration_us                "raw${idx}_duration_us"
		json_get_var slots                      "raw${idx}_slots"
		json_get_var cross_slot                 "raw${idx}_cross_slot"
		json_get_var max_beacon_spread          "raw${idx}_max_beacon_spread"
		json_get_var nominal_stas_per_beacon    "raw${idx}_nominal_stas_per_beacon"
		json_get_var praw_period                "raw${idx}_period"
		json_get_var praw_start_offset          "raw${idx}_start_offset"

		append raw_block "raw={" "$N"
		# RAW index indicates the priority
		[ -n "$idx" ]                     && append raw_block "priority=${idx}" "$N$T"
		[ -n "$enabled" ]                 && append raw_block "enabled=${enabled}" "$N$T"
		[ -n "$start_time_us" ]           && append raw_block "start_time_us=${start_time_us}" "$N$T"
		[ -n "$duration_us" ]             && append raw_block "duration_us=${duration_us}" "$N$T"
		[ -n "$slots" ]                   && append raw_block "slots=${slots}" "$N$T"
		[ -n "$cross_slot" ]              && append raw_block "cross_slot=${cross_slot}" "$N$T"
		[ -n "$max_beacon_spread" ]       && append raw_block "max_beacon_spread=${max_beacon_spread}" "$N$T"
		[ -n "$nominal_stas_per_beacon" ] && append raw_block "nominal_stas_per_beacon=${nominal_stas_per_beacon}" "$N$T"
		[ -n "$praw_period" ]             && append raw_block "praw_period=${praw_period}" "$N$T"
		[ -n "$praw_start_offset" ]       && append raw_block "praw_start_offset=${praw_start_offset}" "$N$T"

		append raw_block "}" "$N"
	done
}

#################################################
#
#      wpa_supplicant helpers
#
#################################################

morse_wpa_supplicant_add() {
	local _ifname=$1
	local _enable=$2
	local matter=$3
	local _save_dir="/etc/morse"
	local _save_file="${_save_dir}/wpa_supplicant-wlan-saved-over-boot.conf"

	if [ "$_enable" = 0 ]; then
		echo "interface is disabled"
		kill_wait wpa_supplicant_s1g &> /dev/null
		ip link set dev "$_ifname" down
		iw dev "$_ifname" del
		return 0
	fi

	wpa_supplicant_prepare_interface "$_ifname" nl80211 || {
		echo "wpa_supplicant_prepare_interface failed."
		iw dev "$_ifname" del
		return 1
	}
	morse_wpa_supplicant_prepare_interface "$_ifname"
	if [ "$mode" = "sta" ]; then
		morse_override_wpa_supplicant_add_network "$_ifname"
	else
		morse_override_wpa_supplicant_add_network "$_ifname" "$freq" "$htmode" "$noscan" "$enable_sgi"
	fi

	_wpa_supplicant_common $_ifname

	# As $_config ends up /var which is symlink to /tmp
	# the HaLow credentials are lost across reboot
	# So save them outside /var
	if [ "$matter" = 1 ]; then
		if ! [ -f "$_save_file" ]; then
			mkdir -p $_save_dir
			cp $_config $_save_file
		fi
		# Upstream passes -u here to expose the D-Bus control interface.
		# In mm6108-2.0.1 the u case is compiled only under
		# CONFIG_CTRL_IFACE_DBUS_NEW, and this tree builds wpa_supplicant_s1g with
		# MORSE_WPA_SUPPLICANT_S1G_DBUS=n, so -u would fall through to
		# "default: usage(); exit" and the matter STA would never start. Re-add it
		# together with MORSE_WPA_SUPPLICANT_S1G_DBUS=y if matter is ever enabled.
		/usr/sbin/wpa_supplicant_s1g -t -D nl80211 -s -i $_ifname -c $_save_file -B
	else
		# Hand wpa_supplicant the bridge this interface belongs to, when that
		# bridge exists, so EAPOL frames reach it on a bridged interface.
		morse_ifname=$_ifname
		morse_network=
		config_load wireless
		config_foreach get_morse_iface wifi-iface
		local _bridge=
		[ -n "$morse_network" ] && [ -d "/sys/class/net/br-${morse_network}/bridge" ] && _bridge="-bbr-${morse_network}"
		/usr/sbin/wpa_supplicant_s1g -t -D nl80211 -s -i $_ifname $_bridge -c $_config -B
	fi

	return 0
}

#################################################
#
#      interface helpers
#
#################################################

find_phy() {
	[ -n "$phy" -a -d /sys/class/ieee80211/$phy ] && return 0

	if [ -n "$path" ]; then
		phy="$(iwinfo nl80211 phyname "path=$path")"
		[ -n "$phy" ] && return 0
	fi

	if [ -n "$macaddr" ]; then
		for phy in $(ls /sys/class/ieee80211 2>/dev/null); do
			grep -i -q "$macaddr" "/sys/class/ieee80211/${phy}/macaddress" && return 0
		done
	fi
	return 1
}

morse_iw_interface_add() {
	local _phy="$1"
	local _ifname="$2"
	local _macaddr="$3"
	local _type="$4"
	local _wdsflag="$5"
	local rc
	local old_ifname

	iw phy "$_phy" interface add "$_ifname" type "$_type" $_wdsflag
	rc="$?"

	if [ "$rc" = 233 ]; then
		# Device might have just been deleted, give the kernel some time to finish cleaning it up
		sleep 1
		echo "retrying..."
		iw phy "$_phy" interface add "$_ifname" type "$_type" $_wdsflag >/dev/null 2>&1
		rc="$?"
	fi

	if [ "$rc" = 233 ]; then
		# Keep matching pre-existing interface
		if [ -d "/sys/class/ieee80211/${_phy}/device/net/${_ifname}" ]; then
			case "$(iw dev $_ifname info | grep "^\ttype" | cut -d' ' -f2- 2>/dev/null)" in
				"AP")
					[ "$_type" = "__ap" ] && rc=0
					;;
				"IBSS")
					[ "$_type" = "adhoc" ] && rc=0
					;;
				"managed")
					[ "$_type" = "managed" ] && rc=0
					;;
				"mesh point")
					[ "$_type" = "mp" ] && rc=0
					;;
				"monitor")
					[ "$_type" = "monitor" ] && rc=0
					;;
			esac
		fi
	fi

	if [ "$rc" = 233 ]; then
		iw dev "$_ifname" del >/dev/null 2>&1
		if [ "$?" = 0 ]; then
			sleep 1
			iw phy "$_phy" interface add "$_ifname" type "$_type" $_wdsflag >/dev/null 2>&1
			rc="$?"
		fi
	fi

	if [ "$rc" != 0 ]; then
		# Device might not support virtual interfaces, so the interface never got deleted in the first place.
		# Check if the interface already exists, and avoid failing in this case.
		[ -d "/sys/class/ieee80211/${_phy}/device/net/${_ifname}" ] && rc=0
	fi

	if [ "$rc" != 0 ]; then
		# Device doesn't support virtual interfaces and may have existing interface other than _ifname.
		old_ifname="$(basename "/sys/class/ieee80211/${_phy}/device/net"/* 2>/dev/null)"
		[ "$old_ifname" ] && ip link set "$old_ifname" name "$_ifname" 1>/dev/null 2>&1
		rc="$?"
	fi

	if [ "$rc" != 0 ]; then
		echo "Failed to create interface $_ifname"
	else
		ip link set dev "$_ifname" address "$macaddr"
	fi
	return $rc
}

morse_get_addr() {
	local phy="$1"
	local idx="$(($2 + 1))"

	head -n $idx /sys/class/ieee80211/${phy}/addresses | tail -n1
}

#this is exactly same as mac80211.sh
morse_generate_mac() {
	local phy="$1"
	local id="${macidx:-0}"

	local ref="$(cat /sys/class/ieee80211/${phy}/macaddress)"
	local mask="$(cat /sys/class/ieee80211/${phy}/address_mask)"

	[ "$mask" = "00:00:00:00:00:00" ] && {
		mask="ff:ff:ff:ff:ff:ff";

		[ "$(wc -l < /sys/class/ieee80211/${phy}/addresses)" -gt $id ] && {
			addr="$(morse_get_addr "$phy" "$id")"
			[ -n "$addr" ] && {
				echo "$addr"
				return
			}
		}
	}

	local oIFS="$IFS"; IFS=":"; set -- $mask; IFS="$oIFS"

	local mask1=$1
	local mask6=$6

	local oIFS="$IFS"; IFS=":"; set -- $ref; IFS="$oIFS"

	macidx=$(($id + 1))
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

add_driver morse
