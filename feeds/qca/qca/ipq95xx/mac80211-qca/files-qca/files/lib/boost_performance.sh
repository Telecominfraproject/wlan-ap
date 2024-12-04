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

type ipq806x_board_name &>/dev/null  || ipq806x_board_name() {
        echo $(board_name) | sed 's/^\([^-]*-\)\{1\}//g'
}

boost_performance() {
	local board=$(ipq806x_board_name)

	if [ -e /sys/module/ath11k/parameters/nss_offload ];then
		uni_dp=0
	else
		uni_dp=1
	fi

		case "$board" in
			ap-hk10-c2)
				#case for rdp413
				echo 1 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0000\:01\:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0001\:01\:00.0/stats_disable

				echo 0 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0000\:01\:00.0/ce_latency_stats
				echo 0 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0001\:01\:00.0/ce_latency_stats

				echo 0 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0000\:01\:00.0/trace_qdss
				echo 0 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0001\:01\:00.0/trace_qdss

				tc qdisc replace dev wlan0 root noqueue
				tc qdisc replace dev wlan1 root noqueue

				echo 0 > /proc/sys/dev/nss/clock/auto_scale
				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
				;;
			ap-hk14)
				#case for rdp419
				echo 1 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0000\:01\:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath11k/ipq8074\ hw2.0/stats_disable

				echo 0 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0000\:01\:00.0/ce_latency_stats
				echo 0 > /sys/kernel/debug/ath11k/ipq8074\ hw2.0/ce_latency_stats

				echo 0 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0000\:01\:00.0/trace_qdss
				echo 0 > /sys/kernel/debug/ath11k/ipq8074\ hw2.0/trace_qdss

				tc qdisc replace dev wlan0 root noqueue
				tc qdisc replace dev wlan1 root noqueue
				tc qdisc replace dev wlan2 root noqueue

				echo 0 > /proc/sys/dev/nss/clock/auto_scale
				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
				;;
			ap-oak03)
				#case for rdp393
				echo 1 > /sys/kernel/debug/ath11k/ipq8074\ hw2.0/stats_disable
				echo 0 > /sys/kernel/debug/ath11k/ipq8074\ hw2.0/ce_latency_stats
				echo 0 > /sys/kernel/debug/ath11k/ipq8074\ hw2.0/trace_qdss

				tc qdisc replace dev wlan0 root noqueue
				tc qdisc replace dev wlan1 root noqueue
				tc qdisc replace dev wlan2 root noqueue

				echo 0 > /proc/sys/dev/nss/clock/auto_scale
				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
				;;
			ap-cp01-c1)
				#case for rdp352
				echo 1 > /sys/kernel/debug/ath11k/ipq6018\ hw1.0/stats_disable
				echo 0 > /sys/kernel/debug/ath11k/ipq6018\ hw1.0/ce_latency_stats
				echo 0 > /sys/kernel/debug/ath11k/ipq6018\ hw1.0/trace_qdss

				tc qdisc replace dev wlan0 root noqueue
				tc qdisc replace dev wlan1 root noqueue

				echo 0 > /proc/sys/dev/nss/clock/auto_scale
				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
				;;
			ap-mp03.5-c1)
				#case for rdp432
				if [ "$uni_dp" -eq 1 ];then
					ethtool -K eth1 gro off
					ethtool -K eth0 gro off
					ethtool -K wlan0 gro off
					ethtool -K wlan1 gro off
					ethtool -K wlan2 gro off

					tc qdisc replace dev wlan0 root noqueue
					tc qdisc replace dev wlan1 root noqueue
					tc qdisc replace dev wlan2 root noqueue
					tc qdisc replace dev eth0 root noqueue
					tc qdisc replace dev eth1 root noqueue

					echo 1 > /sys/kernel/debug/ath11k/qcn6122_2/stats_disable
					echo 1 > /sys/kernel/debug/ath11k/qcn6122_1/stats_disable
					echo 1 > /sys/kernel/debug/ath11k/ipq5018\ hw1.0/stats_disable

					echo 0 > /sys/kernel/debug/ath11k/ipq5018\ hw1.0/trace_qdss
					echo 0 > /sys/kernel/debug/ath11k/qcn6122_1/trace_qdss
					echo 0 > /sys/kernel/debug/ath11k/qcn6122_2/trace_qdss

					echo 0x44444444 > /sys/kernel/debug/ath11k/ipq5018\ hw1.0/rx_hash
					echo 0x44444444 > /sys/kernel/debug/ath11k/qcn6122_1/rx_hash
					echo 0x44444444 > /sys/kernel/debug/ath11k/qcn6122_2/rx_hash
				fi
				;;
			ap-al02-c1)
				#case for rdp418
				echo 1 > /sys/kernel/debug/ath11k/ipq9574/stats_disable
				echo 0 > /sys/kernel/debug/ath11k/ipq9574/ce_latency_stats

				echo 1 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0003\:01\:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0004\:01\:00.0/stats_disable

				echo 0 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0003\:01\:00.0/ce_latency_stats
				echo 0 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0004\:01\:00.0/ce_latency_stats

				echo 0 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0003\:01\:00.0/trace_qdss
				echo 0 > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0004\:01\:00.0/trace_qdss

				tc qdisc replace dev wlan0 root noqueue
				tc qdisc replace dev wlan1 root noqueue
				tc qdisc replace dev wlan2 root noqueue
				tc qdisc replace dev eth0 root noqueue
				tc qdisc replace dev eth1 root noqueue
				tc qdisc replace dev eth2 root noqueue
				tc qdisc replace dev eth4 root noqueue
				tc qdisc replace dev eth5 root noqueue

				ethtool -K eth4 gro off
				ethtool -K eth4 gso off
				ethtool -K eth5 gro off
				ethtool -K eth5 gso off

				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

				echo 0x71c71c > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0003\:01\:00.0/rx_hash
				echo 0x71c71c > /sys/kernel/debug/ath11k/qcn9074\ hw1.0_0004\:01\:00.0/rx_hash

				;;
			ap-al02-c4)
				tc qdisc replace dev eth0 root noqueue
				tc qdisc replace dev eth1 root noqueue
				tc qdisc replace dev eth2 root noqueue
				tc qdisc replace dev eth4 root noqueue
				tc qdisc replace dev eth5 root noqueue
				ethtool -K eth4 gro off
				ethtool -K eth4 gso off
				ethtool -K eth5 gro off
				ethtool -K eth5 gso off
				ssdk_sh fdb learnCtrl set disable
				ssdk_sh fdb entry flush 1
				sysctl -w net.bridge.bridge-nf-call-ip6tables=1
				sysctl -w net.bridge.bridge-nf-call-iptables=1
				echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
				/etc/init.d/firewall stop

				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
				if [ -d "/sys/kernel/debug/ath12k" ]; then
					# logic to identify QCN9274 V1.0 / V2.0
					soc=`ls  /sys/kernel/debug/ath12k/ | head -1 |awk '{print substr($0,0,13)}' | awk '{print $2}'`
					case $soc in
						"hw1.0")
							echo 0x21212121 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0002\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0002\:01\:00.0/rx_hash_ix3
							echo 0x33333333 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0003\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0003\:01\:00.0/rx_hash_ix3
							echo 0x21212121 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0001\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0001\:01\:00.0/rx_hash_ix3
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0004\:01\:00.0/stats_disable
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0003\:01\:00.0/stats_disable
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0002\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0004\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0003\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0002\:01\:00.0/stats_disable

						;;
						"hw2.0")
							echo 0x21212121 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0002\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0002\:01\:00.0/rx_hash_ix3
							echo 0x33333333 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0003\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0003\:01\:00.0/rx_hash_ix3
							echo 0x21212121 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001\:01\:00.0/rx_hash_ix3
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001\:01\:00.0/stats_disable
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0003\:01\:00.0/stats_disable
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0002\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0003\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0002\:01\:00.0/stats_disable

						;;
					esac
				fi
				echo 0 > /sys/class/net/wlan0/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan1/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan2/queues/rx-0/rps_cpus

				if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
					tc qdisc replace dev wlan0_b root noqueue
					tc qdisc replace dev wlan0_l0 root noqueue
					tc qdisc replace dev wlan0_l1 root noqueue
					tc qdisc replace dev wlan0_l2 root noqueue

					tc qdisc replace dev eth0 root noqueue
					tc qdisc replace dev eth1 root noqueue
					tc qdisc replace dev eth2 root noqueue
					tc qdisc replace dev eth3 root noqueue
					tc qdisc replace dev eth4 root noqueue
					tc qdisc replace dev eth5 root noqueue

					echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
					echo f > /proc/net/nf_conntrack
				fi

				tc qdisc replace dev wlan0 root noqueue
				tc qdisc replace dev wlan1 root noqueue
				tc qdisc replace dev wlan2 root noqueue
				echo "16384" > /proc/net/skb_recycler/max_skbs
				#case for rdp433 (QCN9274 2.4, 5, 6 GHz)

				;;
			ap-al02-c6)
				#rdp433 (IPQ9574(2.4 GHz) + QCN9274(5 and 6 GHz))

				tc qdisc replace dev eth0 root noqueue
				tc qdisc replace dev eth1 root noqueue
				tc qdisc replace dev eth2 root noqueue
				tc qdisc replace dev eth4 root noqueue
				tc qdisc replace dev eth5 root noqueue
				ethtool -K eth4 gro off
				ethtool -K eth4 gso off
				ethtool -K eth5 gro off
				ethtool -K eth5 gso off
				ssdk_sh fdb learnCtrl set disable
				ssdk_sh fdb entry flush 1
				sysctl -w net.bridge.bridge-nf-call-ip6tables=1
				sysctl -w net.bridge.bridge-nf-call-iptables=1
				echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
				/etc/init.d/firewall stop

				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
				if [ -d "/sys/kernel/debug/ath12k" ]; then
					# logic to identify QCN9274 V1.0 / V2.0
					soc=`ls  /sys/kernel/debug/ath12k/ | head -1 |awk '{print substr($0,0,13)}' | awk '{print $2}'`
					case $soc in
						"hw1.0")
							echo 0x33333333 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0004\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0004\:01\:00.0/rx_hash_ix3
							echo 0x21212121 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0003\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0003\:01\:00.0/rx_hash_ix3
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0004\:01\:00.0/stats_disable
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0003\:01\:00.0/stats_disable
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0002\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0004\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0003\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0002\:01\:00.0/stats_disable

						;;
						"hw2.0")
							echo 0x33333333 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0004\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0004\:01\:00.0/rx_hash_ix3
							echo 0x21212121 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0003\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0003\:01\:00.0/rx_hash_ix3
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0004\:01\:00.0/stats_disable
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0003\:01\:00.0/stats_disable
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0002\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0004\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0003\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0002\:01\:00.0/stats_disable

						;;
					esac
				fi
				echo 0 > /sys/class/net/wlan0/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan1/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan2/queues/rx-0/rps_cpus

				if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
					tc qdisc replace dev wlan0_b root noqueue
					tc qdisc replace dev wlan0_l0 root noqueue
					tc qdisc replace dev wlan0_l1 root noqueue
					tc qdisc replace dev wlan0_l2 root noqueue

					tc qdisc replace dev eth0 root noqueue
					tc qdisc replace dev eth1 root noqueue
					tc qdisc replace dev eth2 root noqueue
					tc qdisc replace dev eth3 root noqueue
					tc qdisc replace dev eth4 root noqueue
					tc qdisc replace dev eth5 root noqueue

					echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
					echo f > /proc/net/nf_conntrack
				fi


				tc qdisc replace dev wlan0 root noqueue
				tc qdisc replace dev wlan1 root noqueue
				tc qdisc replace dev wlan2 root noqueue
				echo "16384" > /proc/net/skb_recycler/max_skbs
				;;
			ap-al02-c9)
				#case for rdp454 (QCN9274 (2.4 and 5 Low) + QCN9274 (5 High and 6 GHz))

				tc qdisc replace dev eth0 root noqueue
				tc qdisc replace dev eth1 root noqueue
				tc qdisc replace dev eth2 root noqueue
				tc qdisc replace dev eth4 root noqueue
				tc qdisc replace dev eth5 root noqueue
				ethtool -K eth4 gro off
				ethtool -K eth4 gso off
				ethtool -K eth5 gro off
				ethtool -K eth5 gso off
				ssdk_sh fdb learnCtrl set disable
				ssdk_sh fdb entry flush 1
				sysctl -w net.bridge.bridge-nf-call-ip6tables=1
				sysctl -w net.bridge.bridge-nf-call-iptables=1
				echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
				/etc/init.d/firewall stop

				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
				if [ -d "/sys/kernel/debug/ath12k" ]; then
					# logic to identify QCN9274 V1.0 / V2.0
					soc=`ls  /sys/kernel/debug/ath12k/ | head -1 |awk '{print substr($0,0,13)}' | awk '{print $2}'`
					case $soc in
						"hw1.0")
							echo 0x33333333 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0000\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0000\:01\:00.0/rx_hash_ix3
							echo 0x21212121 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0002\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0002\:01\:00.0/rx_hash_ix3
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0001\:01\:00.0/stats_disable
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0003\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0001\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw1.0_0003\:01\:00.0/stats_disable

						;;
						"hw2.0")
							echo 0x33333333 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000\:01\:00.0/rx_hash_ix3
							echo 0x21212121 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0002\:01\:00.0/rx_hash_ix2
							echo 0x21321321 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0002\:01\:00.0/rx_hash_ix3
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001\:01\:00.0/stats_disable
							echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0003\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001\:01\:00.0/stats_disable
							echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0003\:01\:00.0/stats_disable

						;;
					esac
				fi
				echo 0 > /sys/class/net/wlan0/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan1/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan2/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan3/queues/rx-0/rps_cpus

				if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
					tc qdisc replace dev wlan0_b root noqueue
					tc qdisc replace dev wlan0_l0 root noqueue
					tc qdisc replace dev wlan0_l1 root noqueue
					tc qdisc replace dev wlan0_l2 root noqueue

					tc qdisc replace dev eth0 root noqueue
					tc qdisc replace dev eth1 root noqueue
					tc qdisc replace dev eth2 root noqueue
					tc qdisc replace dev eth3 root noqueue
					tc qdisc replace dev eth4 root noqueue
					tc qdisc replace dev eth5 root noqueue

					echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
					echo f > /proc/net/nf_conntrack
				fi


				tc qdisc replace dev wlan0 root noqueue
				tc qdisc replace dev wlan1 root noqueue
				tc qdisc replace dev wlan2 root noqueue
				tc qdisc replace dev wlan3 root noqueue
				echo "16384" > /proc/net/skb_recycler/max_skbs

				;;
			ap-mi01.2)
				tc qdisc replace dev eth0 root noqueue
				tc qdisc replace dev eth1 root noqueue

				ethtool -K eth0 gro off
				ethtool -K eth0 gso off
				ethtool -K eth1 gro off
				ethtool -K eth1 gso off

				ssdk_sh fdb learnCtrl set disable
				ssdk_sh fdb entry flush 1

				sysctl -w net.bridge.bridge-nf-call-ip6tables=1
				sysctl -w net.bridge.bridge-nf-call-iptables=1

				echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all

				/etc/init.d/firewall stop

				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

				#5G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/rx_hash_ix3

				#For 6G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/rx_hash_ix3

				#For 2G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix3

				echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/stats_disable

				echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/stats_disable

				echo 0 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable

				echo 0 > /sys/class/net/wlan0/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan1/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan2/queues/rx-0/rps_cpus

				if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
					tc qdisc replace dev wlan0_b root noqueue
					tc qdisc replace dev wlan0_l0 root noqueue
					tc qdisc replace dev wlan0_l1 root noqueue
					tc qdisc replace dev wlan0_l2 root noqueue

					tc qdisc replace dev eth0 root noqueue
					tc qdisc replace dev eth1 root noqueue
					tc qdisc replace dev eth2 root noqueue
					tc qdisc replace dev eth3 root noqueue
					tc qdisc replace dev eth4 root noqueue
					tc qdisc replace dev eth5 root noqueue

					echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
					echo f > /proc/net/nf_conntrack
				fi


				tc qdisc replace dev wlan0 root noqueue
                                tc qdisc replace dev wlan1 root noqueue
                                tc qdisc replace dev wlan2 root noqueue
				echo "16384" > /proc/net/skb_recycler/max_skbs
				#no settings
				;;
			ap-mi01.3 | \
			ap-mi04.1)
				#case for RDP442, RDP446 (IPQ5332(2.4GHz) + QCN6432(5/6 GHz)))
                                tc qdisc replace dev eth0 root noqueue
                                tc qdisc replace dev eth1 root noqueue

                                ethtool -K eth0 gro off
                                ethtool -K eth0 gso off
                                ethtool -K eth1 gro off
                                ethtool -K eth1 gso off

                                ssdk_sh fdb learnCtrl set disable
                                ssdk_sh fdb entry flush 1

                                sysctl -w net.bridge.bridge-nf-call-ip6tables=1
                                sysctl -w net.bridge.bridge-nf-call-iptables=1

                                echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all

                                /etc/init.d/firewall stop

                                echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
                                echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
                                echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
                                echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

                                #For 6GHz reo queues
                                echo 0x23123123 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_2/rx_hash_ix2
                                echo 0x23123123 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_2/rx_hash_ix3

                                #For 5GHz reo queues
                                echo 0x23123123 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/rx_hash_ix2
                                echo 0x23123123 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/rx_hash_ix3

                                #For 2GHz reo queues
                                echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix2
                                echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix3

                                echo 0 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_2/stats_disable
                                echo 1 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_2/stats_disable

                                echo 0 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/stats_disable
                                echo 1 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/stats_disable

                                echo 0 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable
                                echo 1 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable

                                echo 0 > /sys/class/net/wlan0/queues/rx-0/rps_cpus
                                echo 0 > /sys/class/net/wlan1/queues/rx-0/rps_cpus
                                echo 0 > /sys/class/net/wlan2/queues/rx-0/rps_cpus

                                tc qdisc replace dev wlan0 root noqueue
                                tc qdisc replace dev wlan1 root noqueue
                                tc qdisc replace dev wlan2 root noqueue
                                echo "16384" > /proc/net/skb_recycler/max_skbs
				if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
					tc qdisc replace dev wlan0_b root noqueue
					tc qdisc replace dev wlan0_l0 root noqueue
					tc qdisc replace dev wlan0_l1 root noqueue
					tc qdisc replace dev wlan0_l2 root noqueue

					tc qdisc replace dev eth0 root noqueue
					tc qdisc replace dev eth1 root noqueue
					tc qdisc replace dev eth2 root noqueue
					tc qdisc replace dev eth3 root noqueue
					tc qdisc replace dev eth4 root noqueue
					tc qdisc replace dev eth5 root noqueue

					echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
					echo f > /proc/net/nf_conntrack
				fi
                                #nosettings
                                ;;
			ap-mi01.6)
                                tc qdisc replace dev eth0 root noqueue
                                tc qdisc replace dev eth1 root noqueue

                                ethtool -K eth0 gro off
                                ethtool -K eth0 gso off
                                ethtool -K eth1 gro off
                                ethtool -K eth1 gso off

                                ssdk_sh fdb learnCtrl set disable
                                ssdk_sh fdb entry flush 1

                                sysctl -w net.bridge.bridge-nf-call-ip6tables=1
                                sysctl -w net.bridge.bridge-nf-call-iptables=1

                                echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all

                                /etc/init.d/firewall stop

                                echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
                                echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
                                echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
                                echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

                                #For 5G/6G reo queues
                                echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/rx_hash_ix2
                                echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/rx_hash_ix3

                                #For 2G reo queues
                                echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix2
                                echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix3

				echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/stats_disable

				echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/stats_disable

				echo 0 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable

                                echo 0 > /sys/class/net/wlan0/queues/rx-0/rps_cpus
                                echo 0 > /sys/class/net/wlan1/queues/rx-0/rps_cpus
                                echo 0 > /sys/class/net/wlan2/queues/rx-0/rps_cpus

                                if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
                                    tc qdisc replace dev wlan0_b root noqueue
                                    tc qdisc replace dev wlan0_l0 root noqueue
                                    tc qdisc replace dev wlan0_l1 root noqueue
                                    tc qdisc replace dev wlan0_l2 root noqueue

                                    tc qdisc replace dev eth0 root noqueue
                                    tc qdisc replace dev eth1 root noqueue
                                    tc qdisc replace dev eth2 root noqueue
                                    tc qdisc replace dev eth3 root noqueue
                                    tc qdisc replace dev eth4 root noqueue
                                    tc qdisc replace dev eth5 root noqueue

                                    echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
                                    echo f > /proc/net/nf_conntrack
                                fi

                                tc qdisc replace dev wlan0 root noqueue
                                tc qdisc replace dev wlan1 root noqueue
                                tc qdisc replace dev wlan2 root noqueue
				echo "16384" > /proc/net/skb_recycler/max_skbs
                                #no settings
                                ;;
			ap-mi01.9)
				tc qdisc replace dev eth0 root noqueue
				tc qdisc replace dev eth1 root noqueue

				ethtool -K eth0 gro off
				ethtool -K eth0 gso off
				ethtool -K eth1 gro off
				ethtool -K eth1 gso off

				ssdk_sh fdb learnCtrl set disable
				ssdk_sh fdb entry flush 1

				sysctl -w net.bridge.bridge-nf-call-ip6tables=1
				sysctl -w net.bridge.bridge-nf-call-iptables=1

				echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all

				/etc/init.d/firewall stop

				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

				#For 5G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/rx_hash_ix3

				#For 2G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/rx_hash_ix3

				echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/stats_disable

				echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/stats_disable

				echo 0 > /sys/class/net/wlan0/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan1/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan2/queues/rx-0/rps_cpus
				
				if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
					tc qdisc replace dev wlan0_b root noqueue
					tc qdisc replace dev wlan0_l0 root noqueue
					tc qdisc replace dev wlan0_l1 root noqueue
					tc qdisc replace dev wlan0_l2 root noqueue
				
					tc qdisc replace dev eth0 root noqueue
					tc qdisc replace dev eth1 root noqueue
					tc qdisc replace dev eth2 root noqueue
					tc qdisc replace dev eth3 root noqueue
					tc qdisc replace dev eth4 root noqueue
					tc qdisc replace dev eth5 root noqueue
				
					echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
					echo f > /proc/net/nf_conntrack
				fi

				tc qdisc replace dev wlan0 root noqueue
				tc qdisc replace dev wlan1 root noqueue
				tc qdisc replace dev wlan2 root noqueue
				echo "16384" > /proc/net/skb_recycler/max_skbs
				;;
			ap-mi01.3-c2 | \
			ap-mi04.1-c2)

				tc qdisc replace dev eth0 root noqueue
				tc qdisc replace dev eth1 root noqueue

				ethtool -K eth0 gro off
				ethtool -K eth0 gso off
				ethtool -K eth1 gro off
				ethtool -K eth1 gso off

				ssdk_sh fdb learnCtrl set disable
				ssdk_sh fdb entry flush 1

				sysctl -w net.bridge.bridge-nf-call-ip6tables=1
				sysctl -w net.bridge.bridge-nf-call-iptables=1

				echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all

				/etc/init.d/firewall stop

				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

				#For 5GHz reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/rx_hash_ix3

				#For 2GHz reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix3

				echo 0 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/stats_disable

				echo 0 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable

				echo 0 > /sys/class/net/wlan0/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan1/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan2/queues/rx-0/rps_cpus

				if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
					tc qdisc replace dev wlan0_b root noqueue
					tc qdisc replace dev wlan0_l0 root noqueue
					tc qdisc replace dev wlan0_l1 root noqueue
					tc qdisc replace dev wlan0_l2 root noqueue

					tc qdisc replace dev eth0 root noqueue
					tc qdisc replace dev eth1 root noqueue
					tc qdisc replace dev eth2 root noqueue
					tc qdisc replace dev eth3 root noqueue
					tc qdisc replace dev eth4 root noqueue
					tc qdisc replace dev eth5 root noqueue

					echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
					echo f > /proc/net/nf_conntrack
				fi

				tc qdisc replace dev wlan0 root noqueue
				tc qdisc replace dev wlan1 root noqueue
				tc qdisc replace dev wlan2 root noqueue
				echo "16384" > /proc/net/skb_recycler/max_skbs
				#no settings
				;;
			ap-mi01.14)
				tc qdisc replace dev eth0 root noqueue
				tc qdisc replace dev eth1 root noqueue

				ethtool -K eth0 gro off
				ethtool -K eth0 gso off
				ethtool -K eth1 gro off
				ethtool -K eth1 gso off

				ssdk_sh fdb learnCtrl set disable
				ssdk_sh fdb entry flush 1

				sysctl -w net.bridge.bridge-nf-call-ip6tables=1
				sysctl -w net.bridge.bridge-nf-call-iptables=1

				echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all

				/etc/init.d/firewall stop

				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

				#5G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/rx_hash_ix2
			    echo 0x23123123 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/rx_hash_ix3

				#For 6G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/rx_hash_ix3

				#For 2G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix3

				echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/stats_disable

				echo 0 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/stats_disable

				echo 0 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable

				echo 0 > /sys/class/net/wlan0/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan1/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan2/queues/rx-0/rps_cpus

				if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
					tc qdisc replace dev wlan0_b root noqueue
					tc qdisc replace dev wlan0_l0 root noqueue
					tc qdisc replace dev wlan0_l1 root noqueue
					tc qdisc replace dev wlan0_l2 root noqueue

					tc qdisc replace dev eth0 root noqueue
					tc qdisc replace dev eth1 root noqueue
					tc qdisc replace dev eth2 root noqueue
					tc qdisc replace dev eth3 root noqueue
					tc qdisc replace dev eth4 root noqueue
					tc qdisc replace dev eth5 root noqueue

					echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
					echo f > /proc/net/nf_conntrack
				fi


				tc qdisc replace dev wlan0 root noqueue
                                tc qdisc replace dev wlan1 root noqueue
                                tc qdisc replace dev wlan2 root noqueue
				echo "16384" > /proc/net/skb_recycler/max_skbs
				#no settings
				;;
			ap-mi01.12)
				tc qdisc replace dev eth0 root noqueue
				tc qdisc replace dev eth1 root noqueue

				ethtool -K eth0 gro off
				ethtool -K eth0 gso off
				ethtool -K eth1 gro off
				ethtool -K eth1 gso off

				ssdk_sh fdb learnCtrl set disable
				ssdk_sh fdb entry flush 1

				sysctl -w net.bridge.bridge-nf-call-ip6tables=1
				sysctl -w net.bridge.bridge-nf-call-iptables=1

				echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all

				/etc/init.d/firewall stop

				echo "performance" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
				echo "performance" > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

				#5G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/rx_hash_ix2
			    echo 0x23123123 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/rx_hash_ix3

				#For 6G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0000:01:00.0/rx_hash_ix3

				#For 2G reo queues
				echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix2
				echo 0x23123123 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/rx_hash_ix3

				echo 0 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn9274\ hw2.0_0001:01:00.0/stats_disable

				echo 0 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/qcn6432\ hw1.0_1/stats_disable

				echo 0 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable
				echo 1 > /sys/kernel/debug/ath12k/ipq5332\ hw1.0_c000000.wifi/stats_disable

				echo 0 > /sys/class/net/wlan0/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan1/queues/rx-0/rps_cpus
				echo 0 > /sys/class/net/wlan2/queues/rx-0/rps_cpus

				if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
					tc qdisc replace dev wlan0_b root noqueue
					tc qdisc replace dev wlan0_l0 root noqueue
					tc qdisc replace dev wlan0_l1 root noqueue
					tc qdisc replace dev wlan0_l2 root noqueue

					tc qdisc replace dev eth0 root noqueue
					tc qdisc replace dev eth1 root noqueue
					tc qdisc replace dev eth2 root noqueue
					tc qdisc replace dev eth3 root noqueue
					tc qdisc replace dev eth4 root noqueue
					tc qdisc replace dev eth5 root noqueue

					echo 1 > /sys/kernel/debug/ecm/ecm_db/defunct_all
					echo f > /proc/net/nf_conntrack
				fi


				tc qdisc replace dev wlan0 root noqueue
                                tc qdisc replace dev wlan1 root noqueue
                                tc qdisc replace dev wlan2 root noqueue
				echo "16384" > /proc/net/skb_recycler/max_skbs
				#no settings
				;;

			*)
				#no settings
				;;
		esac
}

boost_performance
