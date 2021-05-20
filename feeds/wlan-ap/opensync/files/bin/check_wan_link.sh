#!/bin/sh

if="$(uci get network.wan.ifname)"
[ "$(cat /sys/class/net/"${if}"/carrier)" = 0 ] && {
	return 0
}
return 1
