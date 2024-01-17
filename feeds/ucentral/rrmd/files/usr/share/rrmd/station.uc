let stations = {};

function station_add(device, addr, data, seen, bssid) {
	let add = false;

	/* only honour stations that are authenticated */
	if (!data.auth || !data.assoc)
		return;

	/* if the station is new, add the initial entry */
	if (!stations[addr]) {
		ulog_info(`add station ${ addr }\n`);
		add = true;

		/* extract the rrm bits and give them meaningful names */
		let rrm = {
			link_measure: !!(data.rrm[0] & 0x1),
			beacon_passive_measure: !!(data.rrm[0] & 0x10),
			beacon_active_measure: !!(data.rrm[0] & 0x20),
			beacon_table_measure: !!(data.rrm[0] & 0x40),
			statistics_measure: !!(data.rrm[1] & 0x8),
		};

		/* add the new station */
		stations[addr] = {
			bssid,
			addr,
			rrm,
			beacon_report: {},
		};

	/*	if (config.beacon_request_assoc)
			stations[addr].beacon_request_assoc = time();
	*/
	}

	/* update device, seen and signal data */
	stations[addr].device = device;
	stations[addr].seen = seen;
	stations[addr].ssid = global.local.interfaces[device].ssid;
	stations[addr].channel = global.local.interfaces[device].channel;
	stations[addr].op_class = global.local.interfaces[device].op_class;
	if (data.signal)
		stations[addr].signal = data.signal;

	/* if the station just joined, send an event */
	if (add)
		global.event.send('rrm.station.add', { addr, rrm: stations[addr].rrm, bssid });

	/* check if a beacon_report should be triggered */
	/*
	if (stations[addr].beacon_request_assoc && time() - stations[addr].beacon_request_assoc >= 30) {
		global.station.beacon_request({ addr, channel: stations[addr].channel});
		stations[addr].beacon_request_assoc = 0;	
	}

	if (stations[addr].beacon_report_seen && time() - stations[addr].beacon_report_seen > 3) {
		global.event.send('rrm.beacon.report', { addr, report: stations[addr].beacon_report });
		stations[addr].beacon_report_seen = 0;
	}
	*/
}

function station_del(addr) {
	if (!stations[addr])
		return;
	ulog_info(`deleting ${ addr }\n`);

	/* send an event */
	global.event.send('rrm.station.del', { addr, bssid: stations[addr].bssid });

	delete stations[addr];
}

function stations_update() {
	try {
		/* lets not call time() multiple times */
		let seen = time();
		
		/* list off all known macs */
		let macs = [];

		/* iterate over all ssids and ask hapd for the list of associations */
		for (let device, v in global.local.status()) {
			let clients = global.ubus.conn.call(`hostapd.${device}`, 'get_clients');

			for (let client in clients.clients) {
				station_add(device, client, clients.clients[client], seen, v.bssid);
				push(macs, client);
			}
		}
		
		/* purge all stations that have not been seen in a while or are not associated */
		for (let station in stations) {
			if (!(station in macs) || seen - stations[station].seen <= +global.config.station_expiry)
				continue;
			station_del(station);
		}

	} catch (e) {
		printf('%.J', e);
	}

	/* restart the timer */
	return +global.config.station_update;
}

function beacon_report(type, report) {
	/* make sure that the station exists */
	if (!stations[report.address]) {
		ulog_err(`beacon report on unknown station ${report.address}\n`);
		return true;
	}

	if (stations[report.address].beacon_report[report.bssid])
		return true;

	/* store the report */
	let payload = {
		addr: report.address,
		bssid: report.bssid,
		seen: time(),
		channel: report.channel,
		rcpi: report.rcpi,
		rsni: report.rsni,
	};
	stations[report.address].beacon_report[report.bssid] = payload;
	stations[report.address].beacon_report_seen = time();;
}

function probe_handler(type, data) {
	/* track non-associated stations SNR */
	stations[data.address] = {
		signal: data.signal,
		connected: false,
		seen: time(),
	};
	return true;
}

function disassoc_handler(type, data) {
	station_del(data.address);

	return true;
}

return {
	stations,
	init: function() {
		/* register the mgmt frame handlers */
		global.local.register_handler('beacon-report', beacon_report);
		//global.local.register_handler('probe', probe_handler);
		global.local.register_handler('disassoc', disassoc_handler);

		/* initial probe of associated stations */
		uloop_timeout(stations_update, 100);
	},

	status: function() {
		let ret = { };
		let now = time();

		/* get the list of our local APs */
		let local = global.local.status();

		/* iterate over all APs and aggregate their associations */
		for (let device in local) {
			/* iterate over all known stations */
			for (let addr, station in stations) {
				/* match for the current AP */
				if (station.device != device)
					continue;

				/* add the station info to the return data */
				ret[addr] = {
					[device]: {
						signal: station.signal,
						rrm: station.rrm,
						last_seen: now - station.seen,
					},
				};

				if (length(station.beacon_report))
					ret[addr][device].beacon_report = station.beacon_report;
			}
		}
		return ret;
	},

	beacon_request: function(msg) {
		if (!msg.addr)
			return false;

		let station = stations[msg.addr];

		/* make sure that the station exists */
		if (!station) {
			ulog_err(`beacon request on unknown station ${msg.addr}`);
			return false;
		}

		/* make sure that the station supports active beacon requests */
		if (!station.rrm?.beacon_active_measure) {
			ulog_err(`${msg.addr} does not support beacon requests`);
			return false;
		}

		station.beacon_report = {};
		let payload = {
			addr: msg.addr,
			mode: msg.mode || 1,
			op_class: station.op_class || 128,
			duration: msg.duration || 100,
			channel: (msg.channel == null) ? station.channel : msg.channel,
		};
		if (msg.ssid)
			payload.ssid = msg.ssid;
		global.ubus.conn.call(`hostapd.${station.device}`, 'rrm_beacon_req', payload);

		return true;
	},

	kick: function(msg) {
		if (!msg.addr || !msg.ban_time || !msg.reason)
			return false;

		if (!exists(stations, msg.addr))
			return false;

		let payload = {
			addr: msg.addr,
			reason: msg.reason,
			deauth: 1,
			ban_time: msg.ban_time,
			global_ban: msg.global_ban || false,
		};

		/* tell hostapd to kick a station via ubus */
		global.ubus.conn.call(`hostapd.${stations[msg.addr].device}`, 'del_client', payload);

		return true;
	},

	list: function(msg) {
		if (msg?.addr)
			return stations[msg.addr] || {};
		return stations;
	},

	bss_transition: function(msg) {
		if (!msg.addr || !msg.neighbors)
			return false;
		if (!stations[msg.addr])
			return false;

		let neighbors = [];
		for (let i = 0; i < 5; i++)
			if (msg.neighbors[i])
				push(neighbors, replace(msg.neighbors[i], ':', ''));

		let ret = global.ubus.conn.call(`hostapd.${stations[msg.addr].device}`, 'wnm_disassoc_imminent', {
			addr: msg.addr, duration: 20, abridged: 1, neighbors }) == null;
		return ret;
	},

	reload: function() {
	},
};
