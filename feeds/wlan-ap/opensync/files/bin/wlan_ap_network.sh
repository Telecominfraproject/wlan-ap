#!/bin/sh

/usr/opensync/tools/ovsh insert Wifi_Inet_Config enabled:=true if_name:=lan_100 if_type:=bridge inet_addr:=192.168.100.1 ip_assign_scheme:=static mtu:=1500 netmask:=255.255.255.0 network:=true vlan_id:=100
