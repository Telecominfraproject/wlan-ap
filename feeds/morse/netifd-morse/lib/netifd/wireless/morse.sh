#!/bin/sh

# NOTE: do NOT print anything to stdout at handler top level. On 25.12 the
# netifd ucode wireless master loads every /lib/netifd/wireless/*.sh via
# handler_load(), which runs "<handler> '' dump" and parses stdout line by
# line as JSON. Stray non-JSON lines here (the old "Adding device handler"
# / "Configuring" echoes) corrupt that stream and make handler_load spin in
# its `while (!f.error())` loop -> netifd busy-loops (state=R, network dead).
# mac80211.sh keeps its top level output-free for the same reason.

. /lib/netifd/netifd-wireless.sh
. /lib/netifd/hostapd_s1g.sh
. /lib/netifd/morse/morse_overrides.sh
. /lib/netifd/morse/morse_utils.sh

# Capture the script-level command BEFORE init_wireless_driver consumes "$@".
# handler_load() registers this handler via `morse.sh "" dump`, so here the
# 2nd positional arg is "dump". drv_morse_cleanup() (called with no args by the
# framework during that dump path) must use THIS flag to skip its
# side-effecting hostapd_common_cleanup (killall meshd-nl80211), which would
# otherwise corrupt the JSON dump written to handler_load's tmpfd and leave the
# morse handler unregistered (no radio2, no wlan0).
MORSE_HANDLER_CMD="$2"

init_wireless_driver "$@"

MM_MOD_INT="watchdog_interval_secs max_rates max_rate_tries spi_clock_speed max_txq_len virtual_sta_max max_aggregation_count
			default_cmd_timeout_ms sdio_reset_time tx_max_power_mbm max_mc_frames duty_cycle_mode ocs_type fixed_mcs fixed_bw
			fixed_ss fixed_guard tx_status_lifetime_ms max_total_vendor_ie_bytes"
MM_MOD_BOOL="enable_mac80211_connection_monitor mcs10_mode enable_rts_8mhz
			enable_otp_check enable_survey enable_subbands enable_ps enable_trav_pilot enable_watchdog_reset
			enable_watchdog no_hwcrypt enable_raw enable_arp_offload enable_dynamic_ps_offload
			enable_coredump thin_lmac enable_mbssid_ie enable_trav_pilot enable_cts_to_self enable_airtime_fairness
			enable_twt enable_bcn_change_seq_monitor enable_dhcpc_offload enable_ibss_probe_filtering enable_auto_duty_cycle
			enable_auto_mpsw enable_mcast_whitelist log_modparams_on_boot enable_fixed_rate spi_use_edge_irq"
MM_MOD_STRING="bcf serial country test_mode debug_mask macaddr_octet mcs_mask dhcpc_lease_update_script"
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

check_cac(){
	json_select config
	json_get_vars cac
	if [ "${cac:-0}" -gt 0 ]; then
		enable_cac=1
	fi
	json_select ..
}

check_sgi(){
	enable_sgi=1
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
}

build_morse_mod_params(){
	json_select config

	for var in $MM_MOD_BOOL $MM_MOD_INT $MM_MOD_STRING; do
		json_get_var mm_mod_val "$var"
		[ -n "$mm_mod_val" ] && MOD_PARAMS="$MOD_PARAMS $var=$mm_mod_val"
	done

	check_sgi
	if [ $enable_sgi -ne 1 ]; then
		MOD_PARAMS="$MOD_PARAMS enable_sgi_rc=0"

	else
		MOD_PARAMS="$MOD_PARAMS enable_sgi_rc=1"
	fi
	json_select ..
	enable_cac=
	for_each_interface "ap" check_cac
	[ -n "$enable_cac" ] && MOD_PARAMS="$MOD_PARAMS enable_cac=$enable_cac"

	# Get the last three octets of the eth0 MAC address
	# to use as the default HaLow MAC address
	local ETH0_MAC_SUFFIX=`cat /sys/class/net/eth0/address | cut -d: -f4-`

	MOD_PARAMS="$MOD_PARAMS macaddr_suffix=$ETH0_MAC_SUFFIX"

	MOD_PARAMS=`echo $MOD_PARAMS | xargs`
}

drv_morse_cleanup() {
	# CRITICAL: skip the real cleanup during the "dump" phase.
	#
	# 25.12 handler_load() registers each wireless handler by running
	# "./morse.sh '' dump >&<tmpfd>" and parsing that fd as the JSON dump.
	# The framework's init_wireless_driver() invokes drv_<name>_cleanup as
	# part of that dump path. hostapd_common_cleanup runs `killall
	# meshd-nl80211` (and touches processes/fds); doing that while the dump
	# is being written to the inherited tmpfd corrupts the stream so
	# handler_load reads 0 objects -> the morse handler is NEVER registered
	# -> radio2 never enters netifd (no wlan0). mac80211's cleanup does no
	# such thing, which is why only morse broke.
	#
	# During dump the script was invoked as `morse.sh "" dump`, captured in
	# MORSE_HANDLER_CMD at the top level (the framework calls this function
	# with no positional args, so we cannot rely on $2 here). Skip the
	# side-effecting cleanup in that case; only run it for genuine
	# teardown/cleanup calls.
	[ "$MORSE_HANDLER_CMD" = "dump" ] && return 0
	hostapd_common_cleanup
}

drv_morse_init_device_config() {
	hostapd_common_add_device_config

	config_add_string path phy 'macaddr:macaddr'
	config_add_string tx_burst
	config_add_int frag rts
	config_add_int op_class
	config_add_int txpower
	config_add_int s1g_prim_chwidth
	config_add_string s1g_prim_1mhz_chan_index
	config_add_int bss_color
	config_add_boolean ampdu
	config_add_int forced_listen_interval
	config_add_boolean noscan
	config_add_array s1g_capab
	config_add_array channels
	config_add_boolean vendor_keep_alive_offload

	#module parameters
	config_add_int $MM_MOD_INT
	config_add_boolean $MM_MOD_BOOL
	config_add_string $MM_MOD_STRING $MM_MOD_UNKNOWN
}


drv_morse_init_iface_config() {
	hostapd_common_add_bss_config
	config_add_string 'macaddr:macaddr' ifname
	config_add_boolean wds powersave enable
	config_add_boolean wps_virtual_push_button
	config_add_array sae_group
	config_add_array owe_group
	config_add_int maxassoc
	config_add_int max_listen_int
	config_add_int dtim_period
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

	#raw
	config_add_int raw_sta_priority
	config_add_array raws

	# mesh
	config_add_string mesh_id

	#dpp
	config_add_boolean dpp

	#beacon interval
	config_add_int beacon_int
}

# The 25.12 netifd-wireless.sh framework's init_wireless_driver() calls
# drv_<name>_init_{device,iface,vlan,station}_config during the "dump" phase.
# morse only defined device/iface; the missing vlan/station callbacks made the
# dump eval hit "drv_morse_init_vlan_config: not found" and abort mid-way, so
# the handler's JSON dump was truncated. handler_load() then read 0 objects and
# the morse wireless handler was NEVER registered -> radio2 never entered
# netifd's wdev state machine (network.wireless up radio2 = "Not found", no
# wlan0). Define the two callbacks (mirroring mac80211) so the dump completes
# and morse registers as a wireless handler.
drv_morse_init_vlan_config() {
	config_add_string name
	config_add_int vid
}

drv_morse_init_station_config() {
	config_add_string ifname
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

is_module_loaded() {
	lsmod | grep -q '^morse '
}

change_module_parameters() {
	# These are parameters that we use morse_cli to configure,
	# but because we have no way to revert back to the original
	# state any change requires us to reload the module.
	#
	# Therefore we store these as a comment in /etc/modules.d/morse
	# (and changing this comment will mean that we will decide
	# to reload the module; see use of cmp below).
	local morse_cli_params="bss_color=$bss_color forced_listen_interval=$forced_listen_interval"
	local proposed_module="$(mktemp)"
	cat > "$proposed_module" <<-MORSE
	# Morse module, with subsequent morse_cli commands: $morse_cli_params
	morse $MOD_PARAMS
	MORSE

	if cmp -s "$proposed_module" /etc/modules.d/morse; then
		# Parameters didn't change; do nothing.
		rm "$proposed_module"
		return 1
	else
		mv "$proposed_module" /etc/modules.d/morse
		return 0
	fi
}

drv_morse_setup() {
	morse_band_override
	json_select config
	json_get_vars \
		phy macaddr path \
		country \
		txpower \
		frag rts htmode \
		ampdu \
		op_class \
		bss_color forced_listen_interval
	json_get_values basic_rate_list basic_rate
	json_select ..

	MOD_PARAMS=
	build_morse_mod_params

	# The morse driver's regulatory country is a module parameter, fixed at
	# insertion time (boot loads it with the default country=US). When the
	# cloud/uci config selects another country (e.g. JP), the driver keeps
	# emitting "Regulatory domain JP is not consistent with loaded country
	# code US" and repeatedly runs morse_mac_restart. build_morse_mod_params
	# has folded the desired country into MOD_PARAMS, so reload the module
	# with the new parameters whenever they changed (or it isn't loaded yet).
	#
	# Deliberately do NOT issue a global "iw reg set" here: the mt7996
	# 2.4G/5G radios are not self-managed and share the single global
	# cfg80211 regulatory domain, so a global reg set for the HaLow country
	# would be picked up by mt7996's reg_notifier and clobber 2.4G/5G. The
	# per-phy S1G regulatory is handled in morse_set_ap_regulatory instead.
	if [ -n "$country" ]; then
		if change_module_parameters || ! is_module_loaded; then
			is_module_loaded && rmmod morse
			/sbin/kmodloader /etc/modules.d/morse
		fi
	fi

	local retries=4
	while ! find_phy; do
		sleep 0.5
		retries="$((retries - 1))"
		if [ "$retries" -le 0 ]; then
			echo "Could not find PHY for device '$1'" >&2
			wireless_set_retry 0
			return 1
		fi
	done

	# wlan? is automatically created on module insertion, which usually
	# happens in two situations: boot, and a module load above.
	auto_ifname=$(morse_get_ifname "$phy")
	if [ $auto_ifname ]; then
		# EdgeCore: after a country-change module reload the driver kicks
		# off a firmware restart (morse_mac_restart). The phy node appears
		# before that restart has settled, so starting hostapd_s1g too early
		# races the driver's nl80211 (re-)registration and fails with
		# "nl80211: kernel reports: Match already configured", after which
		# bring-up only recovers on the next netifd reconf retry (~1-2 min).
		# Gate on firmware readiness: morse_cli hw_version fails until the
		# firmware is back up, so poll it before proceeding.
		local _ready_retries=40
		while ! morse_cli -i "$auto_ifname" hw_version >/dev/null 2>&1; do
			sleep 0.25
			_ready_retries="$((_ready_retries - 1))"
			[ "$_ready_retries" -le 0 ] && break
		done

		# As a happy byproduct of the bonus wlan?, we can interact
		# without module to determine the MAC and chip id.
		# This is an ugly place to do this, since we really only
		# need to do it once.
		# Note that this currently guaranteed to happen on each boot
		# (since we'll have a wlan? then).
		set_chipid $auto_ifname
		update_dpp_qrcode /etc/dpp_key.pem "$(cat /sys/class/ieee80211/$phy/macaddress)"

		# Now remove wlan?, since any wlan* interfaces we want will be
		# created by the wifi-iface sections in the uci config.
		iw dev "$auto_ifname" del
	fi

	json_add_object data
	json_add_string phy "$phy"
	json_close_object

	local hostapd_conf_file="/var/run/hostapd-$phy.conf"
	rm -f "$hostapd_conf_file"

	wireless_set_data phy="$phy"

	[ -z "$(uci -q -P /var/state show wireless._${phy})" ] && uci -q -P /var/state set wireless._${phy}=phy

	morse_interface_cleanup ${phy}

	set_default rts 1000
	iw phy "$phy" set rts "${rts%%.*}"

	[ -n "$frag" ] && iw phy "$phy" set frag "${frag%%.*}"


	already_have_wpa_supplicant_running=
	already_have_hostapd_running=
	has_ap=
	has_sta=
	has_mesh=
	has_adhoc=

	#bring the interfaces up
	for_each_interface "ap sta adhoc mesh none" morse_iface_bringup

	# setup the 11ah specific regulatory translation
	# and setup the general s1g device defaults as common configs for all interfaces
	morse_set_ap_regulatory
	morse_setup_s1g_device_defaults

	[ -n "$has_ap" ] && {
		morse_hostapd_conf_setup "$phy"
	}
	for_each_interface "ap" morse_setup_ap

	[ -n "$has_sta" ] && {
		json_select config
		json_get_vars vendor_keep_alive_offload
		json_select ..
	}
	for_each_interface "sta" morse_setup_sta

	[ -n "$has_mesh" ] && {
		get_mesh11sd_config
		json_select config
		json_get_vars mesh_max_peer_links mesh_plink_timeout mesh_hwmp_rootmode mesh_gate_announcements mesh_fwding mesh_rssi_threshold mbca_config mbca_min_beacon_gap_ms mbca_tbtt_adj_interval_sec mesh_beacon_timing_report_int mbss_start_scan_duration_ms mesh_beacon_less_mode mesh_dynamic_peering mesh_rssi_margin mesh_blacklist_timeout
		json_select ..
	}
	for_each_interface "mesh" morse_setup_mesh

	[ -n "$has_adhoc" ] && {
		json_select config
		json_get_vars op_class channel country s1g_prim_chwidth s1g_prim_1mhz_chan_index
		json_select ..

	}
	for_each_interface "adhoc" morse_setup_adhoc

	# Ideally, this would also be in the hostapd/wpa_supplicant config,
	# but for now they don't have support so we use morse_cli.
	set_default ampdu 1

	# There will only be an ifname if at least one interface is brought up.
	# If no interfaces, it doesn't matter if we don't set these
	# (since they won't be used).
	if [ -n "$ifname" ]; then
		morse_cli -i $ifname ampdu $ampdu
		[ -n "$bss_color" ] && morse_cli -i $ifname bsscolor $bss_color
	fi

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
			unscaled_interval=$(("$forced_listen_interval" / 10000))
		elif [ "$forced_listen_interval" -gt $((10 * $max_val)) ]; then
			scale_factor=2
			unscaled_interval=$(("$forced_listen_interval" / 1000))
		elif [ "$forced_listen_interval" -gt $max_val ]; then
			scale_factor=1
			unscaled_interval=$(("$forced_listen_interval" / 1000))
		else
			scale_factor=0
			unscaled_interval="$forced_listen_interval"
		fi

		morse_cli -i $ifname li $unscaled_interval $scale_factor
	fi

	# Seed the nl80211 survey so cloud/iwinfo telemetry has channel-survey
	# data from boot. Unlike the mt7996 (mac80211) 2.4G/5G radios - which
	# accumulate operating-channel survey continuously and therefore already
	# have a survey dump right after bring-up - the morse S1G firmware only
	# populates the survey table AFTER an explicit scan (per Morse support:
	# "run iw dev <if> scan first, then the survey dump has data"). Without
	# this the HaLow radio shows no survey at boot while 2.4G/5G do.
	#
	# Kick a single background scan once the interface is up. Done detached so
	# it never blocks/fails bring-up, and only once here (netifd re-runs setup
	# on reconf, which refreshes it). Harmless on AP/mesh: it is a brief
	# off-channel scan; failures (busy/unsupported) are ignored.
	[ -n "$ifname" ] && morse_survey_seed_scan "$ifname"

	wireless_set_up
}

# One-shot, detached survey seed scan (see call site in drv_morse_setup).
morse_survey_seed_scan() {
	local _if="$1"
	(
		# Wait briefly for the netdev to be operationally up so the scan
		# request is accepted, then trigger a single scan. morse populates
		# its survey table off the back of this.
		local _tries=20
		while [ "$_tries" -gt 0 ]; do
			[ -d "/sys/class/net/${_if}" ] && \
				[ "$(cat /sys/class/net/${_if}/operstate 2>/dev/null)" != "down" ] && break
			_tries=$((_tries - 1))
			sleep 0.5
		done
		# A single scan is enough to make `iw dev <if> survey dump` return
		# data. Ignore errors (e.g. transient EBUSY); the next reconf retries.
		iw dev "${_if}" scan >/dev/null 2>&1 || \
			iw dev "${_if}" scan passive >/dev/null 2>&1
	) >/dev/null 2>&1 &
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

	#remove hostapd conffile before tearing down.
	local hostapd_conf_file="/var/run/hostapd-$phy.conf"
	rm "$hostapd_conf_file" -f

	morse_interface_cleanup "$phy"
	uci -q -P /var/state revert wireless._${phy}

	#Set mesh11sd to disabled
	uci set mesh11sd.setup.enabled='0'
	uci commit mesh11sd
}

morse_iface_bringup() {
	json_select config
	json_get_vars ifname mode ssid wds powersave macaddr enable wpa_psk_file vlan_file

	# guard against more than one AP interface
	if [ -n "$has_ap" -a "$mode" = "ap" ]; then
		echo "Can't have more than one AP interface."
		json_select ..
		return
	fi
	# guard against more than one STA interface
	if [ -n "$has_sta" -a "$mode" = "sta" ]; then
		echo "Can't have more than one STA interface."
		json_select ..
		return
	fi

	# guard against more than one mesh interface
	if [ -n "$has_mesh" -a "$mode" = "mesh" ]; then
		echo "Can't have more than one MESH interface."
		json_select ..
		return
	fi

	set_default wds 0

	# Name the HaLow netdev after the mt7996 (mac80211) convention instead of
	# the generic "wlanN": <phy>-<mode><idx>, e.g. phy3-ap0 / phy3-sta0 /
	# phy3-mesh0. $phy is the morse phy resolved by find_phy() (NOT hardcoded -
	# the HaLow dongle may enumerate as any phyN), so the name tracks the real
	# phy. Only auto-name when the config didn't pin an ifname; a cloud/uci
	# supplied ifname is still honoured (mirrors mac80211_prepare_vif).
	[ -z "$ifname" ] && ifname="$(morse_set_ifname "$phy" "$mode")"

	json_add_string ifname "$ifname"
	json_add_string phy "$phy"


	[ -n "$macaddr" ] || {
		macaddr="$(morse_generate_mac $phy)"
		macidx="$(($macidx + 1))"
	}

	json_add_string macaddr "$macaddr"
	json_select ..

	case "$mode" in
		ap)
			has_ap=1
			morse_iw_interface_add "$phy" "$ifname" __ap
			if [ $? -ne 0 ]; then
				echo "morse_iface_bringup: error adding interface $ifname to $phy" >&2
				exit 1
			fi
			ifconfig "$ifname" hw ether $macaddr
			ip link set $ifname up
		;;

		sta)
			has_sta=1
			[ "$wds" -gt 0 ] && wdsflag="4addr on"
			morse_iw_interface_add "$phy" "$ifname" managed "$wdsflag" || return
			if [ "$wds" -gt 0 ]; then
				iw dev "$ifname" set 4addr on
			else
				iw dev "$ifname" set 4addr off
			fi

			set_default powersave 1
			[ "$powersave" -gt 0 ] && powersave="on" || powersave="off"
			iw dev "$ifname" set power_save "$powersave"
			ifconfig "$ifname" hw ether $macaddr
			ip link set $ifname up
		;;

		mesh)
			has_mesh=1
			morse_iw_interface_add "$phy" "$ifname" mp
			ifconfig "$ifname" hw ether $macaddr
			ip link set $ifname up
		;;

		adhoc)
			has_adhoc=1
			morse_iw_interface_add "$phy" "$ifname" adhoc
		;;

		*)
			morse_iw_interface_add "$phy" "$ifname" managed || return
			ip link set $ifname up
		;;
	esac

}

morse_get_ifname()
{
	local _phy=$1
	local _oldifname="$(basename "/sys/class/ieee80211/${_phy}/device/net"/* 2>/dev/null)"

	if [[ "$_oldifname" == "wlan"* ]]; then
		echo "$_oldifname"
	else
		echo ""
	fi
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

# Build a HaLow netdev name the mt7996/mac80211 way: <phy>-<mode><idx>.
# mac80211 uses "$phy$suffix-<mode><idx>" (e.g. phy1.1-ap0); morse has a single
# S1G band so there is no ".N" band suffix -> phy3-ap0 / phy3-sta0 / phy3-mesh0.
# The mode maps sta/adhoc the same way mac80211 does (adhoc -> ibss). phy is
# passed in (dynamic, whatever phyN the dongle enumerated as). Picks the first
# free index so multiple vifs of the same mode don't collide.
morse_set_ifname()
{
	local _phy="$1"
	local _mode="$2"
	local _p

	case "$_mode" in
		ap)      _p=ap ;;
		sta)     _p=sta ;;
		mesh)    _p=mesh ;;
		adhoc)   _p=ibss ;;
		monitor) _p=mon ;;
		*)       _p=ap ;;
	esac

	_find_free_ifname "${_phy}-${_p}"
}

morse_setup_ap() {
	local iface_index=$1
	json_select config
	json_get_vars ifname phy mode ssid wds powersave macaddr enable wpa_psk_file vlan_file multi_ap key encryption
	json_select ..

	# guard against more than one hostapd_s1g instance
	if [ -n "$already_have_hostapd_running" ]; then
		echo "Can't have more than one hostapd_s1g running."
		return
	fi

	local hostapd_ctrl="${hostapd_ctrl:-/var/run/hostapd/$ifname}"
	local type=interface

	morse_hostapd_add_bss "$phy" "$ifname" "$macaddr" "$type"

	json_get_vars mode
	json_get_var vif_txpower

	uci -q -P /var/state set wireless._${phy}.aplist="${ifname}"

	/sbin/hostapd_s1g -t -B -s ${hostapd_conf_file}
	# prplmesh is looking for /var/morse/hostapd_s1g_multiap.conf as hostapd conf file.
	# So, we add a symlink from the actual conf file for prplmesh.
	if [ "$multi_ap" -gt 0 ]; then
		mkdir -p /var/morse
		rm /var/morse/hostapd_s1g_multiap.conf
		ln -s ${hostapd_conf_file} /var/morse/hostapd_s1g_multiap.conf
	fi

	#mark that we have already started the hostapd_s1g
	already_have_hostapd_running=1

	[ -z "$vif_txpower" ] || iw dev "$ifname" set txpower fixed "${vif_txpower%%.*}00"

	wireless_add_vif "$iface_index" "$ifname"
}

morse_set_ap_regulatory() {
	halow_bw=
	center_freq=
	if [ -n "$has_ap" ] ||  [ -n "$has_mesh" ] || [ -n "$has_adhoc" ]; then
		_get_regulatory "$mode" "$country" "$channel" "$op_class"
		if [ $? -ne 0 ]; then
			echo "Couldn't find reg for $mode in $country with ch=$channel op=$op_class" >&2
			return
		fi

		#add ap radio settings to the ap interface configs to be used when bringing hostapd_s1g up.
		json_select config
		json_add_int bw "$halow_bw"
		json_add_string freq "$center_freq"
		json_add_string op_class "$op_class"
		json_select ..
	fi
}

morse_setup_sta() {
	local iface_index=$1

	# guard against more than one wpa_supplicant_s1g instance
	if [ -n "$already_have_wpa_supplicant_running" ]; then
		echo "Can't have more than one wpa_supplicant_s1g running."
		return
	fi

	json_select config
	json_get_vars ifname

	morse_wpa_supplicant_add $ifname 1 || failed=1
	#mark that we have already started the wpa_supp_s1g
	already_have_wpa_supplicant_running=1
	json_select ..

	[ -n "$failed" ] || wireless_add_vif "$iface_index" "$ifname"
	uci -q -P /var/state set wireless._${phy}.splist="${ifname}"
	uci -q -P /var/state set wireless._${phy}.umlist="${ifname}"
}

morse_setup_mesh() {
	local iface_index=$1

	# guard against more than one wpa_supplicant_s1g instance
	if [ -n "$already_have_wpa_supplicant_running" ]; then
		echo "Can't have more than one wpa_supplicant_s1g running."
		return
	fi

	json_select config
	json_get_vars ifname

	morse_wpa_supplicant_add $ifname 1 || failed=1
	#mark that we have already started the wpa_supp_s1g
	already_have_wpa_supplicant_running=1
	json_select ..

	[ -n "$failed" ] || wireless_add_vif "$iface_index" "$ifname"
	uci -q -P /var/state set wireless._${phy}.splist="${ifname}"
	uci -q -P /var/state set wireless._${phy}.umlist="${ifname}"

	#Set mesh11sd to enabled
	uci set mesh11sd.setup.enabled='1'
	uci commit mesh11sd

}

morse_setup_adhoc() {
	local iface_index=$1

	wireless_vif_parse_encryption
	# guard against more than one wpa_supplicant_s1g instance
	if [ -n "$already_have_wpa_supplicant_running" ]; then
		echo "Can't have more than one wpa_supplicant_s1g running."
		return
	fi

	json_select config
	json_get_vars ifname

	morse_wpa_supplicant_add $ifname 1 || failed=1
	#mark that we have already started the wpa_supp_s1g
	already_have_wpa_supplicant_running=1
	json_select ..

	[ -n "$failed" ] || wireless_add_vif "$iface_index" "$ifname"
	uci -q -P /var/state set wireless._${phy}.splist="${ifname}"
	uci -q -P /var/state set wireless._${phy}.umlist="${ifname}"
}

morse_vap_cleanup() {
	local service="$1"
	local vaps="$2"

	for wdev in $vaps; do
		[ "$service" != "none" ] && kill_wait $service &> /dev/null
		ip link set dev "$wdev" down 2>/dev/null
		iw dev "$wdev" del
	done
}

morse_interface_cleanup() {
	local phy="$1"

	morse_vap_cleanup hostapd_s1g "$(uci -q -P /var/state get wireless._${phy}.aplist)"
	morse_vap_cleanup wpa_supplicant_s1g "$(uci -q -P /var/state get wireless._${phy}.splist)"
	morse_vap_cleanup none "$(uci -q -P /var/state get wireless._${phy}.umlist)"
}

#################################################
#
#      generic s1g helpers
#
#################################################

morse_setup_s1g_device_defaults() {
	json_select config
	json_get_vars s1g_prim_1mhz_chan_index s1g_prim_chwidth bw

	if [ -n "$bw" ] && [ -n "$s1g_prim_chwidth" ] && [ "$s1g_prim_chwidth" -gt "$bw" ]; then
		s1g_prim_chwidth=
		echo "s1g_prim_chwidth incorrectly set for bw=$bw, using default"
	fi

	if [ -n "$bw" ] && [ -n "$s1g_prim_1mhz_chan_index" ] && [ "$s1g_prim_1mhz_chan_index" -ge "$bw" ]; then
		s1g_prim_1mhz_chan_index=
		echo "s1g_prim_1mhz_chan_index incorrectly set for bw=$bw, using default"
	fi

	#If bw config is empty the chwidth and chanindex are set to defaults.
	#In case of STA, where bw config is empty these configs are omitted and not configured to wpa_supplicant

	if [ -z "$s1g_prim_chwidth" ]; then
		if [ ! -z $bw ] && ([ $bw -eq 4 ] || [ $bw -eq 8 ]); then
			s1g_prim_chwidth=2
		else
			s1g_prim_chwidth=1
		fi
	fi

	set_default s1g_prim_1mhz_chan_index auto
	if [ "$s1g_prim_1mhz_chan_index" = "auto" ]; then
		if [ ! -z $bw ] && [ $bw -eq 8 ]; then
			s1g_prim_1mhz_chan_index=3
		elif [ ! -z $bw ] && [ $bw -eq 4 ]; then
			if [ "$s1g_prim_chwidth" -eq 2 ]; then
				s1g_prim_1mhz_chan_index=2
			else
				s1g_prim_1mhz_chan_index=1
			fi
		else
			s1g_prim_1mhz_chan_index=0
		fi
	fi

	s1g_prim_chwidth=$(( $s1g_prim_chwidth - 1 ))

	json_add_string s1g_prim_1mhz_chan_index "$s1g_prim_1mhz_chan_index"
	json_add_int s1g_prim_chwidth "$s1g_prim_chwidth"

	json_select ..
}


#################################################
#
#      hostapd helpers
#
#################################################


morse_hostapd_conf_setup() {
	local phy=$1
	json_select config
	json_get_vars noscan
	json_get_vars s1g_prim_chwidth s1g_prim_1mhz_chan_index op_class dtim_period
	json_get_vars bw freq
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

EOF
	json_select ..
}


morse_hostapd_add_bss(){
	local _phy="$1"
	local _ifname="$2"
	local _macaddr="$3"
	local _type="$4"

	hostapd_cfg=
	append hostapd_cfg "# Interface $_ifname "
	append hostapd_cfg "$_type=$_ifname" "$N"

	json_select config
	morse_override_hostapd_set_bss_options hostapd_cfg "$_phy" "$vif" || return 1
	json_get_vars wds wds_bridge sae_pwe dtim_period max_listen_int start_disabled

	local network_config network_values
	json_get_values network_values network
	network_config=$(echo "$network_values" | cut -d' ' -f1)

	if [ "$wds" -gt 0 ] && [ -z "$wds_bridge" ]; then
		wds_bridge="${network_config%%[0-9]*}"
	fi

	raw_block=
	json_for_each_item morse_hostapd_add_raw raws
	json_select ..

	set_default wds 0
	set_default start_disabled 0
	set_default sae_pwe 1

	if [ "$wds" -gt 0 ]; then
		append hostapd_cfg "wds_sta=1" "$N"
		[ -n "$wds_bridge" ] && append hostapd_cfg "wds_bridge=$wds_bridge" "$N"
	fi

	[ "$start_disabled" -eq 1 ] && append hostapd_cfg "start_disabled=1" "$N"

		cat >> /var/run/hostapd-$_phy.conf <<EOF
$hostapd_cfg
bssid=$_macaddr
${dtim_period:+dtim_period=$dtim_period}
${max_listen_int:+max_listen_interval=$max_listen_int}
${sae_pwe:+sae_pwe=$sae_pwe}
$raw_block
EOF
}

morse_hostapd_add_raw(){
	local cfgtype priority enabled start_time_us duration_us slots cross_slot max_beacon_spread nominal_stas_per_beacon
	local T="	"
	config_load wireless
	config_get cfgtype "$1" TYPE
	[ "$cfgtype" != "raw" ] && return

	config_get priority "$1" priority
	config_get enabled "$1" enabled
	config_get start_time_us "$1" start_time_us
	config_get duration_us "$1" duration_us
	config_get slots "$1" slots
	config_get cross_slot "$1" cross_slot
	config_get max_beacon_spread "$1" max_beacon_spread
	config_get nominal_stas_per_beacon "$1" nominal_stas_per_beacon

	append raw_block "raw={" "$N"
	append raw_block "priority=${priority:=0}" "$N$T"
	append raw_block "enabled=${enabled:=0}" "$N$T"
	append raw_block "${start_time_us:+start_time_us=$start_time_us}" "$N$T"
	append raw_block "${duration_us:+duration_us=$duration_us}" "$N$T"
	append raw_block "${slots:+slots=$slots}" "$N$T"
	append raw_block "cross_slot=${cross_slot:=false}" "$N$T"
	append raw_block "${max_beacon_spread:+max_beacon_spread=$max_beacon_spread}" "$N$T"
	append raw_block "${nominal_stas_per_beacon:+nominal_stas_per_beacon=$nominal_stas_per_beacon}" "$N$T"
	append raw_block "}" "$N"
}

#################################################
#
#      wpa_supplicant helpers
#
#################################################

morse_wpa_supplicant_add() {
	local _ifname=$1
	local _enable=$2

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
	#need to handle bridge mode??
	/sbin/wpa_supplicant_s1g -t -D nl80211 -s -i $_ifname -c $_config -B

	#React to DPP events (wpa_s1g_dpp_action will persist creds and restart network)
	[ "$dpp" = 1 ] && /usr/sbin/wpa_event_listener -a "/lib/netifd/morse/wpa_s1g_dpp_action.sh" -B
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
	local _type="$3"
	local _wdsflag="$4"
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

	[ "$rc" != 0 ] && echo "Failed to create interface $_ifname"
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

set_chipid() {
	local _ifname=$1
	local state
	state="$(cat /sys/class/net/${_ifname}/operstate 2>/dev/null)"
	[ $? -ne 0 ] && return

	if [ "$state" == "down" ]; then
		ip link set ${_ifname} up
		[ $? -ne 0 ] && return
	fi
	local chip_revision
	chip_revision="$(morse_cli -i ${_ifname} hw_version 2>/dev/null)"
	[ $? -ne 0 ] && return
	chip_revision=${chip_revision##"HW Version: "}

	uci set system.@system[0].notes="${chip_revision}"
	uci commit system

	if [ "$state" == "down" ]; then
		ip link set ${_ifname} down
	fi
}

add_driver morse
