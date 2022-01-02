#!/bin/sh

wanvlan_interface="`uci get network.wanvlan 2>/dev/null`"

[ -z "$wanvlan_interface" ] && {
    uci set network.wanvlan=interface
    uci set network.wanvlan.enabled=0
    uci commit network
}


