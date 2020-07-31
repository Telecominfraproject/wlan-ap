#!/bin/sh

/usr/opensync/tools/ovsh insert Wifi_Inet_Config NAT:=false enabled:=true if_name:=lan_100 if_type:=eth inet_addr:=192.168.100.1 ip_assign_scheme:=static mtu:=1500 netmask:=255.255.255.0 network:=true vlan_id:=100 parent_ifname:=lan dhcpd:="[\"map\",[[\"start\",\"100\"], [\"stop\",\"250\"], [\"lease_time\",\"1h\"]]]"
/usr/opensync/tools/ovsh insert Wifi_Inet_Config NAT:=true enabled:=true if_name:=wan_100 if_type:=eth ip_assign_scheme:=dhcp mtu:=1500 network:=true vlan_id:=100 parent_ifname:=wan

ubus call osync-wm dbg_add_vif '{"radio":"radio1", "name":"test", "vid":100, "network":"lan", "ssid":"test"}'
