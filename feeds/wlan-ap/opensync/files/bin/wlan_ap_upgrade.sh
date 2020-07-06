#!/bin/sh

url=$1
[ -z "$url" ] && url=http://192.168.178.9/openwrt-ipq40xx-generic-linksys_ea8300-squashfs-sysupgrade.bin

/usr/opensync/tools/ovsh update AWLAN_Node firmware_url:="$url"  > /dev/null
/usr/opensync/tools/ovsh update AWLAN_Node upgrade_timer:=30  > /dev/null
