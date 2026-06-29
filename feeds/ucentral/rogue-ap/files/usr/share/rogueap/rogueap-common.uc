let common = {};

/* Modules and ubus connection */
common.nl = require("nl80211");
common.rtnl = require("rtnl");
common.def = common.nl.const;
common.fs = require("fs");
common.uci = require("uci");
common.cursor = common.uci.cursor();
common.ubus = require("ubus");
common.ctx = common.ubus.connect();
if (!common.ctx)
	die("Failed to connect to ubus: " + (common.ubus.error() || "unknown error") + "\n");
common.uloop = require("uloop");

/* Tunables */

/* Retry delay after a failed step, also used by rogueap_main() */
common.CONFIG_RETRY_MS = 10000;

/* Failed scan rounds retried before reporting wifi_scan_completed=false */
const SCAN_MAX_RETRIES = 3;

/* Scan interval, in minutes */
const INTERVAL_DEFAULT_MIN = 720;
const INTERVAL_MIN_MIN = 30;
const INTERVAL_MAX_MIN = 1440;

/* Wireless readiness checks (steps 3-5), in seconds */
const WIRELESS_STABLE_SEC = 3;		/* wireless config unchanged */
const WIRELESS_REGISTER_SEC = 30;	/* interfaces registered */
const IFACE_UP_TIMEOUT_SEC = 120;	/* per interface */
const WIRELESS_POLL_MS = 1000;

/* the checks count polls */
const WIRELESS_STABLE_POLLS = int(WIRELESS_STABLE_SEC * 1000 / WIRELESS_POLL_MS);
const WIRELESS_REGISTER_POLLS = int(WIRELESS_REGISTER_SEC * 1000 / WIRELESS_POLL_MS);
const IFACE_UP_POLLS = int(IFACE_UP_TIMEOUT_SEC * 1000 / WIRELESS_POLL_MS);

/* DFS override (step 6) */
const DFS_TARGET_FREQ = 5180;		/* ch36 as primary channel */
const DFS_TARGET_CENTER = { '20': 5180, '40': 5190, '80': 5210 };
const DFS_MAX_WIDTH = 80;		/* ch36 @160 MHz spans ch52-64 (DFS) */
const DFS_FALLBACK_MIN = 5260;		/* without radar flags */
const DFS_FALLBACK_MAX = 5720;
const DFS_SWITCH_WAIT_MS = 5000;

/* Runtime state */
common.stopping = false;
common.action_running = false;

/* main_timer is created by rogueap_main() in the platform scripts */
common.main_timer = null;
common.config_timer = null;
common.ready_timer = null;
common.dfs_timer = null;

/* [ { ifname, mode, radio, htmode } ], set by wireless_config_registered() */
common.wireless_ifaces = null;

/* consecutive failed scan rounds */
common.scan_fail_count = 0;

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
};

function iface_get(wdev) {
	let params = { dev: wdev };
	let res = common.nl.request(common.def.NL80211_CMD_GET_INTERFACE, wdev ? null : common.def.NLM_F_DUMP, wdev ? params : null);

	if (res === false)
	warn("Interface lookup failed: " + common.nl.error() + "\n");
	return res || [];
};

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
};

function extract_oui_lower(mac) {
	if (type(mac) != "string")
		return "";

	let parts = split(mac, ":");
	if (length(parts) != 6)
		return "";

	let oui = parts[0] + parts[1] + parts[2];
	return ascii_to_lower(oui);
};

function parse_config_int(value) {
	if (type(value) == "int")
		return value;

	if (type(value) == "string" && match(value, /^-?[0-9]+$/))
		return int(value, 10);

	return null;
};

function valid_bssid(value) {
	return type(value) == "string" && length(value) == 17 &&
		match(value, /^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$/) != null;
};

function load_rogueap_rules() {
	let result = {
		interval: INTERVAL_DEFAULT_MIN,
		override_dfs: 0,
		administeredbssid: {},
		rules: []
	};

	let rule_mode = null;

	try {
		/* reload from disk on every round */
		common.cursor.unload("rogueap");
		if (!common.cursor.load("rogueap"))
			die("Cannot load /etc/config/rogueap");

		common.cursor.foreach("rogueap", "config", (section) => {
			if (exists(section, "interval")) {
				let value = parse_config_int(section.interval);
				if (type(value) != "int" ||
				value < INTERVAL_MIN_MIN || value > INTERVAL_MAX_MIN)
					die(sprintf(
					    "interval=%J (%d..%d)",
					    section.interval,
					    INTERVAL_MIN_MIN, INTERVAL_MAX_MIN));

				result.interval = value;
                        }

			if (exists(section, "override_dfs")) {
				let value = section.override_dfs;
				if (type(value) == "bool") {
					result.override_dfs = value ? 1 : 0;
				} else if (value === 0 || value === 1 ||
					value === "0" || value === "1") {
					result.override_dfs = int(value);
				} else {
					die(sprintf(
					    "override_dfs=%J (bool or 0/1)",
					    value));
				}
			}
		});

		common.cursor.foreach("rogueap", "administeredbssid", (section) => {
			let section_name = section[".name"] || "<unnamed>";

			for (let key in keys(section)) {
				if (substr(key, 0, 1) == ".")
					continue;

				if (key != "bssid")
					die(sprintf(
					    "administeredbssid %s: unknown key %J",
					    section_name, key));
			}

			if (!exists(section, "bssid") ||
			    type(section.bssid) != "array" ||
			    length(section.bssid) == 0)
				die(sprintf(
				    "administeredbssid %s: bssid list required",
				    section_name));

			for (let value in section.bssid) {
				if (!valid_bssid(value))
					die(sprintf(
					    "administeredbssid %s: invalid BSSID %J",
					    section_name, value));

				result.administeredbssid[ascii_to_lower(value)] = true;
			}
		});

		common.cursor.foreach("rogueap", "rules", (section) => {
			let rule_name = section[".name"] || "<unnamed>";
			let rule_keys = [];

			for (let key in keys(section)) {
				if (substr(key, 0, 1) == ".")
					continue;

				push(rule_keys, key);
			}

			if (length(rule_keys) != 1)
				die(sprintf(
				    "rule %s: expected one key, got %d: %J",
				    rule_name, length(rule_keys), rule_keys));

			let key = rule_keys[0];
			if (index([ "ssid", "rssi", "bssid", "vendor" ], key) < 0)
				die(sprintf(
				    "rule %s: unknown key %J",
				    rule_name, key));

			let current_mode = key == "vendor" ? "vendor" : "blacklist";
			if (rule_mode != null && rule_mode != current_mode)
				die(sprintf(
				    "rule %s: %J conflicts with %s mode",
				    rule_name, key, rule_mode));

			rule_mode = current_mode;

			let rule = {};

			if (section.ssid)
				rule.ssid = section.ssid;

			if (exists(section, "rssi")) {
				let value = parse_config_int(section.rssi);
				if (type(value) != "int" || value < -100 || value > 0)
					die(sprintf(
					    "rssi=%J (-100..0)",
					    section.rssi));

				rule.rssi = value;
			}

			if (exists(section, "bssid")) {
				let value = section.bssid;
				if (type(value) != "string" ||
				    length(value) != 17 ||
				    !match(value, /^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$/))
					die(sprintf(
					    "rule %s: invalid bssid %J",
					    section[".name"] || "<unnamed>",
					    value));

				rule.bssid = ascii_to_lower(value);
			}

			if (exists(section, "vendor")) {
				let value = section.vendor;
				if (type(value) != "string" ||
				    length(value) != 6 ||
				    !match(value, /^[0-9a-fA-F]{6}$/))
					die(sprintf(
					    "invalid vendor %J",
					    section.vendor));

				rule.vendor = ascii_to_lower(value);
			}

			if (length(rule) > 0)
				push(result.rules, rule);

		});
	} catch(e) {
		warn("Config error: " + (e.message || e) +
		     "; retry in " + common.CONFIG_RETRY_MS / 1000 + "s\n");

		return null;
	}
	return result;
};

function cancel_wait_timers() {
	if (common.config_timer) {
		common.config_timer.cancel();
		common.config_timer = null;
	}

	if (common.ready_timer) {
		common.ready_timer.cancel();
		common.ready_timer = null;
	}

	if (common.dfs_timer) {
		common.dfs_timer.cancel();
		common.dfs_timer = null;
	}
};

function is_interface_up(dev) {
	let operstate_raw = common.fs.readfile("/sys/class/net/" + dev + "/operstate");
	if (operstate_raw === false) {
		warn("Cannot read " + dev + " operstate\n");
		return false;
	}

	let state = trim(operstate_raw);
	if (state != "up" && state != "UP")
		return false;

	let wireless_iface_status = common.ctx.call("hostapd." + dev, "get_status");
	if (!wireless_iface_status || wireless_iface_status.status != "ENABLED")
		return false;

	return true;
};

function wireless_config_registered() {
	let status = common.ctx.call('network.wireless', 'status');
	if (!status) {
		warn("Wireless status query failed\n");
		return false;
	}

	let wanted = [];
	for (let radio_name, radio in status) {
		if (radio?.pending)
			return false;

		if (radio?.disabled || radio?.config?.disabled)
			continue;

		for (let iface in radio?.interfaces || []) {
			if (iface?.config?.disabled)
				continue;

			let ifname = trim(iface?.ifname || "");
			if (ifname == "")
				continue;

			push(wanted, {
				ifname: ifname,
				mode: iface?.config?.mode || "ap",
				radio: radio_name,
				htmode: radio?.config?.htmode
			});
		}
	}

	if (length(wanted) == 0)
		return false;

	/* extra kernel interfaces (MLD, monitor, ...) are fine */
	let actual = {};
	for (let iface in iface_get())
		if (iface?.dev)
			actual[iface.dev] = true;

	for (let w in wanted)
		if (!actual[w.ifname])
			return false;

	common.wireless_ifaces = wanted;
	return true;
};

function wait_wireless_config_ready(done_cb) {
	if (!done_cb)
		done_cb = function() {};

	if (common.stopping)
		return;

	common.wireless_ifaces = null;

	let conf = "/etc/config/wireless";
	let last_mtime = 0;
	let stable = 0;
	let phase = "stable";
	let attempts = 0;
	let finished = false;

	let finish = function(ok) {
		if (finished)
			return;

		finished = true;
		if (common.config_timer) {
			common.config_timer.cancel();
			common.config_timer = null;
		}

		if (!common.stopping)
			done_cb(ok);
	};

	common.config_timer = common.uloop.timer(0, function() {
		if (common.stopping)
			return;

		try {
			if (phase == "stable") {
				let cur = common.fs.stat(conf)?.mtime || 0;
				if (cur == last_mtime) {
					stable++;
				} else {
					stable = 0;
					last_mtime = cur;
				}

				if (stable < WIRELESS_STABLE_POLLS) {
					this.set(WIRELESS_POLL_MS);
					return;
				}
				phase = "registered";
			}

			if (attempts >= WIRELESS_REGISTER_POLLS) {
				warn("Wireless config timeout\n");
				finish(false);
				return;
			}

			attempts++;
			if (wireless_config_registered()) {
				finish(true);
				return;
			}

			if (!common.stopping)
				this.set(WIRELESS_POLL_MS);
		} catch (e) {
			warn("Wireless config error: " +
			     (e.message || e) + "\n");
			finish(false);
		}
	});

	if (!common.config_timer) {
		warn("Config timer failed\n");
		finish(false);
	}
};

function wait_wireless_ready(done_cb) {
	if (!done_cb)
		done_cb = function() {};

	if (common.stopping)
		return;

	let idx = 0;
	let tries = 0;

	/* only hostapd APs can reach ENABLED */
	let ifaces = filter(common.wireless_ifaces || [], (w) => w.mode == "ap");

	let finished = false;
	let finish = function(ok) {
		if (finished)
			return;
		finished = true;

		if (common.ready_timer) {
			common.ready_timer.cancel();
			common.ready_timer = null;
		}

		if (!common.stopping)
			done_cb(ok);
	};

	if (length(ifaces) == 0) {
		warn("No AP interfaces to check\n");
		finish(false);
		return;
	}

	common.ready_timer = common.uloop.timer(0, function() {
		if (common.stopping)
			return;

		try {
			if (idx >= length(ifaces)) {
				finish(true);
				return;
			}

			let iface = ifaces[idx];
			if (is_interface_up(iface.ifname)) {
				idx++;
				tries = 0;
			} else {
				tries++;
				if (tries >= IFACE_UP_POLLS) {
					warn("Interface UP timeout: " + iface.ifname + "\n");
					finish(false);
					return;
				}
			}

			if (!common.stopping)
				this.set(WIRELESS_POLL_MS);
		} catch (e) {
			warn("Interface check failed: " + (e.message || e) + "\n");
			finish(false);
		}
	});

	if (!common.ready_timer) {
		warn("Interface timer failed\n");
		finish(false);
	}
};

/* DFS override helpers (step 6) */

/* Radar (DFS) frequencies; empty on error */
function dfs_radar_freqs() {
	let radar = {};

	try {
		let phys = common.nl.request(common.def.NL80211_CMD_GET_WIPHY,
					     common.def.NLM_F_DUMP,
					     { split_wiphy_dump: true });

		if (phys === false)
			warn("radar flag lookup failed: " + common.nl.error() +
			     ", using fallback range\n");

		for (let phy in phys || [])
			for (let band in phy?.wiphy_bands || [])
				for (let f in band?.freqs || [])
					if (f?.radar)
						radar[f.freq] = true;
	} catch (e) {
		warn("radar flag lookup failed: " + (e.message || e) +
		     ", using fallback range\n");
		radar = {};
	}

	return radar;
};

/* Returns is_dfs(freq), falling back to DFS_FALLBACK_* without radar data */
function dfs_checker() {
	let radar = dfs_radar_freqs();

	return (freq) => length(radar) ? !!radar[freq] :
		(freq >= DFS_FALLBACK_MIN && freq <= DFS_FALLBACK_MAX);
};

/* "EHT80" -> { mode: "EHT", width: 80 } */
function htmode_parse(htmode) {
	let m = match(htmode || "", /^(NOHT|HT|VHT|HE|EHT)([0-9]+)?/);

	return {
		mode: m ? m[1] : null,
		width: (m && m[2]) ? int(m[2]) : null
	};
};

/* switch_chan parameters for ch36, width capped at DFS_MAX_WIDTH */
function dfs_csa_params(mode, width) {
	if (mode == "NOHT")
		width = 20;
	if (width > DFS_MAX_WIDTH)
		width = DFS_MAX_WIDTH;
	if (!DFS_TARGET_CENTER["" + width])
		width = 20;

	return {
		freq: DFS_TARGET_FREQ,
		bcn_count: 10,
		bandwidth: width,
		center_freq1: DFS_TARGET_CENTER["" + width],
		sec_channel_offset: width > 20 ? 1 : 0,
		ht: mode != "NOHT",
		vht: index([ "VHT", "HE", "EHT" ], mode) >= 0,
		he: index([ "HE", "EHT" ], mode) >= 0,
		eht: mode == "EHT"
	};
};

/* switch_chan returns no data: success is judged by ctx.error() */
function hostapd_switch_chan(obj, params) {
	common.ctx.error();			/* clear stale error */
	common.ctx.call(obj, "switch_chan", params);

	return common.ctx.error() || null;
};

/* Verify the switched radios after the CSA; only logs, always continues */
function dfs_wait_and_verify(items, done_cb) {
	if (length(items) == 0) {
		done_cb(true);
		return;
	}

	common.dfs_timer = common.uloop.timer(DFS_SWITCH_WAIT_MS, function() {
		common.dfs_timer = null;
		if (common.stopping)
			return;

		try {
			for (let s in items) {
				let st = common.ctx.call(s.obj, "get_status");
				if (st?.freq != DFS_TARGET_FREQ || st?.status != "ENABLED")
					warn(sprintf("%s: after switch_chan freq=%J status=%J\n",
						s.label, st?.freq, st?.status));
			}
		} catch (e) {
			warn("DFS switch verification failed: " + (e.message || e) + "\n");
		}

		done_cb(true);
	});

	if (!common.dfs_timer) {
		warn("DFS timer creation failed\n");
		done_cb(false);
	}
};

/* Scan results */

/* GET_SCAN dump of dev, [] if empty, null on error */
function scan_get_results(dev) {
	common.nl.error();
	let res = common.nl.request(common.def.NL80211_CMD_GET_SCAN,
				    common.def.NLM_F_DUMP, { dev });
	let err = common.nl.error();

	if (res === false || err || (res != null && type(res) != "array")) {
		warn("Scan results failed: " + dev + ": " +
		     (err || "invalid dump") + "\n");
		return null;
	}

	return res || [];
};

function bss_parse(bss) {
	let ssid = "";

	for (let ie in bss.beacon_ies) {
		if (ssid)
			break;

		if (ie.type == 0)
			ssid = ie.data;
	}

	return {
		ssid: ssid,
		bssid: bss.bssid,
		rssi: +bss.signal_mbm / 100,
		vendor: extract_oui_lower(bss.bssid),
		channel: frequency_to_channel(+bss.frequency),
		frequency: +bss.frequency
	};
};

/* whitelist mode is selected by the first rule */
function rules_vendor_whitelist(cfg) {
	return length(cfg.rules) > 0 && cfg.rules[0].vendor != null;
};

/* administeredbssid -> never rogue
 * whitelist mode    -> rogue unless the vendor is listed
 * blacklist mode    -> rogue if any bssid/ssid/rssi rule matches
 */
function classify_rogue(entry, cfg, whitelist) {
	let bssid = entry.bssid;

	if (type(bssid) == "string" &&
	    exists(cfg.administeredbssid, ascii_to_lower(bssid)))
		return false;

	if (whitelist) {
		for (let f in cfg.rules)
			if (entry.vendor && entry.vendor === f.vendor)
				return false;
		return true;
	}

	for (let f in cfg.rules) {
		if (f.bssid != null) {
			if (bssid && bssid === f.bssid)
				return true;
		} else if (f.ssid != null) {
			if (entry.ssid === f.ssid)
				return true;
		} else if (f.rssi != null) {
			if (entry.rssi != null && entry.rssi >= f.rssi)
				return true;
		}
	}

	return false;
};

function classify_results(res, cfg) {
	let whitelist = rules_vendor_whitelist(cfg);
	let out = [];

	for (let r in res || []) {
		if (!r?.bss)
			continue;

		let entry = bss_parse(r.bss);
		entry.rogue_ap = classify_rogue(entry, cfg, whitelist);
		push(out, entry);
	}

	return out;
};

/* Outcome of a scan round; null makes action_start() retry.
 * failed: unexpected errors, retried; skipped: DFS radios, not retried.
 * Partial results are retried and only reported once retries run out.
 */
function scan_round_result(scan, scanned, failed, skipped) {
	if (scanned == 0) {
		warn(sprintf("Scan failed: no PHY delivered results (%d failed, %d skipped)\n",
			     failed, skipped));

		/* only DFS radios: retrying cannot help */
		if (failed == 0 && skipped > 0) {
			warn("Only radios on DFS channels, not retrying\n");
			common.scan_fail_count = SCAN_MAX_RETRIES;
		}

		return null;
	}

	if (failed == 0)
		return scan;

	if (common.scan_fail_count < SCAN_MAX_RETRIES) {
		warn(sprintf("Scan incomplete: %d PHY(s) failed, retrying\n", failed));
		return null;
	}

	warn(sprintf("Scan incomplete after %d retries: reporting %d PHY(s), %d failed\n",
		     SCAN_MAX_RETRIES, scanned, failed));
	return scan;
};

/* wifi_scan_completed=false: data is empty, not "no APs seen" */
function report_scan(completed, scan) {
	common.ctx.error();			/* clear stale error */
	common.ctx.call('ucentral', 'send', {
		method: 'rogue_ap',
		params: {
			wifi_scan_completed: completed,
			data: completed ? scan : []
		}
	});

	let err = common.ctx.error();
	if (err)
		warn("ucentral send failed: " + err + "\n");
};

function handle_sigterm() {
	if (common.stopping)
		return;

	common.stopping = true;
	common.action_running = false;

	if (common.main_timer) {
		common.main_timer.cancel();
		common.main_timer = null;
	}

	cancel_wait_timers();
	common.uloop.end();
};

/* Scan cycle (steps 2-7) */

function ucentral_connected() {
	let st = common.ctx.call("ucentral", "status");
	return !!(st && st.connected != null);
};

/* hooks: override_dfs(cfg, cb), wifi_scan(cfg)
 * done_cb(true) waits cfg.interval, done_cb(false) retries sooner
 */
function action_start(hooks, cfg, done_cb) {
	if (common.stopping)
		return;

	if (common.action_running) {
		warn("Scan already running\n");
		if (done_cb)
			done_cb(false);
		return;
	}
	common.action_running = true;

	let finished = false;
	let finish = function(ok) {
		if (finished)
			return;

		finished = true;
		cancel_wait_timers();
		common.action_running = false;

		if (!common.stopping && done_cb)
			done_cb(ok);
	};

	/* an exception in a stage callback must still end the cycle */
	let guard = (fn) => function(ok) {
		try {
			fn(ok);
		} catch (e) {
			warn("Scan cycle failed: " + (e.message || e) + "\n");
			finish(false);
		}
	};

	/* retry up to SCAN_MAX_RETRIES times, then report the failure */
	let scan_fail = function() {
		common.scan_fail_count++;
		if (common.scan_fail_count <= SCAN_MAX_RETRIES) {
			warn(sprintf("Scan failed, retry %d/%d\n",
				     common.scan_fail_count, SCAN_MAX_RETRIES));
			finish(false);
			return;
		}

		warn("Scan failed, no retries left, reporting failure\n");
		common.scan_fail_count = 0;
		report_scan(false, []);
		finish(true);
	};

	/* step 7 */
	let run_scan = function() {
		try {
			let scan = hooks.wifi_scan(cfg);
			if (common.stopping)
				return;

			if (scan == null) {
				scan_fail();
				return;
			}

			common.scan_fail_count = 0;
			report_scan(true, scan);
			finish(true);
		} catch (e) {
			warn("Scan/report failed: " + (e.message || e) + "\n");

			/* count exceptions as failed scans */
			scan_fail();
		}
	};

	try {
		/* step 2 */
		if (!ucentral_connected()) {
			warn("ucentral service is not connected, try again later\n");
			finish(false);
			return;
		}
		warn("ucentral service is connected\n");

		/* steps 3 + 4 */
		wait_wireless_config_ready(guard(function(cfg_complete) {
			if (common.stopping)
				return;
			if (!cfg_complete) {
				warn("wireless configure failed, try again later\n");
				finish(false);
				return;
			}
			warn("wireless config ready\n");

			/* step 5 */
			wait_wireless_ready(guard(function(wireless_ok) {
				if (common.stopping)
					return;
				if (!wireless_ok) {
					warn("wireless interfaces not ready, will retry later\n");
					finish(false);
					return;
				}
				warn("wireless interface ready\n");

				/* step 6 */
				hooks.override_dfs(cfg, guard(function(dfs_ready) {
					if (common.stopping)
						return;
					if (!dfs_ready) {
						finish(false);
						return;
					}

					run_scan();
				}));
			}));
		}));
	} catch (e) {
		warn("Scan cycle failed: " + (e.message || e) + "\n");
		finish(false);
	}
};

export {
	common,
	frequency_to_channel,
	iface_get,
	ascii_to_lower,
	extract_oui_lower,
	parse_config_int,
	valid_bssid,
	load_rogueap_rules,
	cancel_wait_timers,
	is_interface_up,
	wireless_config_registered,
	wait_wireless_config_ready,
	wait_wireless_ready,
	scan_get_results,
	bss_parse,
	rules_vendor_whitelist,
	classify_rogue,
	classify_results,
	scan_round_result,
	report_scan,
	DFS_TARGET_FREQ,
	dfs_radar_freqs,
	dfs_checker,
	htmode_parse,
	dfs_csa_params,
	hostapd_switch_chan,
	dfs_wait_and_verify,
	SCAN_MAX_RETRIES,
	ucentral_connected,
	action_start,
	handle_sigterm
};

