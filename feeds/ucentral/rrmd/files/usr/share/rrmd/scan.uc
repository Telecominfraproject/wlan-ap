const SCAN_FLAG_AP = (1<<2);

let phys = {};
let beacons = {};

function scan(phy, params) {
	if (params.wiphy_freq) {
		params.center_freq1 = params.wiphy_freq + phys[phy].offset;
		params.scan_ssids = [ '' ];
	}

	params.scan_flags = SCAN_FLAG_AP;

	printf('%.J\n', params);
	let res = global.nl80211.request(global.nl80211.const.NL80211_CMD_TRIGGER_SCAN, 0, params);
	if (res === false)
		printf("Unable to trigger scan: " + global.nl80211.error() + "\n");
	else
		printf('triggered scan on %s\n', params?.dev);
	phys[phy].pending = true;
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

function scan_timer() {
	try {
		for (let k, v in phys) {
			if (v.pending && time() - v.last >= v.delay) {
				let dev = global.local.lookup(k);
				if (dev) {
					let res = global.nl80211.request(global.nl80211.const.NL80211_CMD_GET_SCAN, global.nl80211.const.NLM_F_DUMP, { dev });
					scan_parse(res);
				}
				v.pending = false;
				v.last = time();
				v.delay = 1;
			}

			if (!v.pending && time() - v.last >= global.config.scan_interval) {
				let dev = global.local.lookup(k);
				scan(k, {
					dev,
					wiphy_freq: v.channels[v.curr_chan],
					measurement_duration: global.config.scan_dwell_time,
				
				});
				v.curr_chan = (v.curr_chan + 1) % v.num_chan;
			}
		}


	} catch(e) {
		printf('%.J\n', e.stacktrace[0].context);
	};
//	return 1000;
}

return {
	beacons,

	add_wdev: function(dev, phy) {
		if (phys[phy])
			return;
		let channels;
		let offset = 0;
		printf('%s \n', global.phy.phys[phy].band[0]);
		switch (global.phy.phys[phy].band[0]) {
		case '2G':
			printf('fooo abc\n');
			channels = global.config.channels_2g;
			break;
		case '5G':
			channels = global.config.channels_5g;
			offset = global.config.offset_5g || 0;
			break;
		default:
			return;
		}

		printf('fooo, %.J\n', channels);

		if (!channels)
			return;
		printf('fooo2\n');
		let num_chan = length(channels);
		printf('fooo3\n');
		phys[phy] = {
			channels,
			offset,
			num_chan,
			curr_chan: 0,
			delay: 5,
		};

		printf('%.J\n', phys[phy]);

	//	scan(phy, { dev });
//		scan(phy, { dev, scan_ssids: [ '' ], });
	},
	
	status: function() {
		return beacons;
	},

	init: function() {
		uloop_timeout(scan_timer, 5000);
	},
};
