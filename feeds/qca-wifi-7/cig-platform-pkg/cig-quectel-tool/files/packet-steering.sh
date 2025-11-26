#!/bin/sh

steering_flows="$(uci -q get "network.@globals[0].steering_flows")"
[ "${steering_flows:-0}" -gt 0 ] && opts="-l $steering_flows"
/usr/libexec/network/packet-steering.uc $opts $1

if [ "$(uci -q get network.up1v0.proto)" = "qmi" ]; then
	echo f > /sys/class/net/rmnet_mhi0/queues/rx-0/rps_cpus
	echo 4096 > /sys/class/net/rmnet_mhi0/queues/rx-0/rps_flow_cnt
	echo f > /sys/class/net/up1v0/queues/rx-0/rps_cpus
	echo 4096 > /sys/class/net/up1v0/queues/rx-0/rps_flow_cnt
fi
