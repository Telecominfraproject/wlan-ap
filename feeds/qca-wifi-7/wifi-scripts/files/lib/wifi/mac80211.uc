#!/usr/bin/env ucode

/*
 * This has been copied from https://github.com/openwrt/openwrt which is under GPL-2.0-only license
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

import { readfile } from "fs";
import * as uci from 'uci';

const bands_order = [ "6G", "5G", "2G" ];
const htmode_order = [ "EHT", "HE", "VHT", "HT" ];

let board = json(readfile("/etc/board.json"));
if (!board.wlan)
	exit(0);

let idx = 0;
let commit;
let single_wiphy = false;

let config = uci.cursor().get_all("wireless") ?? {};

print(`set wireless.mac80211=smp_affinity
set wireless.mac80211.enable_smp_affinity='1'
set wireless.mac80211.enable_color='1'
`);

function freq_to_channel(freq) {
	if (freq < 1000)
		return 0;
	if (freq == 2484)
		return 14;
	if (freq == 5935)
		return 2;
	if (freq < 2484)
		return (freq - 2407) / 5;
	if (freq >= 4910 && freq <= 4980)
		return (freq - 4000) / 5;
	if (freq < 5950)
		return (freq - 5000) / 5;
	if (freq <= 45000)
		return (freq - 5950) / 5;
	if (freq >= 58320 && freq <= 70200)
		return (freq - 56160) / 2160;
	return 0;
}

function radio_exists(path, macaddr, phy) {
	for (let name, s in config) {
		if (s[".type"] != "wifi-device")
			continue;
		if (s.macaddr & lc(s.macaddr) == lc(macaddr))
			return true;
		if (s.phy == phy)
			return true;
		if (!s.path || !path)
			continue;
		if (substr(s.path, -length(path)) == path)
			return true;
	}
}

function get_band(freq) {
	if (freq > 50000)
		band_name = "60G";
	else if (freq > 5900)
		band_name = "6G";
	else if (freq > 4000)
		band_name = "5G";
	else if (freq > 2000)
		band_name = "2G";
	return band_name;
}

function get_channel_list(start_freq, end_freq) {
	channels = freq_to_channel(start_freq) + "-" + freq_to_channel(end_freq);
	return channels;
}

function generate_config(info, name, single_wiphy, id, radio_idx) {
	let s = "wireless." + name;
	let si = "wireless.default_" + name;

	let band_name;
	if (!single_wiphy) {
		band_name = filter(bands_order, (b) => info.bands[b])[0];
	} else {
		let freq = radio_idx.first_freq;
		band_name = get_band(freq);
	}

	if (!band_name)
		return;

	let band = info.bands[band_name];
	let channel = band.default_channel;

	if (single_wiphy) {
		let start_freq = radio_idx.first_freq;
		let end_freq = radio_idx.last_freq;
		channels = get_channel_list(start_freq, end_freq);
		if (band_name == "6G") {
			let start_freq = radio_idx.first_freq;
			if (freq_to_channel(start_freq) >= 129)
				channel = 197;
			else
				channel = 49;
		}
		if (band_name == "5G") {
			let start_freq = radio_idx.first_freq;
			if (freq_to_channel(start_freq) == 36)
				channel = 36;
			else
				channel = 149;
		}
	} else {
		if (band_name == "5G") {
		       if (band.default_channel == "100")
				channel = 149;
			else
				channel = 36;
		}
		if (band_name == "6G") {
			if (band.default_channel == "129")
				channel = 197;
			else
				channel = 49;
		}
	}

	if (band_name == "2G")
		channel = 6;

	let width = band.max_width;
	if (band_name == "2G")
		width = 20;
	else if (width > 80)
		width = 80;

	let htmode = filter(htmode_order, (m) => band[lc(m)])[0];
	if (htmode)
		htmode += width;
	else
		htmode = "NOHT";

	let country, num_global_macaddr, macaddr_base;
	if (board.wlan.defaults) {
		country = board.wlan.defaults.country;
		num_global_macaddr = board.wlan.defaults.ssids?.[lc(band_name)]?.mac_count;
		macaddr_base = board.wlan.defaults.ssids?.[lc(band_name)]?.macaddr_base;
	}

	print(`set ${s}=wifi-device
set ${s}.type='mac80211'
set ${s}.${id}
set ${s}.band='${lc(band_name)}'
set ${s}.channel='${channel}'
set ${s}.country='${country || ''}'
set ${s}.macaddr_base='${macaddr_base || ''}'
set ${s}.num_global_macaddr='${num_global_macaddr || ''}'
`);

if (radio_idx != null) {
	print(`set ${s}.radio='${radio_idx.idx}'
`);
}

if (channels)
	print(`set ${s}.channels='${channels}'`);

print(`
set ${s}.htmode='${htmode}'
set ${s}.disabled='1'

set ${si}=wifi-iface
set ${si}.device='${name}'
set ${si}.network='lan'
set ${si}.mode='ap'
set ${si}.ssid='OpenWrt'
set ${si}.encryption='none'

`);
	if (band_name == "6G") {
		print(`set ${si}.encryption='sae'
		set ${si}.sae_pwe='1'
		set ${si}.key='0123456789'
`);
	}

}

for (let phy_name, phy in board.wlan) {
	let info = phy.info;
	let name;
	if (!info || !length(info.bands))
		continue;

	if (!phy.path)
		return;

	let macaddr = trim(readfile(`/sys/class/ieee80211/${phy_name}/macaddress`));
	if (radio_exists(phy.path, macaddr, phy_name)) {
		idx++;
		continue;
	}

	id = `phy='${phy_name}'`;
	if (match(phy_name, /^phy[0-9]/))
		id = `path='${phy.path}'`;

	if (phy.multi_radio)
		single_wiphy = true;

	if (single_wiphy) {
		let multi_radio = phy.multi_radio;
		let hw_idx;
		for (let radio_name in multi_radio ) {
			let radio_idx = multi_radio[radio_name];
			name = "radio" + idx + "_band" + hw_idx++;
			generate_config(info, name, single_wiphy, id, radio_idx);
		}
	} else {
		name = "radio" + idx;
		generate_config(info, name, single_wiphy, id, NULL);
	}
	idx++;
	commit = true;
}

if (commit)
	print("commit wireless\n");
