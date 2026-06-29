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
let rogue_ap_cfg = load_rogueap_rules();
rogue_ap_cfg.interval *= 60000;

function hostapd_dev(obj) {
	let prefix = 'hostapd.';
	let plen = length(prefix);

	if (substr(obj, 0, plen) != prefix)
		return null;

	let dev = substr(obj, plen);
	return length(dev) ? dev : null;
}

function override_dfs() {
	if (!rogue_ap_cfg.override_dfs)
		return;

	for (let obj in ctx.list()) {
		if (!hostapd_dev(obj))
			continue;

		let status = ctx.call(obj, 'get_status');
		if (!status)
			continue;

		if (status.freq <= 5180 || status.freq >= 5960)
			continue;

		ctx.call(obj, 'switch_chan', { freq: 5180, bcn_count: 10 });
		sleep(5000);
		break;
	}
}

function wifi_scan() {
	let scan = [];
	for (let obj in ctx.list()) {
		let dev = hostapd_dev(obj);
		if (!dev)
			continue;
		system(`iw dev ${dev} scan ap-force ssid ''`);
		warn("scan complete\n");

		let res = nl.request(def.NL80211_CMD_GET_SCAN, def.NLM_F_DUMP, { dev  });
		if (res === false) {
			warn("Unable to get scan results: " + nl.error() + "\n");
			return scan;
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
		/*Just scan one to get all the data*/
		return scan;
	}
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
	let ifaces = iface_get();

	if (!done_cb)
		done_cb = function(){};

	if (length(ifaces) == 0) {
		warn("iface get fail, retrying\n");
		done_cb(false);
		return;
	}

	uloop.timer(0, function() {
		if (idx >= length(ifaces)) {
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

		override_dfs();
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

