#!/bin/sh

append DRIVERS "mac80211"

detect_mac80211() {
	[ -e /tmp/.config_pending ] && return
	ucode /usr/share/hostap/wifi-detect.uc
	ucode /lib/wifi/mac80211.uc | uci -q batch
}
