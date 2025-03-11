const SCAN_FLAG_AP = (1<<2);
const PHY_SCAN_STATE_READY = 0;
const PHY_SCAN_STATE_TRIGGERED = 1;
const PHY_SCAN_STATE_STARTED = 2;
const PHY_SCAN_STATE_COMPLETED = 3;
const PHY_SCAN_STATE_BLOCKED = 4;

let phys = {};
let beacons = {};
let scan_blocked_cnt = 0;

function scan(phy, params) {
	if (params.wiphy_freq) {
		params.center_freq1 = (int) (params.wiphy_freq) + (int) (phys[phy].offset);
		params.scan_ssids = [ '' ];
	}

	params.scan_flags = SCAN_FLAG_AP;

	printf('%.J\n', params);
	let res = global.nl80211.request(global.nl80211.const.NL80211_CMD_TRIGGER_SCAN, 0, params);
	if (res === false)
		printf("Unable to trigger scan: " + global.nl80211.error() + "\n");
	else
		printf('triggered scan on %s\n', params?.dev);
	phys[phy].state = PHY_SCAN_STATE_TRIGGERED;
	phys[phy].last = time();
}

function scan_parse(data) {
	let seen = time();
	for (let bss in data) {
		bss = bss.bss;
		if (!bss)
			continue;
		let beacon = {
			freq: bss.frequency,
			signal: bss.signal_mbm / 100,
			seen,
		};
		for (let ie in bss.beacon_ies)
			switch (ie.type) {
			case 0:
				beacon.ssid = ie.data;
				break;
			case 114:
				beacon.meshid = ie.data;
				break;
			case 0xdd:
				let oui = hexenc(substr(ie.data, 0, 3));
				let type = ord(ie.data, 3);
				let data = substr(ie.data, 4);
				switch (oui) {
				case '48d017':
					beacon.tip = true;
					switch(type) {
					case 1:
						if (data)
							beacon.tip_name = data;
						break;
					case 2:
						if (data)
							beacon.tip_serial = data;
						break;
					case 3:
						if (data)
							beacon.tip_network_id = data;
						break;
					}
					break;
				}
				break;
			}
		beacons[bss.bssid] = beacon;
	}
	printf('%.J\n', beacons);
}

function get_scan_res(dev) {
	let res = global.nl80211.request(global.nl80211.const.NL80211_CMD_GET_SCAN, global.nl80211.const.NLM_F_DUMP, { dev });
	if (!res || res == false) {
		ulog_err("Unable to get scan results: " + global.nl80211.error() + "\n");
		return;
	}
	scan_parse(res);
}

function scan_timer() {
	try {
		let scan_trigger = false;
		let cur_blocked = 0;
		let last_blocked = 0;
		for (let k, v in phys) {
			let scan_pending = (v.state == PHY_SCAN_STATE_TRIGGERED || v.state == PHY_SCAN_STATE_STARTED) ? true : false;

			/* It should not happen, get_scan is handled by NL CB */
			if (scan_pending && time() - v.last >= v.delay) {
				ulog_warn("Scanning process too long, phy state: %s\n", phys[k]);
				let dev = global.local.lookup(k);
				if (dev) {
					let res = global.nl80211.request(global.nl80211.const.NL80211_CMD_GET_SCAN, global.nl80211.const.NLM_F_DUMP, { dev });
					scan_parse(res);
				}
				v.state = PHY_SCAN_STATE_READY;
				scan_pending = false;
				v.last = time();
			}

			if (!v?.channels || !v?.num_chan)
				continue;

			/* Running scanning parallel on virtual phys not allowed */
			if (v?.virtual_phys) {
				if (v?.state == PHY_SCAN_STATE_BLOCKED)
					last_blocked++;
				else if (scan_blocked_cnt > 0)
					continue;

				if (scan_trigger) {
					v.state = PHY_SCAN_STATE_BLOCKED;
					cur_blocked++;
					continue;
				} else if (v.state == PHY_SCAN_STATE_BLOCKED) {
					v.state = PHY_SCAN_STATE_READY;
				}
			}

			if (!scan_pending && time() - v.last >= global.config.scan_interval) {
				let dev = global.local.lookup(k);
				scan(k, {
					dev,
					wiphy_freq: v.channels[v.curr_chan],
					measurement_duration: global.config.scan_dwell_time,
				
				});
				v.state = PHY_SCAN_STATE_TRIGGERED;
				scan_trigger = true;
				v.curr_chan = (v.curr_chan + 1) % v.num_chan;
			}
			/* Shouldn't never happen */
			if (scan_blocked_cnt > 0 && scan_blocked_cnt != last_blocked) {
				log_warn("Mismatch in blocked phys config:  %s -> %s\n", phys.blocked, last_blocked);
				scan_blocked_cnt = 0;
			} else {
				scan_blocked_cnt = cur_blocked;
			}
		}


	} catch(e) {
		printf('%.J\n', e.stacktrace[0].context);
	};
//	return 1000;
}

function get_phy_from_msg(msg) {
	if (!msg?.msg)
		return;

	let phy = "phy" + msg.msg.wiphy;
	let dev = msg.msg?.dev;
	if (!dev)
		dev = global.local.lookup(phy);
	if (!dev)
		return null;

	if (global.local.interfaces[dev]?.virtual_phys)
		phy = global.local.interfaces[dev].phy;

	if (!phy)
		return null;

	return phy;
}

function nl_handle_scan_msg(msg) {
	let phy = get_phy_from_msg(msg);
	if (!phy)
		return;

	phys[phy].last = time();
	phys[phy].state = PHY_SCAN_STATE_COMPLETED;
	//get_scan_res(msg.msg?.dev);
}

function nl_scan_res_cb(msg) {
	nl_handle_scan_msg(msg);
}

function nl_scan_abort_cb(msg) {
	nl_handle_scan_msg(msg);
}

function nl_scan_start_cb(msg) {
	let phy = get_phy_from_msg(msg);
	if (!phy)
		return;

	if (!phys[phy])
		return;

	if (phys[phy].state != PHY_SCAN_STATE_TRIGGERED) {
		ulog_warn("Skip scan results not started by scan module: %s\n", msg);
		return;
	}

	phys[phy].state = PHY_SCAN_STATE_STARTED;
}

return {
	beacons,

	add_wdev: function(dev, phy) {
		if (phys[phy])
			return;

		let p = split(phy, '.');
		let virtual_phys = length(p) > 1 ? true : false;
		let bands = global.phy.phys[phy]?.band;
		if (!bands)
			bands = [ global.local.interfaces[dev].band ];

		if (!bands)
			return;
		let channels;
		let offset = 0;

		switch (bands[0]) {
		case '2G':
			channels = global.config.channels_2g;
			break;
		case '5G':
			channels = global.config.channels_5g;
			offset = global.config.offset_5g || 0;
			break;
		default:
			return;
		}

		if (!channels)
			return;
		let num_chan = length(channels);
		phys[phy] = {
			channels,
			offset,
			num_chan,
			bands,
			curr_chan: 0,
			virtual_phys,
			delay: 25,
			state: PHY_SCAN_STATE_READY,
			parent_phy: p[0],
		};
	},
	
	status: function() {
		return beacons;
	},

	init: function() {
		uloop_timeout(scan_timer, 5000);
		nl80211.listener(nl_scan_start_cb, [ nl80211.const.NL80211_CMD_TRIGGER_SCAN ]);
		nl80211.listener(nl_scan_res_cb, [ nl80211.const.NL80211_CMD_NEW_SCAN_RESULTS ]);
		nl80211.listener(nl_scan_abort_cb, [ nl80211.const.NL80211_CMD_SCAN_ABORTED ]);
	},
};
