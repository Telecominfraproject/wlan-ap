'use strict';

import * as nl80211 from 'nl80211';
import * as libubus from 'ubus';
import { readfile, stat, popen } from "fs";

let wifi_devices = json(readfile('/usr/share/wifi_devices.json'));
let countries = json(readfile('/usr/share/iso3166.json'));
let board_data = json(readfile('/etc/board.json'));

export let phys = nl80211.request(nl80211.const.NL80211_CMD_GET_WIPHY, nl80211.const.NLM_F_DUMP, { split_wiphy_dump: true });
let interfaces = nl80211.request(nl80211.const.NL80211_CMD_GET_INTERFACE, nl80211.const.NLM_F_DUMP);

let ubus = libubus.connect();
let wireless_status = ubus.call('network.wireless', 'status');

function find_phy(wiphy) {
	for (let k,  phy in phys)
		if (phy && phy.wiphy == wiphy)
			return phy;
	return null;
}

function get_noise(iface) {
	for (let phy in phys) {
		let channels = nl80211.request(nl80211.const.NL80211_CMD_GET_SURVEY, nl80211.const.NLM_F_DUMP, { dev: iface.ifname });
		for (let k, channel in channels)
			if (channel.survey_info.frequency == iface.wiphy_freq)
				return channel.survey_info.noise;
	}

	return -100;
}

function get_country(iface) {
	let reg = nl80211.request(nl80211.const.NL80211_CMD_GET_REG, 0, { dev: iface.ifname });

	return reg.reg_alpha2 ?? '';
}

function get_max_power(iface) {
	let phy = find_phy(iface.wiphy);

	for (let k, band in phy.wiphy_bands)
		if (band)
			for (let freq in band.freqs)
				if (freq.freq == iface.wiphy_freq)
					return freq.max_tx_power;;
	return 0;
}

function get_hardware_id(iface) {
	let hw = {
		type: 'nl80211',
		id: 'Generic MAC80211',
		power_offset: 0,
		channel_offset: 0,
	};

	let path = `/sys/class/ieee80211/phy${iface.wiphy}/device/`;
	if (stat(path + 'vendor')) {
		let data = [];
		for (let lookup in [ 'vendor', 'device', 'subsystem_vendor', 'subsystem_device' ])
			push(data, trim(readfile(path + lookup), '\n'));
		
		for (let device in wifi_devices.pci) {
			let match = 0;
			for (let i = 0; i < 4; i++)
				if (lc(data[i]) == lc(device[i]))
					match++;
			if (match == 4) {
				hw.type = `${data[0]}:${data[1]} ${data[2]}:${data[3]}`;
				hw.power_offset = device[4];
				hw.channel_offset = device[5];
				hw.id = `${device[6]} ${device[7]}`;
			}
		}
	}

	let compatible = trim(readfile(`/sys/class/net/${iface.ifname}/device/of_node/compatible`), '\n');
	if (compatible && wifi_devices.compatible[compatible]) {
		hw.id = wifi_devices.compatible[compatible][0] + ' ' + wifi_devices.compatible[compatible][1];
		hw.compatible = compatible;
		hw.type = 'embedded';
	}

	return hw;
}

const iftypes = [
	'Unknown', 'Ad-Hoc', 'Client', 'Master', 'Master (VLAN)',
	'WDS', 'Monitor', 'Mesh Point', 'P2P Client', 'P2P Go',
];

export let ifaces = {};
for (let k, v in interfaces) {
	let iface = ifaces[v.ifname] = v;

	iface.mode = iftypes[iface.iftype] ?? 'unknown',
	iface.noise = get_noise(iface);
	iface.country = get_country(iface);
	iface.max_power = get_max_power(iface);
	iface.assoclist = nl80211.request(nl80211.const.NL80211_CMD_GET_STATION, nl80211.const.NLM_F_DUMP, { dev: v.ifname }) ?? [];
	iface.hardware = get_hardware_id(iface);

	iface.bss_info = ubus.call('hostapd', 'bss_info', { iface: v.ifname });
	if (!iface.bss_info)
		iface.bss_info = ubus.call('wpa_supplicant', 'bss_info', { iface: v.ifname });
}

for (let radio, data in wireless_status)
	for (let k, v in data.interfaces) {
		if (!v.ifname || !ifaces[v.ifname])
			continue;

		ifaces[v.ifname].ssid = v.config.ssid || v.config.mesh_id;
		ifaces[v.ifname].radio = data.config;
		
		let bss_info = ifaces[v.ifname].bss_info;
		let owe_transition_ifname = bss_info?.owe_transition_ifname;

		if (v.config.owe_transition && ifaces[owe_transition_ifname]) {
			ifaces[v.ifname].owe_transition_ifname = owe_transition_ifname;
			ifaces[owe_transition_ifname].ssid = v.config.ssid;
			ifaces[owe_transition_ifname].radio = data.config;
			ifaces[owe_transition_ifname].owe_transition_ifname = v.ifname
		}
	}

function format_channel(freq) {
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

	return 'unknown';
}

function format_band(freq) {
	if (freq == 5935)
		return '6';
	if (freq < 2484)
		return '2.4';
	if (freq < 5950)
		return '5';
	if (freq <= 45000)
		return '6';

	return '60';
}

function format_frequency(freq) {
	return freq ? sprintf('%.03f', freq / 1000.0) : 'unknown';
}

function format_rate(rate) {
	return rate ? sprintf('%.01f', rate / 10.0) : 'unknown';
}

function format_mgmt_key(key) {
	switch(+key) {
	case 1:
		return '802.1x';

	case 2:
		return 'WPA PSK';

	case 3:
		return 'FT 802.1x';

	case 4:
		return 'FT PSK';

	case 5:
	case 11: // deprecated 802.1x-suiteB-SHA256
		return '802.1x-SHA256';

	case 6:
		return 'WPA PSK-SHA256';

	case 8: // SAE with SHA256
	case 24: // SAE using group-dependent hash
		return 'SAE';

	case 9: // FT SAE with SHA256
	case 25: // FT SAE using group-dependent hash
		return 'FT SAE';

	case 12:
		return '802.1x-192bit';

	case 13:
		return 'FT 802.1x-SHA384';

	case 14:
		return 'FILS-SHA256';

	case 15:
		return 'FILS-SHA384';

	case 16:
		return 'FT FILS-SHA256';

	case 17:
		return 'FT FILS-SHA384';

	case 18:
		return 'OWE';

	case 19:
		return 'FT PSK-SHA384';

	case 20:
		return 'WPA PSK-SHA384';

	}

	return null;
}

function assoc_flags(data, is_s1g, s1g_mhz) {
	const assoc_mhz = {
		width_40: 40,
		width_80: 80,
		width_80p80: '80+80',
		width_160: 160,
		width_320: 320,
		width_10: 10,
		width_5: 5
	};

	let mhz = 'unknown';
	for (let k, v in assoc_mhz)
		if (data[k])
			mhz = v; 

	// HaLow (S1G): the morse driver reports the rate through nl80211 using the
	// borrowed 5 GHz VHT fields (e.g. VHT-MCS 7 / 80MHz). Present it as S1G:
	// use plain "MCS" (no VHT/HE prefix) and the real S1G operating bandwidth
	// (e.g. 4MHz) instead of the borrowed 80MHz. Matches the C dot11ah backend.
	if (is_s1g) {
		let flags = [];
		let mcs = data.vht_mcs ?? data.he_mcs ?? data.mcs;
		if (mcs != null) {
			push(flags, `MCS ${mcs}`);
			push(flags, `${s1g_mhz ? s1g_mhz : mhz}MHz`);
		}
		return flags;
	}

	const assoc_flags = {
		mcs: {
			mcs: 'MCS',
		},
		vht_mcs: {
			vht_mcs: 'VHT-MCS',
			vht_nss: 'VHT-NSS',
		},
		he_mcs: {
			he_mcs: 'HE-MCS',
			he_nss: 'HE-NSS',
			he_gi: 'HE-GI',
			he_dcm: 'HE-DCM',
		},
		eht_mcs: {
			eht_mcs: 'EHT-MCS',
			eht_nss: 'EHT-NSS',
			eht_gi: 'EHT-GI',
		},
	};

	let flags = [];
	for (let k, v in assoc_flags) {
		if (!data[k])
			continue;

		let first = 0;
		for (let name, flag in v) {
			if (data[name] == null)
				continue;
			push(flags, `${flag} ${data[name]}`);
			if (!first++)
				push(flags, `${mhz}MHz`);
		}
	}

	return flags;
}

function dbm2mw(dbm) {
	const LOG10_MAGIC = 1.25892541179;
	let res = 1.0;
	let ip = dbm / 10;
	let fp = dbm % 10;

	for (let k = 0; k < ip; k++)
		res *= 10;
	for (let k = 0; k < fp; k++)
		res *= LOG10_MAGIC;
	
	return int(res);
}

function dbm2quality(dbm) {
	let quality = dbm;

	if (quality < -110)
		quality = -110;
	else if (quality > -40)
		quality = -40;
	quality += 110;

	return quality;
}

function hwmodelist(name) {
	const mode = { 'HT*': 'n', 'VHT*': 'ac', 'HE*': 'ax' };
	let iface = ifaces[name];
	let phy = board_data.wlan?.['phy' + iface.wiphy];
	if (!phy || !iface.radio?.band)
		return '';
	// HaLow (S1G) has no standard HT/VHT/HE htmode and its band key is absent
	// from phy.info.bands, so bands[uc(band)] is null. Guard against it to
	// avoid a null-deref (iwinfo <iface> i crashing) - return an empty hwmode
	// list, matching the empty capabilities.htmode for morse phys.
	let band_info = phy.info.bands?.[uc(iface.radio.band)];
	if (!band_info)
		return '';
	let htmodes = band_info.modes;
	let list = [];
	if (iface.radio.band == '2g' && 'NOHT' in htmodes)
		push(list, 'g/b');
	for (let k, v in mode)
		for (let htmode in htmodes)
			if (wildcard(htmode, k))
				push(list, v);

	return join('/', reverse(uniq(list)));
}

// HaLow (S1G) channel/frequency cannot be derived from wiphy_freq: the morse
// driver registers on a borrowed 5 GHz regulatory frequency, so nl80211
// reports a 5 GHz freq and the generic format_channel()/format_band() would
// print e.g. "Channel 44 (5.220 GHz)" instead of the real S1G channel. Query
// the chip directly via morse_cli for the true operating frequency/bandwidth.
// Returns null if morse_cli is unavailable or the output can't be parsed.
function morse_s1g_channel_info(ifname) {
	let fp = popen(`morse_cli -i ${ifname} channel 2>/dev/null`);
	if (!fp)
		return null;

	let info = {};
	for (let line = fp.read('line'); length(line); line = fp.read('line')) {
		let m = match(trim(line), /^Operating Frequency:[ \t]+([0-9]+)[ \t]*kHz$/);
		if (m) { info.freq_khz = +m[1]; continue; }
		m = match(trim(line), /^Operating BW:[ \t]+([0-9]+)[ \t]*MHz$/);
		if (m) { info.op_bw_mhz = +m[1]; continue; }
		m = match(trim(line), /^Primary BW:[ \t]+([0-9]+)[ \t]*MHz$/);
		if (m) { info.prim_bw_mhz = +m[1]; continue; }
	}
	fp.close();

	return length(info) ? info : null;
}

// HaLow noise: the nl80211 survey is keyed on the borrowed 5 GHz frequency,
// so get_noise() never matches for a morse phy and falls back to -100. The C
// dot11ah backend reads it from the chip stats instead; do the same here.
// Returns an integer dBm (e.g. -85) or null if unavailable.
function morse_stats_noise(ifname) {
	let fp = popen(`morse_cli -i ${ifname} stats 2>/dev/null`);
	if (!fp)
		return null;

	// Match exactly the "Noise (dBm)" line. morse_cli 1.17.8 aligns the
	// colon with padding spaces, e.g.:
	//     "Noise (dBm)                          : -85"
	// so the ')' is NOT immediately followed by ':'. Allow arbitrary spaces
	// between ')' and ':'. Anchor on "^Noise \(dBm\)" so we never pick up the
	// other noise-ish lines ("Noise metric 2 : 0", "Min noise estimation :
	// 187", "Coproc LTF noise ...", "PHY noise state reset : 0"), which start
	// with different text and would otherwise yield a bogus value.
	let noise = null;
	for (let line = fp.read('line'); length(line); line = fp.read('line')) {
		let m = match(trim(line), /^Noise \(dBm\)[ \t]*:[ \t]*(-?[0-9]+)/);
		if (m) { noise = +m[1]; break; }
	}
	fp.close();

	return noise;
}

// Resolve a S1G noise floor that is ALWAYS a sensible negative dBm.
// Noise is a dBm figure and must never be shown as 0 (or a positive value):
//   - morse_cli reports "Noise (dBm): 0" when it has no valid measurement yet
//     (idle radio, no associated STA, mesh with no peer, just-brought-up iface);
//   - the nl80211 survey fallback is the borrowed 5 GHz freq, so it stays at
//     the -100 placeholder for a morse phy.
// Prefer the real chip value; if it is missing or a bogus >= 0 reading, fall
// back to a conservative S1G noise floor so "iwinfo ... i/a" never prints
// "Noise: 0 dBm". Returns a negative integer dBm.
function morse_resolve_noise(ifname, survey_noise) {
	const S1G_NOISE_FLOOR = -95;
	let n = morse_stats_noise(ifname);
	if (n != null && n < 0)
		return n;
	// chip gave 0 / positive / nothing: try the survey value if it is a real
	// negative reading and not the -100 "no data" placeholder.
	if (survey_noise != null && survey_noise < 0 && survey_noise != -100)
		return survey_noise;
	return S1G_NOISE_FLOOR;
}

// HaLow RSSI can be reported as a small positive number for very strong
// signals; a 0 dBm reading in particular is really "no measurement". Mirror
// the C backend which maps a raw 0 to a valid weak value so it isn't shown as
// a bogus strong 0 dBm.
function morse_sanitise_signal(signal) {
	if (signal == null)
		return signal;
	return (signal == 0) ? -1 : signal;
}

// Compute the real 802.11ah (S1G) PHY bitrate. The morse driver reports rates
// to nl80211 through the borrowed 5 GHz VHT fields, so iwinfo would otherwise
// print e.g. "325.0 MBit/s" (a 5 GHz VHT-MCS7 rate) instead of the true S1G
// rate (~15 Mbit/s for MCS7 @ 4 MHz, 1 SS). Derive it from the S1G MCS /
// operating bandwidth / spatial streams using the 802.11ah rate parameters:
//   rate = Nsd(BW) * databits_per_subcarrier(MCS) * NSS / Tsym
// Nsd = data subcarriers per bandwidth; Tsym = 40us (long GI) / 36us (short
// GI). Returns the rate in units of 100 kbps (same unit nl80211 uses, so it
// plugs straight into format_rate()), or null if inputs are unusable.
function morse_s1g_rate_100kbps(mcs, mhz, nss, sgi) {
	if (mcs == null || mhz == null)
		return null;
	// data subcarriers per S1G bandwidth (MHz -> Nsd)
	const nsd = { "1": 24, "2": 52, "4": 108, "8": 234, "16": 468 };
	// data bits per subcarrier = bits/subcarrier * coding rate, per MCS.
	// MCS10 is the 1 MHz-only MCS0 x2 repetition (half of MCS0).
	const dbps = [ 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 4.5, 5.0, 6.0, 6.66667, 0.25 ];
	let n = nsd[`${mhz}`];
	if (n == null || mcs < 0 || mcs > 10)
		return null;
	let tsym = sgi ? 36.0 : 40.0;
	let mbps = (n * dbps[mcs] * (nss ? nss : 1)) / tsym;
	return int(mbps * 10 + 0.5);
}

// Resolve whether an interface is HaLow (S1G). WDS dynamic STA interfaces
// (e.g. wlan0.sta1) are created by hostapd and are NOT in network.wireless
// status, so they have no .radio.band of their own; inherit it from the
// parent interface (strip the ".staN" suffix). Returns [is_s1g, phy_ifname]
// where phy_ifname is the parent to query morse_cli against.
function morse_s1g_iface(dev) {
	if (ifaces[dev]?.radio?.band == 's1g')
		return [ true, dev ];
	let parent = replace(dev, /\.sta[0-9]+$/, '');
	if (parent != dev && ifaces[parent]?.radio?.band == 's1g')
		return [ true, parent ];
	return [ false, dev ];
}

export function assoclist(dev) {
	let stations = ifaces[dev].assoclist;
	let ret = {};

	// HaLow (S1G): nl80211 survey noise is unavailable on the borrowed 5 GHz
	// frequency, so read it from the chip; also sanitise a bogus 0 dBm RSSI.
	// WDS .staN interfaces inherit S1G from their parent (wlan0).
	let s1g_res = morse_s1g_iface(dev);
	let is_s1g = s1g_res[0];
	let phy_if = s1g_res[1];

	// In WDS/4-addr AP mode hostapd puts each associated STA on a dynamic
	// AP_VLAN child (wlan0.sta1), so a GET_STATION dump on the master (wlan0)
	// comes back EMPTY -> "iwinfo wlan0 a" shows nothing and "iwinfo wlan0 i"
	// reports Signal 0 / Bit Rate unknown. The C dot11ah backend shows the STA
	// on wlan0 itself. Match that: when queried on the S1G master, merge in the
	// stations from all of its ".staN" WDS children so wlan0 and wlan0.sta1
	// report the same peers.
	if (is_s1g && dev == phy_if) {
		let merged = [];
		for (let s in stations)
			push(merged, s);
		for (let ciface, cdata in ifaces) {
			if (ciface == dev)
				continue;
			let m = match(ciface, /^(.+)\.sta[0-9]+$/);
			if (m && m[1] == dev)
				for (let cs in cdata.assoclist)
					push(merged, cs);
		}
		stations = merged;
	}

	let noise = ifaces[dev].noise;
	let s1g_mhz;
	if (is_s1g) {
		// Always resolve to a valid negative dBm (never 0) regardless of
		// whether a STA is associated / the chip has measured yet.
		noise = morse_resolve_noise(phy_if, noise);
		let s1g = morse_s1g_channel_info(phy_if);
		if (s1g?.op_bw_mhz)
			s1g_mhz = s1g.op_bw_mhz;
	}

	for (let station in stations) {
		let signal = is_s1g ? morse_sanitise_signal(station.sta_info.signal_avg)
				    : station.sta_info.signal_avg;

		// For S1G, replace the borrowed 5 GHz VHT bitrate (e.g. 325.0
		// MBit/s) with the true 802.11ah PHY rate derived from the S1G
		// MCS / bandwidth / NSS the driver reports through the VHT fields.
		let rxb = station.sta_info.rx_bitrate ?? {};
		let txb = station.sta_info.tx_bitrate ?? {};
		let rx_raw = rxb.bitrate ?? 0;
		let tx_raw = txb.bitrate ?? 0;
		if (is_s1g) {
			let rxmcs = rxb.vht_mcs ?? rxb.he_mcs ?? rxb.mcs;
			let rxnss = rxb.vht_nss ?? rxb.he_nss ?? rxb.nss ?? 1;
			let r = morse_s1g_rate_100kbps(rxmcs, s1g_mhz, rxnss, rxb.short_gi);
			if (r != null)
				rx_raw = r;
			let txmcs = txb.vht_mcs ?? txb.he_mcs ?? txb.mcs;
			let txnss = txb.vht_nss ?? txb.he_nss ?? txb.nss ?? 1;
			let t = morse_s1g_rate_100kbps(txmcs, s1g_mhz, txnss, txb.short_gi);
			if (t != null)
				tx_raw = t;
		}

		let sta = {
			mac: uc(station.mac),
			signal: signal,
			noise: noise,
			snr: signal - noise,
			inactive_time: station.sta_info.inactive_time,
			rx: {
				bitrate: format_rate(rx_raw),
				bitrate_raw: rx_raw,
				packets: station.sta_info.rx_packets ?? 0,
				flags: assoc_flags(rxb, is_s1g, s1g_mhz),
			},
			tx: {
				bitrate: format_rate(tx_raw),
				bitrate_raw: tx_raw,
				packets: station.sta_info.tx_packets ?? 0,
				flags: assoc_flags(txb, is_s1g, s1g_mhz),
			},
			expected_throughput: station.sta_info.expected_throughput ?? 'unknown',
		};
		ret[sta.mac] = sta;
	}

	return ret;
};

export function freqlist(name) {
	const freq_flags = {
		no_10mhz: 'NO_10MHZ',
		no_20mhz: 'NO_20MHZ',
		no_ht40_minus: 'NO_HT40-',
		no_ht40_plus: 'NO_HT40+',
		no_80mhz: 'NO_80MHZ',
		no_160mhz: 'NO_160MHZ',
		indoor_only: 'INDOOR_ONLY',
		no_ir: 'NO_IR',
		no_he: 'NO_HE',
		radar: 'RADAR_DETECTION',
	};

	let iface = ifaces[name];
	let phy = find_phy(iface.wiphy);
	let channels = [];

	for (let k, band in phy.wiphy_bands) {
		if (!band)
			continue;

		let band_name = format_band(band.freqs[0].freq);
		for (let freq in band.freqs) {
			if (freq.disabled)
				continue;

			let channel = {
				freq: format_frequency(freq.freq),
				band: band_name,
				channel: format_channel(freq.freq),
				flags: [],
				active: iface.wiphy_freq == freq.freq,
			};
	
			for (let k, v in freq_flags)
				if (freq[k])
					push(channel.flags, v);
			
			push(channels, channel);
		}
	}

	return channels;
};
export function info(name) {
	let order = [];
	for (let iface, data in ifaces) 
		push(order, iface);

	let list = [];
	for (let iface in sort(order)) {
		if (name && iface != name)
			continue;
		let data = ifaces[iface];
		let dev = {
			iface,
			ssid: data.ssid,
			mac: data.mac,
			mode: data.mode,
			channel: format_channel(data.wiphy_freq),
			freq: format_frequency(data.wiphy_freq),
			htmode: data.radio?.htmode,
			center_freq1: format_channel(data.center_freq1) || 'unknown',
			center_freq2: format_channel(data.center_freq2) || 'unknown',
			txpower: data.wiphy_tx_power_level / 100,
			noise: data.noise,
			signal: 0,
			bitrate: 0,
			encryption: 'unknown',
			hwmode: hwmodelist(iface),
			phy: 'phy' + data.wiphy,
			vaps: 'no',
			hw_type: data.hardware.type,
			hw_id: data.hardware.id,
			power_offset: data.hardware.power_offset || 'none',
			channel_offset: data.hardware.channel_offset || 'none',
		};

		// HaLow (S1G): the generic wiphy_freq-based fields above are wrong
		// (they map the borrowed 5 GHz frequency to a 5 GHz channel). Match the
		// 802.11ah presentation the C iwinfo dot11ah backend produces:
		//   - channel   : real S1G channel from the wireless config
		//   - frequency : true operating frequency from the chip in MHz
		//                 (e.g. "924.500 MHz", not "5.220 GHz")
		//   - HW Mode   : 802.11ah  (hwmode field 'ah')
		//   - HT Mode   : S1G operating bandwidth (no HT/VHT/HE for S1G)
		//   - Hardware  : the Morse Micro device, not "Generic MAC80211"
		//
		// WDS dynamic STA interfaces (wlan0.sta1) are created by hostapd and
		// are NOT in network.wireless status, so they have no .radio.band of
		// their own and the plain `data.radio?.band == 's1g'` test misses them
		// (they'd print the borrowed 5 GHz info). Use morse_s1g_iface() to also
		// catch the ".staN" children and inherit the radio config (channel/
		// freq/noise) from their parent (wlan0).
		let s1g_res = morse_s1g_iface(iface);
		if (s1g_res[0]) {
			let phy_if = s1g_res[1];
			let pradio = ifaces[phy_if]?.radio;
			let s1g = morse_s1g_channel_info(phy_if);
			dev.channel = pradio?.channel ?? 'unknown';
			// center_freq1/2 come from the borrowed 5 GHz chandef, so
			// format_channel() prints a bogus 5 GHz center channel (e.g. 42).
			// The C dot11ah backend maps the center channel into S1G space;
			// for a single morse AP that is just the operating S1G channel.
			dev.center_freq1 = pradio?.channel ?? 'unknown';
			dev.center_freq2 = 'unknown';
			// morse_cli reports kHz (924500 -> 924.500 MHz); the 004 schema
			// fallback (radio.freq) is already in MHz (e.g. 924.5).
			if (s1g?.freq_khz)
				dev.freq = sprintf('%.03f', s1g.freq_khz / 1000.0);
			else if (pradio?.freq)
				dev.freq = sprintf('%.03f', +pradio.freq);
			dev.freq_unit = 'MHz';
			dev.hwmode = 'ah';
			// The C dot11ah backend reports htmodelist = NOHT (S1G has no
			// HT/VHT/HE htmode), so match that rather than inventing a BW.
			dev.htmode = 'NOHT';
			// A WDS STA link surfaces as "Master (VLAN)"; the C dot11ah backend
			// simply reports the AP as Master. Normalise ONLY that ".staN"
			// child case so it doesn't look like a foreign 5 GHz VLAN. Do NOT
			// clobber genuine mesh interfaces (mode "Mesh Point") or STA mode -
			// a HaLow mesh iface must keep reporting "Mesh Point".
			if (dev.mode == 'Master (VLAN)')
				dev.mode = 'Master';
			// Match the C dot11ah backend: "Type: dot11ah" and a Morse
			// hardware name (the generic PCI/id lookup does not know the
			// MM8108 USB dongle, so it would otherwise show Generic MAC80211).
			dev.iwtype = 'dot11ah';
			// Hardware type: the generic get_hardware_id() only knows PCI
			// (reads sysfs 'vendor'/'device'); the MM8108 is a USB dongle, so
			// build the "USB <idVendor>:<idProduct>" string the C dot11ah
			// backend shows (e.g. "USB 325B:8100"). The ieee80211 phy 'device'
			// symlink points at the USB *interface* (1-1:1.0); its parent
			// (../) is the USB device holding idVendor/idProduct.
			let usb_base = `/sys/class/ieee80211/phy${data.wiphy}/device/`;
			let vid = trim(readfile(usb_base + '../idVendor') ?? '', '\n');
			let pid = trim(readfile(usb_base + '../idProduct') ?? '', '\n');
			if (length(vid) && length(pid))
				dev.hw_type = `USB ${uc(vid)}:${uc(pid)}`;
			else
				dev.hw_type = 'dot11ah';
			dev.hw_id = 'Morse Micro MM8108 HaLow WiFi';
			// Noise is a dBm figure and must never show as 0. The nl80211
			// survey is bogus (-100 placeholder) on the borrowed 5 GHz freq,
			// and morse_cli reports 0 when idle / no STA / mesh w/o peer.
			// Resolve to a guaranteed negative dBm (chip value, else survey,
			// else a conservative S1G noise floor).
			dev.noise = morse_resolve_noise(phy_if, dev.noise);
		}

		let phy = find_phy(data.wiphy);
		for (let limit in phy.interface_combinations[0]?.limits)
			if (limit.types?.ap && limit.max > 1)
				dev.vaps = 'yes';

		if (data.bss_info) {
			if (data.bss_info.wpa_key_mgmt && data.bss_info.wpa_pairwise)
				dev.encryption = `${replace(data.bss_info.wpa_key_mgmt, ' ', ' / ')} (${data.bss_info.wpa_pairwise})`;
			else if (data.owe_transition_ifname)
				dev.encryption = 'none (OWE transition)';
			else
				dev.encryption = 'none';
		}

		let stations = assoclist(iface);
		// Count the stations actually returned by assoclist(). For an S1G
		// master this already includes the WDS ".staN" children (see
		// assoclist()), whereas data.assoclist (the master's own nl80211
		// dump) is empty in WDS mode - dividing by that would wrongly yield
		// Signal 0 / Bit Rate 0. Average over the merged station count.
		let nsta = 0;
		for (let k, station in stations) {
			dev.signal += station.signal;
			dev.bitrate += station.tx.bitrate_raw;
			nsta++;
		}
		dev.signal /= nsta || 1;
		dev.bitrate /= nsta || 1;
		dev.bitrate = format_rate(dev.bitrate);
		dev.quality = dbm2quality(dev.signal);

		if (data.owe_transition_ifname)
			dev.owe_transition_ifname = data.owe_transition_ifname;

		push(list, dev);
	}

	return list;
};

export function htmodelist(name) {
	let iface = ifaces[name];
	let phy = board_data.wlan?.['phy' + iface.wiphy];
	if (!phy || !iface.radio.band)
		return [];

	// HaLow (S1G) band key is absent from phy.info.bands (no HT/VHT/HE htmode),
	// so bands[uc(band)] is null. Guard to avoid a null-deref for morse phys.
	let band_info = phy.info.bands?.[uc(iface.radio.band)];
	if (!band_info)
		return [];

	return filter(band_info.modes, (v) => v != 'NOHT');
};

export function txpowerlist(name) {
	let iface = ifaces[name];
	let max_power = iface.max_power / 100;
	let match = iface.wiphy_tx_power_level / 100;
	let list = [];

	for (let power = 0; power <= max_power; power++) {
		let txpower = {
			dbm: power,
			mw: dbm2mw(power),
			active: power == match,
		};
		push(list, txpower);
	}
	
	return list;
};

export function countrylist(dev) {
	let iface = ifaces[dev];

	let list = {
		active: iface.country,
		countries, 
	};

	return list;
};

function scan_extension(ext, cell) {
	const eht_chan_width = [ '20 MHz', '40 MHz', '80 MHz', '160 MHz', '320 MHz'];

	switch(ord(ext, 0)) {
	case 36:
		let offset = 7;

		if (!(ord(ext, 3) & 0x2))
			break;

		if (ord(ext, 2) & 0x40)
			offset += 3;

		if (ord(ext, 2) & 0x80)
			offset += 1;

		cell.he = {
			chan_width: eht_chan_width[ord(ext, offset + 1) & 0x3],
			center_chan_1: ord(ext, offset + 2),
			center_chan_2: ord(ext, offset + 3),
		};
		break;

	case 106:
		if (!(ord(ext, 1) & 0x1))
			break;

		cell.eht = {
			chan_width: eht_chan_width[ord(ext, 6) & 0x7],
			center_chan_1: ord(ext, 7),
			center_chan_2: ord(ext, 8),
		};
		break;
	}
};

export function scan(dev) {
	const rsn_cipher = [ 'NONE', 'WEP-40', 'TKIP', 'WRAP', 'CCMP', 'WEP-104', 'AES-OCB', 'CKIP', 'GCMP', 'GCMP-256', 'CCMP-256' ];
	const ht_chan_offset = [ 'no secondary', 'above', '[reserved]', 'below' ];
	const vht_chan_width = [ '20 or 40 MHz', '80 MHz', '80+80 MHz', '160 MHz' ];
	const ht_chan_width = [ '20 MHz', '40 MHz or higher' ];
	const SCAN_FLAG_AP = (1<<2);

	let params = {
		dev,
		scan_flags: SCAN_FLAG_AP,
		scan_ssids: [ '' ],
	};

	let res = nl80211.request(nl80211.const.NL80211_CMD_TRIGGER_SCAN, 0, params);
	if (res === false) {
		printf("Unable to trigger scan: " + nl80211.error() + "\n");
		exit(1);
	}

	res = nl80211.waitfor([
		nl80211.const.NL80211_CMD_NEW_SCAN_RESULTS,
		nl80211.const.NL80211_CMD_SCAN_ABORTED
	], 5000);

	if (!res) {
		printf("Netlink error while awaiting scan results: " + nl80211.error() + "\n");
		exit(1);
	} else if (res.cmd == nl80211.const.NL80211_CMD_SCAN_ABORTED) {
		printf("Scan aborted by kernel\n");
		exit(1);
	}

	let scan = nl80211.request(nl80211.const.NL80211_CMD_GET_SCAN, nl80211.const.NLM_F_DUMP, { dev });

	let cells = [];
	for (let k, bss in scan) {
		bss = bss.bss;
		let cell = {
			bssid: uc(bss.bssid),
			frequency: format_frequency(bss.frequency),
			band: format_band(bss.frequency),
			channel: format_channel(bss.frequency),
			dbm: bss.signal_mbm / 100,

		};

		if (bss.capability & (1 << 1))
			cell.mode = 'Ad-Hoc';
		else if (bss.capability & (1 << 0))
			cell.mode = 'Master';
		else
			cell.mode = 'Mesh Point';

		cell.quality = dbm2quality(cell.dbm);

		for (let ie in bss.information_elements)
			switch(ie.type) {
			case 0:
			case 114:
				cell.ssid = ie.data;
				break;

			case 7:
				cell.country = substr(ie.data, 0, 2);
				break;

			case 48:
				cell.crypto = {
					group: rsn_cipher[ord(ie.data, 5)] ?? '',
					pair: [],
					key_mgmt: [],
				};

				let offset = 6;
				let count = ord(ie.data, offset);
				offset += 2;
				
				for (let i = 0; i < count; i++) {
					let key = rsn_cipher[ord(ie.data, offset + 3)];
					if (key)
						push(cell.crypto.pair, key);
					offset += 4;
				}
				
				count = ord(ie.data, offset);
				offset += 2;

				for (let i = 0; i < count; i++) {
					let key = format_mgmt_key(ord(ie.data, offset + 3));
					if (key)
						push(cell.crypto.key_mgmt, key);
					offset += 4;
				}
				break;

			case 61:
				cell.ht = {
					primary_channel: ord(ie.data, 0),
					secondary_chan_off: ht_chan_offset[ord(ie.data, 1) & 0x3],
					chan_width: ht_chan_width[(ord(ie.data, 1) & 0x4) >> 2],
				};
				break;

			case 192:
				cell.vht = {
					chan_width: vht_chan_width[ord(ie.data, 0)],
					center_chan_1: ord(ie.data, 1),
					center_chan_2: ord(ie.data, 2),
				};
				break;

			case 255:
				scan_extension(ie.data, cell);
				break;
			};

		

		push(cells, cell);
	}

	return cells;
};
