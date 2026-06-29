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

let fs = common.fs;
let ctx = common.ctx;

function hostapd_dev(obj) {
	let prefix = 'hostapd.';
	let plen = length(prefix);

	if (substr(obj, 0, plen) != prefix)
		return null;

	let dev = substr(obj, plen);
	return length(dev) ? dev : null;
}

/* override_dfs (step 6). All radios share one wiphy, so the channel
 * comes from hostapd and the width/mode from the configured htmode.
 */
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
	let switched = [];
	let seen_radio = {};

	for (let w in common.wireless_ifaces || []) {
		if (w.mode != "ap" || seen_radio[w.radio])
			continue;

		let obj = "hostapd." + w.ifname;
		let st = ctx.call(obj, "get_status");
		if (!st?.freq)
			continue;

		/* one CSA per radio, all its BSSes follow */
		seen_radio[w.radio] = true;

		if (!is_dfs(st.freq))
			continue;

		let hm = htmode_parse(w.htmode);
		let params = dfs_csa_params(hm.mode || "HT", hm.width || 20);

		let err = hostapd_switch_chan(obj, params);
		if (err) {
			warn(sprintf("%s: switch_chan %d->%d/%dMHz failed: %s\n",
				     w.radio, st.freq, params.freq, params.bandwidth, err));
			continue;
		}

		warn(sprintf("%s: on DFS freq %d, switching to %d/%dMHz (%s)\n",
			     w.radio, st.freq, params.freq, params.bandwidth, w.htmode));
		push(switched, { label: w.radio, obj: obj });
	}

	dfs_wait_and_verify(switched, done_cb);
}

/* iw has no timeout of its own and the target has no timeout(1).
 * One scan covers every band of the wiphy, hence the long limit.
 */
const SCAN_TIMEOUT_SEC = 60;

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
 * Results are read with GET_SCAN afterwards.
 */
function iw_ap_scan(wiphy, c) {
	let out = "";
	let ret = -1;
	let t0 = now_ms();

	let p = fs.popen(iw_with_timeout(sprintf("dev '%s' scan ap-force ssid ''", c.dev)), "r");
	if (p) {
		out = p.read("all") || "";
		ret = p.close();
	}

	let elapsed = int(now_ms() - t0);

	if (ret === 0 && !match(out, /scan aborted/)) {
		warn(sprintf("Scan on %s finished in %d ms\n", c.dev, elapsed));
		return true;
	}

	let reason = match(out, /Resource busy/) ? "busy" :
		(ret === 0 ? "aborted" :
		 (elapsed >= SCAN_TIMEOUT_SEC * 1000 ? "timeout" : "exit=" + ret));

	warn(sprintf("Scan failed: phy%d %s (freq %J%s): %s\n",
		wiphy, c.dev, c.freq, c.dfs ? ", DFS" : "", reason));
	return false;
}

function iface_phy_map(ifaces) {
	let iface_phys = {};
	let phy_ids = [];

	for (let iface in ifaces) {
		/* PHY 0 is valid */
		if (!iface?.dev || iface.wiphy == null)
			continue;

		iface_phys[iface.dev] = iface.wiphy;
		if (index(phy_ids, iface.wiphy) < 0)
			push(phy_ids, iface.wiphy);
	}

	return { iface_phys, phy_ids };
}

/* One target per PHY with all its hostapd interfaces as candidates,
 * non-DFS first: on a multi-radio wiphy another radio can scan for a
 * radio stuck on a DFS channel.
 */
function build_scan_targets(is_dfs) {
	let ifaces = iface_get();
	let objects = ctx.list();
	if (type(ifaces) != "array" || type(objects) != "array") {
		warn("Interface/hostapd lookup failed\n");
		return null;
	}

	let map = iface_phy_map(ifaces);
	let targets = [];
	let target_of = {};

	for (let obj in objects) {
		let dev = hostapd_dev(obj);
		if (!dev)
			continue;

		let phy = map.iface_phys[dev];
		if (phy == null) {
			warn("Skipping " + dev + ": no kernel PHY mapping\n");
			continue;
		}

		let st = ctx.call(obj, "get_status");
		let on_dfs = !!(st?.freq && is_dfs(st.freq));

		let t = target_of["" + phy];
		if (!t) {
			t = { wiphy: phy, candidates: [] };
			target_of["" + phy] = t;
			push(targets, t);
		}
		push(t.candidates, { dev: dev, freq: st?.freq, dfs: on_dfs });
	}

	for (let t in targets)
		t.candidates = [
			...filter(t.candidates, (c) => !c.dfs),
			...filter(t.candidates, (c) => c.dfs)
		];

	warn("Wireless PHYs with interfaces: " + length(map.phy_ids) +
	     ", PHYs selected for AP scan: " + length(targets) + "\n");

	for (let phy in map.phy_ids)
		if (!target_of["" + phy])
			warn("Skipping phy" + phy + ": no hostapd interface\n");

	return targets;
}

/* Returns { status: "ok", dev }, { status: "skipped" } (only DFS
 * candidates) or { status: "failed" }. A DFS candidate is never used:
 * the scan fails with EBUSY or, on some drivers, never completes.
 */
function scan_target(t) {
	let tried = 0;

	for (let c in t.candidates) {
		if (c.dfs) {
			warn("Skipping " + c.dev + " (freq " + c.freq + ", DFS)\n");
			continue;
		}

		tried++;
		if (iw_ap_scan(t.wiphy, c)) {
			warn("Scan complete on phy" + t.wiphy + " (" + c.dev + ")\n");
			return { status: "ok", dev: c.dev };
		}
	}

	if (tried == 0) {
		warn("Skipping phy" + t.wiphy + ": all interfaces on DFS channels\n");
		return { status: "skipped" };
	}

	warn("Scan failed on phy" + t.wiphy + ": no usable interface\n");
	return { status: "failed" };
}

/* see scan_round_result() */
function wifi_scan(cfg) {
	let targets = build_scan_targets(dfs_checker());
	if (targets == null)
		return null;

	let scan = [];
	let scanned = 0;
	let failed = 0;
	let skipped = 0;

	for (let t in targets) {
		let r = scan_target(t);
		if (r.status == "skipped") {
			skipped++;
			continue;
		}

		let res = (r.status == "ok") ? scan_get_results(r.dev) : null;
		if (res == null) {
			failed++;
			continue;
		}

		scanned++;
		push(scan, ...classify_results(res, cfg));
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

