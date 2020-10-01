#!/bin/sh
[ -d "/sys/class/net/wlan0" ] && {
	echo e > /sys/class/net/wlan0/queues/rx-0/rps_cpus
	echo 0 > /sys/class/net/wlan0/queues/tx-0/xps_cpus
	echo 0 > /sys/class/net/wlan0/queues/tx-1/xps_cpus
	echo 0 > /sys/class/net/wlan0/queues/tx-2/xps_cpus
	echo 0 > /sys/class/net/wlan0/queues/tx-3/xps_cpus
}
[ -d "/sys/class/net/wlan1" ] && {
	echo e > /sys/class/net/wlan1/queues/rx-0/rps_cpus
	echo 0 > /sys/class/net/wlan1/queues/tx-0/xps_cpus
	echo 0 > /sys/class/net/wlan1/queues/tx-1/xps_cpus
	echo 0 > /sys/class/net/wlan1/queues/tx-2/xps_cpus
	echo 0 > /sys/class/net/wlan1/queues/tx-3/xps_cpus
}
[ -d "/sys/class/net/wlan2" ] && {
	echo e > /sys/class/net/wlan2/queues/rx-0/rps_cpus
	echo 0 > /sys/class/net/wlan2/queues/tx-0/xps_cpus
	echo 0 > /sys/class/net/wlan2/queues/tx-1/xps_cpus
	echo 0 > /sys/class/net/wlan2/queues/tx-2/xps_cpus
	echo 0 > /sys/class/net/wlan2/queues/tx-3/xps_cpus
}

[ -d "/proc/sys/dev/nss/n2hcfg/" ] && echo 2048 > /proc/sys/dev/nss/n2hcfg/n2h_queue_limit_core0
[ -d "/proc/sys/dev/nss/n2hcfg/" ] && echo 2048 > /proc/sys/dev/nss/n2hcfg/n2h_queue_limit_core1

[ -d "/proc/sys/dev/nss/rps/" ] && echo 14 > /proc/sys/dev/nss/rps/hash_bitmap
