#!/bin/sh
#
# Copyright (c) 2021, The Linux Foundation. All rights reserved.
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
[ -e /lib/ipq806x.sh ] && . /lib/ipq806x.sh
. /lib/functions.sh

type ipqxxxx_board_name &>/dev/null  || ipqxxxx_board_name() {
	echo $(board_name) | sed 's/^\([^-]*-\)\{1\}//g'
}

target_specific_settings() {
	local board=$(ipqxxxx_board_name)

	case "$board" in
		ap-al02-c1)
			#case for rdp418
			echo 0x71c71c > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0003\:01\:00.0/rx_hash
			echo 0x71c71c > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0004\:01\:00.0/rx_hash
			;;
		ap-al06)
			#case for rdp476 alder
			echo 0x69a2d1 > /sys/kernel/debug/ath11k/ahb-c000000.wifi/rx_hash
			;;
		*)
			#no settings
			;;
	esac
}

#Disables stats and QDSS tracing for reducing CPU load leaving more room for actual data traffic
#Disables link metrics update which is used by mesh
disable_stats_n_qdss_trace() {
	for dir in /sys/kernel/debug/ath11k/* /sys/kernel/debug/ath12k/*; do
        	if [ -f "$dir/stats_disable" ]; then
			echo 0 > "$dir/stats_disable"
			echo 1 > "$dir/stats_disable"
		fi

		#TODO: Disable these in ath12k after testing
		if [[ "$dir" != *ath12k* ]]; then
			if [ -f "$dir/ce_latency_stats" ]; then
				echo 0 > "$dir/ce_latency_stats"
			fi

			if [ -f "$dir/trace_qdss" ]; then
				echo 0 > "$dir/trace_qdss"
			fi
		fi
	done

	#Disable Global dp stats
	echo 0 > /sys/kernel/debug/ieee80211/phy00/dp_stats_mask
}

boost_performance() {
	#Enable Skb Recycler explicitly for 512M profile.
	[ -e /proc/device-tree/MP_512 ] && echo 1 >  proc/net/skb_recycler/skb_recycler_enable

	#Increase Max skb recycler buffer count per CPU pool
	echo "16384" > /proc/net/skb_recycler/max_skbs

	#Reduce Max skb recycler buffer count per CPU pool for 256M or 512M profile to 2048.
	[ -e /proc/device-tree/MP_256 ] || [ -e /proc/device-tree/MP_512 ] && echo "2048" > /proc/net/skb_recycler/max_skbs

	#Disable Generic receive offload(GRO) and Generic Segmentation offload(GSO) on interfaces
	eth_interfaces="eth0 eth1 eth4 eth5"
	for eth_iface in $eth_interfaces; do
		if [ -e "/sys/class/net/$eth_iface" ]; then
			ethtool -K "$eth_iface" gro off
			ethtool -K "$eth_iface" gso off
		fi
	done

	phy_list=$(ls /sys/class/ieee80211/)
	for phy in $phy_list; do
		iface_list=$(ls /sys/class/ieee80211/${phy}/device/net/)
		for iface in $iface_list; do
			interfaces="${interfaces} ${iface}"
		done
	done

	for iface in $interfaces; do
	case "$iface" in
		*.sta*)
			continue
			;;
		*)
			echo 0 > /sys/class/net/${iface}/queues/rx-0/rps_cpus
			;;
		esac
	done

	echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
	echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
	echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
	echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

	#Disables stats and QDSS tracing for reducing CPU load leaving more room for actual data traffic
	disable_stats_n_qdss_trace

	#Hash values per target for hash based steering if required
	target_specific_settings

	#FW logging can be disabled on need basis to reduce copy engine loads if detailed debug
	# logging is not required. Use below configuration.
	#		Add “en_fwlog=0” to /etc/modules.d/ath12k file as below
	#		ath12k dyndbg=+p en_fwlog=0
	#
	#		reboot
}

boost_performance
