#!/usr/bin/ucode

let nl = require("nl80211");
let rtnl = require("rtnl");
let def = nl.const;
let fs = require("fs");
let uci = require("uci");
let cursor = uci.cursor();
let ubus = require("ubus");
let ctx = ubus.connect();
let uloop = require("uloop");

const SCAN_FLAG_AP = (1<<2);
const frequency_list_2g = [ 2412, 2417, 2422, 2427, 2432, 2437, 2442,
			  2447, 2452, 2457, 2462, 2467, 2472, 2484 ];
const frequency_list_5g = { '3': [ 5180, 5260, 5500, 5580, 5660, 5745 ],
			  '2': [ 5180, 5220, 5260, 5300, 5500, 5540,
				 5580, 5620, 5660, 5745, 5785, 5825,
				 5865, 5920, 5960 ],
			  '1': [ 5180, 5200, 5220, 5240, 5260, 5280,
				 5300, 5320, 5500, 5520, 5540, 5560,
				 5580, 5600, 5620, 5640, 5660, 5680,
				 5700, 5720, 5745, 5765, 5785, 5805,
				 5825, 5845, 5865, 5885 ],
};
const frequency_offset = { '80': 30, '40': 10 };
const frequency_width = { '80': 3, '40': 2, '20': 1 };
const IFTYPE_STATION = 2;
const IFTYPE_AP = 3;
const IFTYPE_MESH = 7;
const IFF_UP = 1;

function frequency_to_channel(freq) {
	/* see 802.11-2007 17.3.8.3.2 and Annex J */
	if (freq == 2484)
		return 14;
	else if (freq < 2484)
		return (freq - 2407) / 5;
	else if (freq >= 4910 && freq <= 4980)
		return (freq - 4000) / 5;
	else if (freq < 5935) /* DMG band lower limit */
		return (freq - 5000) / 5;
	else if (freq == 5935)
		return 2;
	else if (freq >= 5955 && freq <= 7115)
		return ((freq - 5955) / 5) + 1;
	else if (freq >= 58320 && freq <= 64800)
		return (freq - 56160) / 2160;
	return 0;
}

function iface_get(wdev) {
	let params = { dev: wdev };
	let res = nl.request(def.NL80211_CMD_GET_INTERFACE, wdev ? null : def.NLM_F_DUMP, wdev ? params : null);

	if (res === false)
		warn("Unable to lookup interface: " + nl.error() + "\n");
	return res || [];
}

function iface_find(wiphy, types, ifaces) {
	if (!ifaces)
		ifaces = iface_get();
	for (let iface in ifaces) {
		if (iface.wiphy != wiphy)
			continue;
		if (iface.iftype in types)
			return iface;
	}
	return;
}

function scan_trigger(wdev, frequency, width) {
	let params = { dev: wdev, scan_flags: SCAN_FLAG_AP };

	if (frequency && type(frequency) == 'array') {
		params.scan_frequencies = frequency;
	}
	else if (frequency && width) {
		params.wiphy_freq = frequency;
		params.center_freq1 = frequency + frequency_offset[width];
		params.channel_width = frequency_width[width];
	}

	params.scan_ssids = [ '' ];

	let res = nl.request(def.NL80211_CMD_TRIGGER_SCAN, 0, params);

	if (res === false)
		die("Unable to trigger scan: " + nl.error() + "\n");

	else
		res = nl.waitfor([
			def.NL80211_CMD_NEW_SCAN_RESULTS,
			def.NL80211_CMD_SCAN_ABORTED
		], (frequency && width) ? 500 : 5000);

	if (!res)
		warn("Netlink error while awaiting scan results: " + nl.error() + "\n");

	else if (res.cmd == def.NL80211_CMD_SCAN_ABORTED)
		warn("Scan aborted by kernel\n");
}

function trigger_scan_width(wdev, freqs, width) {
	for (let freq in freqs)
		scan_trigger(wdev, freq, width);
}

function phy_get(wdev) {
	let res = nl.request(def.NL80211_CMD_GET_WIPHY, def.NLM_F_DUMP, { split_wiphy_dump: true });

	if (res === false)
		warn("Unable to lookup phys: " + nl.error() + "\n");

	return res;
}

function phy_get_frequencies(phy) {
	let freqs = [];

	for (let band in phy.wiphy_bands) {
		for (let freq in band?.freqs || [])
			if (!freq.disabled)
				push(freqs, freq.freq);
	}
	return freqs;
}

function phy_frequency_dfs(phy, curr) {
	let freqs = [];

	for (let band in phy.wiphy_bands) {
		for (let freq in band?.freqs || [])
			if (freq.freq == curr && freq.dfs_state >= 0)
				return true;
	}
	return false;
}

function ascii_to_lower(str) {
	let res = "";
	let len = length(str);

	for (let i = 0; i < len; i++) {
		let code = ord(str, i);
		if (code >= 65 && code <= 90)
			res += sprintf("%c", code + 32);
		else
			res += sprintf("%c", code);
	}
	return res;
}

function extract_oui_lower(mac) {
	if (type(mac) != "string")
		return "";

	let parts = split(mac, ":");
	if (length(parts) != 6)
		return "";

	let oui = parts[0] + parts[1] + parts[2];
	return ascii_to_lower(oui);
}

function load_rogueap_rules() {
	let result = {
		interval: 30,
		override_dfs: false,
		rules: []
	};

	try {
		cursor.load("rogueap");
		cursor.foreach("rogueap", "config", (section) => {
			if (section.interval)
				result.interval = int(section.interval);

			if (section.override_dfs)
				result.override_dfs = int(section.override_dfs);
		});

		cursor.foreach("rogueap", "rules", (section) => {
			let rule = {};
			if (section.ssid)
				rule.ssid = section.ssid;

			if (section.rssi) {
				if (int(section.rssi) >= -100 && int(section.rssi) <= 0)
					rule.rssi = int(section.rssi);
				else
					warn("Invalid RSSI value in rule: " + section.rssi + ", ignoring rssi\n");
			}

			if (section.bssid)
				rule.bssid = ascii_to_lower(section.bssid);

			if (section.vendor)
				rule.vendor = ascii_to_lower(section.vendor);

			if (length(rule) > 0)
				push(result.rules, rule);
		});
	} catch(e) {
		warn("Failed to load rogueap rules: " + e + "\n");
	}
	return result;
}

let action_running = false;
let first_start = true;
let ifaces = iface_get();
let rogue_ap_cfg = load_rogueap_rules();
rogue_ap_cfg.interval *= 60000;

function intersect(list, filter) {
	let res = [];

	for (let item in list)
		if (index(filter, item) >= 0)
			push(res, item);
	return res;
}

function wifi_scan() {
	let scan = [];
	let phys = phy_get();

	for (let phy in phys) {
		let iface = iface_find(phy.wiphy, [ IFTYPE_STATION, IFTYPE_AP ], ifaces);
		let scan_iface = false;
		if (!iface) {
			warn("no valid interface found for phy" + phy.wiphy + "\n");
			nl.request(def.NL80211_CMD_NEW_INTERFACE, 0, { wiphy: phy.wiphy, ifname: 'scan', iftype: IFTYPE_STATION });
			nl.waitfor([ def.NL80211_CMD_NEW_INTERFACE ], 1000);
			scan_iface = true;
			iface = {
				dev: 'scan',
				channel_width: 1,
			};
			rtnl.request(rtnl.const.RTM_NEWLINK, 0, { dev: 'scan', flags: IFF_UP, change: 1});
			sleep(1000);
		}

		warn("scanning on phy" + phy.wiphy + "\n");

		let freqs = phy_get_frequencies(phy);
		if (length(intersect(freqs, frequency_list_2g)))
			scan_trigger(iface.dev, frequency_list_2g);

		let ch_width = iface.channel_width;
		let freqs_5g = intersect(freqs, frequency_list_5g[ch_width]);
		if (length(freqs_5g)) {
			if (rogue_ap_cfg.override_dfs && !scan_iface && phy_frequency_dfs(phy, iface.wiphy_freq)) {
				ctx.call(sprintf('hostapd.%s', iface.dev), 'switch_chan', { freq: 5180, bcn_count: 10 });
				sleep(2000)
			}
			trigger_scan_width(iface.dev, freqs_5g, ch_width);
		}

		let res = nl.request(def.NL80211_CMD_GET_SCAN, def.NLM_F_DUMP, { dev: iface.dev });
		if (res === false) {
			warn("Unable to get scan results for " + iface.dev + ": " + nl.error() + "\n");
			if (scan_iface) {
				warn("removing temporary interface\n");
				nl.request(def.NL80211_CMD_DEL_INTERFACE, 0, { dev: 'scan' });
			}
			continue;
		}

		for (let bss in res) {
			bss = bss.bss;
			let ssid = "";
			let bssid = bss.bssid;
			let rssi = +bss.signal_mbm / 100;
			let frequency = +bss.frequency;
			let channel = frequency_to_channel(+bss.frequency);
			let vendor = "";

			for (let ie in bss.beacon_ies) {
				if (ssid && vendor)
					break;

				if (ie.type == 0) {
					ssid = ie.data;
				} else if (ie.type == 0xdd && !vendor) {
					let oui = ascii_to_lower(hexenc(substr(ie.data, 0, 3)));
					if (oui !== "0050f2")
						vendor = oui;
				}
			}

			if (vendor === "")
				vendor = extract_oui_lower(bss.bssid);

			/*Blacklist (ssid, bssid, rssi threshold).
			  Whitelist (vendor).*/
			let is_rogueap = false;
			for (let f in rogue_ap_cfg.rules) {
				let match = false;

				if (f.ssid !== undefined) {
					if (ssid === f.ssid)
						match = true;
				} else if (f.bssid !== undefined) {
					if (bssid && bssid === f.bssid)
						match = true;
				} else if (f.rssi !== undefined) {
					if (rssi !== undefined && rssi < f.rssi)
						match = true;
				} else if (f.vendor !== undefined) {
					if (vendor && vendor !== f.vendor)
						match = true;
				}

				if (match) {
					is_rogueap = true;
					break;
				}
			}

			let filtered_res = {
				ssid: ssid,
				bssid: bssid,
				rssi: rssi,
				vendor: vendor,
				channel: channel,
				frequency: frequency,
				rogue_ap: is_rogueap
			};
			push(scan, filtered_res);
		}
		if (scan_iface) {
			warn("removing temporary interface\n");
			nl.request(def.NL80211_CMD_DEL_INTERFACE, 0, { dev: 'scan' });
		}
	}
	return scan;
}

function is_interface_up(dev) {
	let operstate_raw = fs.readfile("/sys/class/net/" + dev + "/operstate");
	if (operstate_raw === false) {
		warn("Cannot get " + dev + " operstate\n");
		return false;
	}

	let state = trim(operstate_raw);
	if (state != "up" && state != "UP") {
		warn(dev + " operstate:" + state + "\n");
		return false;
	}

	let wireless_iface_status = ctx.call("hostapd." + dev, "get_status");
	if (!wireless_iface_status || wireless_iface_status.status != "ENABLED") {
		warn("interface:" + dev + " status is disabled\n");
		return false;
	}
	return true;
}

function wait_wireless_config_ready() {
	let conf = "/etc/config/wireless";
	let last_mtime = 0;
	let stable = 0;

	/*Ensure that the wireless configuration file is fully written,
	  then wait 3 seconds for it to load.*/
	while (stable < 3) {
		let cur = fs.stat(conf)?.mtime || 0;
		if (cur == last_mtime) {
			stable++;
		} else {
			stable = 0;
			last_mtime = cur;
		}

		if (stable >= 3) {
			warn("wireless config check completed\n");
			break;
		}

		sleep(1000);
	}

	for (let i = 0; i < 30; i++) {
		let status= ubus.call('network.wireless', 'status');
		if (!status) {
			warn("get wireless status fail\n");
			sleep(1000);
			continue;
		}

		/*Check radios status and get expected ifaces to load.*/
		let pending = false;
		let expected = [];
		for (let name, radio in status) {
			if (radio?.pending)
				pending = true;

			if (radio?.interfaces)
				for (let iface in radio.interfaces)
					push(expected, iface.ifname);
		}

		if (pending || length(expected) == 0) {
			warn("Radio not ready, retrying\n");
			sleep(1000);
			continue;
		}

		for (let j = 0; j < length(expected); j++)
			expected[j] = replace(expected[j] || "", /^\s+|\s+$/, '');
		sort(expected);

		let uniq_expected = [];
		for (let j = 0; j < length(expected); j++)
			if (expected[j] != "" && (j == 0 || expected[j] != expected[j-1]))
				push(uniq_expected, expected[j]);
		expected = uniq_expected;

		/*Fetch the kernel-registered interface count and compare to the expected count.*/
		let nl_ifaces = iface_get();
		let actual = [];
		if (length(nl_ifaces) > 0) {
			for(let k = 0; k < length(nl_ifaces); k++) {
				let iface = nl_ifaces[k];
				let name = iface.dev;
				if (name) {
					name = replace(name, /^\s+|\s+$/, '');
					if (name != "")
						push(actual, name);
				}
			}
		} else {
			warn("iface_get returned no entries, retrying\n");
			sleep(1000);
			continue;
		}

		sort(actual);
		let uniq_actual = [];
		for (let i = 0; i < length(actual); i++)
			if (i == 0 || actual[i] != actual[i-1])
				push(uniq_actual, actual[i]);
		actual = uniq_actual;

		/*Compare interface counts.*/
		let match = false;
		if (length(expected) > 0 && length(expected) == length(actual)) {
			match = true;
			for (let ei = 0; ei < length(expected); ei++) {
				let found = false;
				for (let ai = 0; ai < length(actual); ai++) {
					if (expected[ei] == actual[ai]) {
						found = true;
						break;
					}
				}

				if (!found) {
					match = false;
					break;
				}
			}
		}

		if (match) {
			warn("The wireless settings are ready\n");
			return true;
		}

		sleep(1000);
	}
	return false;
}

function wait_wireless_ready(done_cb) {
	let idx = 0;
	let tries = 0;
	ifaces = iface_get();

	if (!done_cb)
		done_cb = function(){};

	if (length(ifaces) == 0) {
		warn("iface get fail, retrying\n");
		done_cb(false);
		return;
	}

	uloop.timer(0, function() {
		if (idx >= length(ifaces)) {
			ifaces = iface_get();
			done_cb(true);
			return;
		}

		let iface = ifaces[idx];
		if (is_interface_up(iface.dev)) {
			warn("wireless iface: " + iface.dev + ", is up\n");
			idx++;
			tries = 0;
		} else {
			tries++;
			if (tries >= 120) {
				warn("wireless iface: " + iface.dev + " not up after timeout\n");
				done_cb(false);
				return;
			}
		}

		this.set(1000);
	});
}

function handle_sigterm() {
	warn("Received SIGTERM, exiting...\n");
	action_running = false;
	uloop.end();
}

function action_start(done_cb_if_ready) {
	if (action_running) {
		warn("action_start already running, skipping this tick\n");
		if (done_cb_if_ready)
			done_cb_if_ready(false);
		return;
	}
	action_running = true;

	let ready_flag = false;
	let ucentral_state = ctx.call("ucentral", "status");
	if (ucentral_state && ucentral_state.connected != null)
		ready_flag = true;

	if (!ready_flag) {
		warn("ucentral service is not connected, try again later\n");
		action_running = false;
		if (done_cb_if_ready)
			done_cb_if_ready(false);
		return;
	}
	warn("ucentral service is connected\n");

	if (first_start) {
		first_start = false;
		let cfg_complete = wait_wireless_config_ready();
		if (!cfg_complete) {
			warn("wireless configure failed, try again later\n");
			action_running = false;
			if (done_cb_if_ready)
				done_cb_if_ready(false);
			return;
		}
	}

	wait_wireless_ready(function(wireless_check_success) {
		if (!action_running) {
			warn("action cancelled before scan\n");
			if (done_cb_if_ready)
				done_cb_if_ready(false);
			return;
		}

		if (!wireless_check_success) {
			warn("wireless interfaces not ready, will retry later\n");
			action_running = false;
			if (done_cb_if_ready)
				done_cb_if_ready(false);
			return;
		}

		let scan = wifi_scan();
		ctx.call('ucentral', 'send', {
			method: 'rogue_ap',
			params: { data: scan }
		});

		action_running = false;
		if (done_cb_if_ready)
			done_cb_if_ready(true);
	});
}

uloop.init();
uloop.signal("SIGTERM", handle_sigterm);
uloop.timer(0, function run() {
	let self = this;
	action_start(function(success) {
		let next = success ? rogue_ap_cfg.interval : 10000;
		warn("next scan will be in " + next/1000 + " seconds\n");
		self.set(next);
	});
});
uloop.run();
uloop.done();

