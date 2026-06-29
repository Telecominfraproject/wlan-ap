#!/usr/bin/ucode

import {
	common,
	iface_get,
	scan_get_results,
	classify_results,
	dfs_checker,
	htmode_parse,
	dfs_csa_params,
	hostapd_switch_chan,
	dfs_wait_and_verify,
	scan_round_result,
	load_rogueap_rules,
	action_start,
	handle_sigterm
} from './rogueap-common.uc';

let nl = common.nl;
let rtnl = common.rtnl;
let def = common.def;
let fs = common.fs;

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
const frequency_width = { '80': 3, '40': 2, '20': 1 };
const IFTYPE_STATION = 2;
const IFTYPE_AP = 3;
const IFTYPE_MESH = 7;
const IFF_UP = 1;

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

/* Scans run through iw, not nl.waitfor(): waitfor() can spin on the
 * shared event socket after a network restart.
 */

/* the target has no timeout(1), see iw_with_timeout() */
const SCAN_TIMEOUT_SEC = 30;

function now_ms() {
	let t = clock(true);		/* monotonic */
	return t[0] * 1000 + t[1] / 1000000;
}

/* Run iw with a watchdog. Its output goes to /dev/null so it never
 * holds the popen() pipe open; when iw ends first, the watchdog also
 * kills its own sleep so nothing is left behind.
 */
function iw_with_timeout(iw_args) {
	return sprintf("{ iw %s 2>&1 & p=$!; " +
		       "( trap 'kill $s; exit' TERM; sleep %d & s=$!; wait $s; kill $p ) " +
		       ">/dev/null 2>&1 & w=$!; " +
		       "wait $p; r=$?; kill $w; exit $r; } 2>/dev/null",
		       iw_args, SCAN_TIMEOUT_SEC);
}

/* iw exits 0 even when the scan is aborted, so check its output too.
 * Returns "ok", "busy" or "failed".
 */
function scan_trigger(wdev, freqs) {
	let cmd = iw_with_timeout(sprintf("dev '%s' scan ap-force freq %s ssid ''",
					  wdev, join(" ", freqs)));
	let out = "";
	let ret = -1;
	let t0 = now_ms();

	let p = fs.popen(cmd, "r");
	if (p) {
		out = p.read("all") || "";
		ret = p.close();
	}

	let elapsed = int(now_ms() - t0);

	if (ret === 0 && !match(out, /scan aborted/)) {
		if (length(freqs) > 1)
			warn(sprintf("Scan on %s finished in %d ms\n", wdev, elapsed));
		return "ok";
	}

	let busy = !!match(out, /Resource busy/);
	let reason = busy ? "busy" :
		(ret === 0 ? "aborted" :
		 (elapsed >= SCAN_TIMEOUT_SEC * 1000 ? "timeout" : "exit=" + ret));

	warn(sprintf("Scan failed on %s (freq %s): %s\n",
		     wdev, join(" ", freqs), reason));

	return busy ? "busy" : "failed";
}

function trigger_scan_width(wdev, freqs) {
	let results = [];

	for (let freq in freqs)
		push(results, scan_trigger(wdev, [ freq ]));
	return results;
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

/* Scanning a HaLow phy hangs in the kernel, so skip it. It advertises 5G
 * frequencies, so detect it via debugfs like renderer/wifi/phy.uc does.
 */
function phy_is_halow(phy) {
	if (phy?.wiphy == null)
		return false;

	return fs.stat('/sys/kernel/debug/ieee80211/phy' + phy.wiphy + '/morse') ? true : false;
}

let ifaces = [];

function intersect(list, filter) {
	let res = [];

	if (!list || !filter)
		return res;

	for (let item in list)
		if (index(filter, item) >= 0)
			push(res, item);
	return res;
}

/* override_dfs (step 6). One wiphy per radio, so channel and actual
 * width come from nl80211; the mode from htmode when configured.
 */

/* nl80211_chan_width -> MHz */
const CHAN_WIDTH_MHZ = { '0': 20, '1': 20, '2': 40, '3': 80, '4': 80, '5': 160 };

function iface_phy_mode(chan_width, htmode) {
	let mode = htmode_parse(htmode).mode;
	if (mode)
		return mode;

	if (chan_width == 0)
		return "NOHT";
	return (CHAN_WIDTH_MHZ["" + chan_width] || 20) >= 80 ? "VHT" : "HT";
}

function override_dfs(cfg, done_cb) {
	if (!done_cb)
		done_cb = function() {};

	if (common.stopping)
		return;

	if (!cfg?.override_dfs) {
		warn("override_DFS=1 flag not enabled\n");
		done_cb(true);
		return;
	}

	let is_dfs = dfs_checker();

	let htmode_of = {};
	for (let w in common.wireless_ifaces || [])
		htmode_of[w.ifname] = w.htmode;

	let switched = [];
	let seen_phy = {};

	for (let iface in iface_get()) {
		/* PHY 0 is valid */
		if (iface?.iftype != IFTYPE_AP || iface.wiphy == null ||
		    !iface.wiphy_freq || seen_phy[iface.wiphy])
			continue;

		/* one CSA per wiphy, all its BSSes follow */
		seen_phy[iface.wiphy] = true;

		if (!is_dfs(iface.wiphy_freq))
			continue;

		let obj = "hostapd." + iface.dev;
		let label = "phy" + iface.wiphy;
		let params = dfs_csa_params(
			iface_phy_mode(iface.channel_width, htmode_of[iface.dev]),
			CHAN_WIDTH_MHZ["" + iface.channel_width] || 20);

		let err = hostapd_switch_chan(obj, params);
		if (err) {
			warn(sprintf("%s: switch_chan %d->%d/%dMHz failed: %s\n",
				label, iface.wiphy_freq, params.freq,
				params.bandwidth, err));
			continue;
		}

		warn(sprintf("%s: on DFS freq %d, switching to %d/%dMHz\n",
			label, iface.wiphy_freq, params.freq, params.bandwidth));
		push(switched, { label: label, obj: obj });
	}

	dfs_wait_and_verify(switched, done_cb);
}

function scan_iface_create(wiphy) {
	/* leftover from an aborted run */
	nl.request(def.NL80211_CMD_DEL_INTERFACE, 0, { dev: 'scan' });
	nl.request(def.NL80211_CMD_NEW_INTERFACE, 0, { wiphy: wiphy, ifname: 'scan', iftype: IFTYPE_STATION });
	sleep(500);
	rtnl.request(rtnl.const.RTM_NEWLINK, 0, { dev: 'scan', flags: IFF_UP, change: 1});
	sleep(1000);

	return {
		dev: 'scan',
		channel_width: 1,
	};
}

function scan_iface_remove() {
	warn("removing temporary interface\n");
	nl.request(def.NL80211_CMD_DEL_INTERFACE, 0, { dev: 'scan' });
}

/* "ok" if at least one scan completed, "busy" if all were refused */
function scan_phy(phy, iface) {
	let status = [];
	let freqs = phy_get_frequencies(phy);

	let freqs_2g = intersect(freqs, frequency_list_2g);
	if (length(freqs_2g))
		push(status, scan_trigger(iface.dev, freqs_2g));

	let ch_width = iface.channel_width;
	if (!frequency_list_5g[ch_width]) {
		warn("channel_width " + ch_width + " not found in frequency_list_5g, fallback to 3\n");
		ch_width = 3;
	}

	let freqs_5g = intersect(freqs, frequency_list_5g[ch_width]);
	if (length(freqs_5g))
		status = [ ...status, ...trigger_scan_width(iface.dev, freqs_5g) ];

	if (index(status, "ok") >= 0)
		return "ok";

	let all_busy = length(status) > 0 &&
		length(filter(status, (x) => x == "busy")) == length(status);
	return all_busy ? "busy" : "failed";
}

/* Returns { status: "ok", res }, { status: "skipped" } (DFS) or
 * { status: "failed" }. Results of a failed scan are never read, they
 * would be stale.
 */
function scan_one_phy(phy, is_dfs) {
	let iface = iface_find(phy.wiphy, [ IFTYPE_STATION, IFTYPE_AP ], ifaces);
	let temp = false;

	if (!iface) {
		warn("no valid interface found for phy" + phy.wiphy + "\n");
		iface = scan_iface_create(phy.wiphy);
		temp = true;
	}

	/* a radio on a DFS channel cannot scan (EBUSY or a scan that never ends) */
	if (!temp && iface.wiphy_freq && is_dfs(iface.wiphy_freq)) {
		warn("Skipping phy" + phy.wiphy + ": " + iface.dev +
		     " on DFS freq " + iface.wiphy_freq + "\n");
		return { status: "skipped" };
	}

	warn("scanning on phy" + phy.wiphy + "\n");

	let res = null;
	let st = scan_phy(phy, iface);
	if (st == "ok")
		res = scan_get_results(iface.dev);
	else
		warn("Scan failed on phy" + phy.wiphy +
		     (st == "busy" ? " (busy)" : "") + "\n");

	if (temp)
		scan_iface_remove();

	return res == null ? { status: "failed" } : { status: "ok", res: res };
}

/* see scan_round_result() */
function wifi_scan(cfg) {
	ifaces = iface_get();

	let phys = phy_get();

	if (type(phys) != "array") {
		warn("PHY lookup failed\n");
		return null;
	}

	let is_dfs = dfs_checker();
	let scan = [];
	let scanned = 0;
	let failed = 0;
	let skipped = 0;

	for (let phy in phys) {
		/* split dumps can return fragments without a wiphy index */
		if (!phy || phy.wiphy == null)
			continue;

		if (phy_is_halow(phy)) {
			warn('skipping phy' + phy.wiphy + ' (HaLow/morse driver)\n');
			continue;
		}

		let r = scan_one_phy(phy, is_dfs);
		if (r.status == "skipped") {
			skipped++;
			continue;
		}
		if (r.status != "ok") {
			failed++;
			continue;
		}

		scanned++;
		push(scan, ...classify_results(r.res, cfg));
	}

	return scan_round_result(scan, scanned, failed, skipped);
}

/* Main loop: step 1 here, steps 2-7 in action_start() (common),
 * which calls back override_dfs() (step 6) and wifi_scan() (step 7).
 */
function rogueap_main() {
	let hooks = { override_dfs, wifi_scan };

	common.uloop.init();
	common.uloop.signal("SIGTERM", handle_sigterm);

	common.main_timer = common.uloop.timer(0, function run() {
		let self = this;
		if (common.stopping)
			return;

		/* step 1 */
		let cfg = load_rogueap_rules();
		if (cfg == null) {
			warn("configuration not ready; retry in " +
			     common.CONFIG_RETRY_MS / 1000 + " seconds\n");
			if (!common.stopping)
				self.set(common.CONFIG_RETRY_MS);
			return;
		}

		cfg.interval *= 60000;

		/* steps 2-7 */
		action_start(hooks, cfg, function(success) {
			if (common.stopping)
				return;

			let next = success ? cfg.interval : common.CONFIG_RETRY_MS;
			warn("next scan will be in " + next / 1000 + " seconds\n");
			self.set(next);
		});
	});

	common.uloop.run();
	common.uloop.done();
}

rogueap_main();

