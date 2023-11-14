#!/bin/sh

. /lib/functions.sh

port_mirror() {
	monitor=$(uci get switch.mirror.monitor)
	analysis=$(uci get switch.mirror.analysis)

	[ -n "$monitor" -a -n "$analysis" ] || return
	tc qdisc del dev $analysis clsact

	for port in $monitor; do
		tc qdisc del dev $port clsact
		tc qdisc add dev $port clsact
		tc filter add dev $port ingress matchall action mirred egress mirror dev $analysis
		tc filter add dev $port egress matchall action mirred egress mirror dev $analysis
	done
}

port_mirror
