#!/bin/sh
#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.  
#SPDX-License-Identifier: ISC

PROGNAME=$(basename "$0")

error_exit() {
	echo "${PROGNAME}: ${1:-Unknown Error}" 1>&2
	exit 1
}

ath12k_print_iw_dev() {
    echo "$1 TimeStamp[$(date)] iw dev output"
    iw dev
}

ath12k_print_ifconfig() {
    echo "$1 TimeStamp[$(date)] ifconfig"
    ifconfig
}

ath12k_print_brctl_show() {
    echo "$1 TimeStamp[$(date)] brctl show"
    brctl show
}

ath12k_print_arp() {
    echo "$1 TimeStamp[$(date)] ARP: arp"
    cat /proc/net/arp
}

# Function to print link information and station dump
ath12k_print_link_info_and_station_dump() {
    for interface in $if_names; do
		[ -z "$interface" ] && continue
		echo "$1 TimeStamp[$(date)] LinkInfo I/F[$interface]:"
		cat /sys/kernel/debug/ieee80211/$wiphy/netdev:$interface/primary_link
		echo "$1 TimeStamp[$(date)] STAs in I/F [$interface]:"
		ls /sys/kernel/debug/ieee80211/$wiphy/netdev:$interface/stations/
		num_stas=$(iw dev $interface station dump | grep Station | wc -l)
		echo "$1 TimeStamp[$(date)] station dump[$interface] NumStations[$num_stas]:"
		mac_addr=$(iw dev $interface station dump | grep Station | cut -d " " -f 2)
		index_count=1
		for mac in $mac_addr;
			do
			echo "$1 TimeStamp[$(date)] station[$mac] index[$index_count]:"
			iw dev $interface station get $mac
			index_count=$((index_count+1))
			parent_if=$(echo $interface | cut -d "." -f 1)
			echo "$1 TimeStamp[$(date)] STA LinkInfo[$mac] I/F[$parent_if] Count[$index_count/$num_stas]:"
		done;
		echo "$1 TimeStamp[$(date)] Station dump: iw dev $interface station dump"
		iw dev $interface station dump
		sleep 1
		echo "$1 TimeStamp[$(date)] Station dump: iw dev $interface station dump"
		iw dev $interface station dump
		echo "$1 TimeStamp[$(date)] Data Path type: iw $interface get_intf_offload type"
		iw $interface get_intf_offload type
	done
}

# Function to print SOC stats
ath12k_print_soc_stats() {
	echo "$1 TimeStamp[$(date)] ath12k Device list"
	ls /sys/kernel/debug/ath12k/
	find /sys/kernel/debug/ath12k -mindepth 1 -maxdepth 1 -type d | while read -r slot_path; do
        slot_id=$(basename "$slot_path")
        echo "$1 device_dp_stats: cat $slot_path/device_dp_stats"
        cat "$slot_path/device_dp_stats" 2>/dev/null
        echo "$1 ppeds_stats: cat $slot_path/ppeds_stats"
        cat "$slot_path/ppeds_stats" 2>/dev/null
    done
    echo "$1 TimeStamp[$(date)] sfe_dump"
    sfe_dump
}

# Function to print NET interrupt stats
ath12k_print_net_interrupt_stats() {
    echo "$1 TimeStamp[$(date)] Net Stats: cat /proc/net/dev"
    cat /proc/net/dev
    echo "$1 TimeStamp[$(date)] Interrupt Stats: cat /proc/interrupts"
    cat /proc/interrupts
}

# Function to print DS packet stats
ath12k_print_ds_packet_stats() {
    echo "$1 TimeStamp[$(date)] DS Packet Stats"
    echo "$1 TimeStamp[$(date)] cat /proc/sys/ppe/ppe_drv/if_bm_to_offload"
    cat /proc/sys/ppe/ppe_drv/if_bm_to_offload
    echo "$1 TimeStamp[$(date)] VP Stats: cat /sys/kernel/debug/qca-nss-ppe/ppe_vp/vp_stats"
    cat /sys/kernel/debug/qca-nss-ppe/ppe_vp/vp_stats
    echo "$1 TimeStamp[$(date)] PPE DS Node Stats: cat /sys/kernel/debug/qca-nss-ppe/ppe_ds/ppe_ds_node_stats"
    cat /sys/kernel/debug/qca-nss-ppe/ppe_ds/ppe_ds_node_stats
}

# Function to print HTT stats
ath12k_print_htt_stats() {
	htt_stats_str=$2
	htt_stats=$(echo "$htt_stats_str" | tr ',' ' ')
	htt_stats_print="1 2 9 10 12 17 26 27"
	if [ ! -z "$htt_stats" ] ;then
		htt_stats_print="$htt_stats_print $htt_stats"
	fi
	echo "$1 TimeStamp[$(date)] HTT STATS:"
	hw_links=$(find /sys/kernel/debug/ieee80211/*/netdev:*/*/*/ -maxdepth 1 -type d -name 'link-*' 2>/dev/null | sed -n 's#.*/link-\([0-9]*\)$#\1#p')
	for stat_id in $htt_stats_print; do
		for hw_id in $hw_links; do
			echo "$1 TimeStamp[$(date)] htt_stats radio[$hw_id] id:[$stat_id]"
			echo $stat_id > /sys/kernel/debug/ieee80211/$wiphy/ath12k_hw$hw_id/htt_stats_type
			[ "$stat_id" = "11" ] && continue
			cat /sys/kernel/debug/ieee80211/$wiphy/ath12k_hw$hw_id/htt_stats
		done;
		if [ "$stat_id" = "11" ]; then
			for interface in $if_names; do
				[ -z "$interface" ] && continue
				ls /sys/kernel/debug/ieee80211/$wiphy/netdev:$interface/stations/
				num_stas=$(iw dev $interface station dump | grep Station | wc -l)
				mac_addr=$(iw dev $interface station dump | grep Station | cut -d " " -f 2)
				index_count=1
				for mac in $mac_addr; do
					[ "$index_count" -gt "$max_stas_limit" ] && break;
					parent_if=$(echo $interface | cut -d "." -f 1)
					echo "$1 TimeStamp[$(date)] htt_peer_stats[$mac]I\F[$parent_if]Count[$index_count/$num_stas]:"
					cat /sys/kernel/debug/ieee80211/$wiphy/netdev:$parent_if/stations/$mac/link-*/htt_peer_stats
					index_count=$((index_count+1))
				done;
			done;
		fi
	done
}

# Function to print DS Flow stats
ath12k_print_ds_flow_stats() {
	echo "$1 TimeStamp[$(date)] DS Flow Stats"
	i=1
	while [ $i -le 5 ]; do
		echo "$1 TimeStamp[$(date)] ssdk_sh flow flowipv45tuple show"
		ssdk_sh flow flowipv45tuple show
		echo "$1 TimeStamp[$(date)] ecm_dump.sh | grep accel_mode"
		ecm_dump.sh | grep accel_mode
		sleep 2
		i=$((i + 1))
	done
}

# Function to print CPU and network stats
ath12k_print_cpu_stats() {
	echo "$1 TimeStamp[$(date)] phy[$wiphy]:"
	echo "$1 TimeStamp[$(date)] CPU stats: taskset 0x8 mpstat -P ALL 1 3" 
	taskset 0x8 mpstat -P ALL 1 3
	echo "$1 TimeStamp[$(date)] Network rate stats: taskset 0x8 sar -n DEV 1 3" 
	taskset 0x8 sar -n DEV 1 3
}

# Function to print script usage
ath12k_print_help() {
    echo "Usage: sh $PROGNAME <before|during|after> [htt_stats]"
    echo ""
    echo "Arguments:"
    echo "  before       Collects mandatory stats before traffic run"
    echo "  during       Collects mandatory stats during traffic run"
    echo "  after        Collects mandatory stats after traffic run"
    echo "  help         Displays this help message"
    echo ""
    echo "Optional:"
    echo "  htt_stats    Comma-separated list of HTT stats (e.g: 3,4,5)"
    echo ""
    echo "Example:"
    echo "  sh $PROGNAME before"
    echo "  sh $PROGNAME after 3,4,5"
}

# Function to print mesh stats
ath12k_print_mesh_stats() {
	for interface in $if_names; do
		[ -z "$interface" ] && continue
		echo "$1 TimeStamp[$(date)] Mesh Stats I/F[$interface]:"
		echo "$1 TimeStamp[$(date)] mpath dump: taskset 0x8 iw $interface mpath dump"
		taskset 0x8 iw $interface mpath dump
		echo "$1 TimeStamp[$(date)] mpp dump: taskset 0x8 iw $interface mpp dump"
		taskset 0x8 iw $interface mpp dump
	done
}

# Function to reset DP SOC stats and HTT stats
ath12k_reset_stats() {
	htt_stats_str=$2
	htt_stats=$(echo "$htt_stats_str" | tr ',' ' ')
	htt_stats_print="1 2 9 10 12 17 26 27"
	if [ ! -z "$htt_stats" ] ;then
		htt_stats_print="$htt_stats_print $htt_stats"
	fi
	echo "$1 TimeStamp[$(date)] HTT STATS:"
	hw_links=$(find /sys/kernel/debug/ieee80211/*/netdev:*/*/*/ -maxdepth 1 -type d -name 'link-*' 2>/dev/null | sed -n 's#.*/link-\([0-9]*\)$#\1#p')
	for stat_id in $htt_stats_print; do
		for hw_id in $hw_links; do
			echo "$1 TimeStamp[$(date)] Resetting htt_stats radio[$hw_id] id:[$stat_id]"
			echo $stat_id > /sys/kernel/debug/ieee80211/$wiphy/ath12k_hw$hw_id/htt_stats_reset
		done
	done
	find /sys/kernel/debug/ath12k -mindepth 1 -maxdepth 1 -type d | while read -r slot_path; do
        slot_id=$(basename "$slot_path")
        echo "$1 Resetting DP SOC Stats before run for [$slot_id]"
        echo reset > "$slot_path/device_dp_stats" 2>/dev/null
        echo reset > "$slot_path/ppeds_stats" 2>/dev/null
	done
}
		

test_execution_str=$1
case "$test_execution_str" in
	BEFORE|Before|before) test_execution="before" ;;
	DURING|During|during) test_execution="during" ;;
	AFTER|After|after) test_execution="after" ;;
	RESET|Reset|reset) test_execution="reset" ;;
	HELP|Help|help) test_execution="help" ;;
	*) test_execution="$test_execution_str" ;;
esac

wiphy=phy0$(iw dev | grep phy# | cut -d "#" -f 2)
if_names=$(iw dev | grep Interface | cut -d " " -f 2)
if [ -z "$wiphy" ] ;then
	wiphy="phy00"
fi

max_stas_limit=4

case "$test_execution" in 
	before)
		prefix="[STATS_BEFORE_TRAFFIC]"
		ath12k_print_iw_dev "$prefix"
		sleep 1
		ath12k_print_iw_dev "$prefix"
		ath12k_print_ifconfig "$prefix"
		ath12k_print_brctl_show "$prefix"
		ath12k_print_arp "$prefix"
		ath12k_print_link_info_and_station_dump "$prefix"
		ath12k_print_htt_stats "$prefix" "$2"
		ath12k_print_net_interrupt_stats "$prefix"
		ath12k_print_ds_packet_stats "$prefix"
		ath12k_print_soc_stats "$prefix"
		;;
	during)
		prefix="[STATS_DURING_TRAFFIC]"
		ath12k_print_cpu_stats "$prefix"
		ath12k_print_ds_flow_stats "$prefix"
		ath12k_print_mesh_stats "$prefix"
		;;
	after)
		prefix="[STATS_AFTER_TRAFFIC]"
		ath12k_print_iw_dev "$prefix"
		sleep 1
		ath12k_print_iw_dev "$prefix"
		ath12k_print_ifconfig "$prefix"
		ath12k_print_brctl_show "$prefix"
		ath12k_print_arp "$prefix"
		ath12k_print_link_info_and_station_dump "$prefix"
		ath12k_print_htt_stats "$prefix" "$2"
		ath12k_print_net_interrupt_stats "$prefix"
		ath12k_print_ds_packet_stats "$prefix"
		ath12k_print_soc_stats "$prefix"
		;;
	reset)
		prefix="[RESETTING STATS]"
		ath12k_reset_stats "$prefix" "$2"
		;;
	help)
		ath12k_print_help
		;;
	*)
	error_exit "Invalid test_execution value: must be 'before', 'during', or 'after'"
		;;
esac
