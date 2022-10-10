let stations = {};

function station_add(device, addr, data, seen) {
	/* only honour stations that are authenticated */
	if (!data.auth || !data.assoc)
		return;

	/* if the station is new, add the initial entry */
	if (!stations[addr]) {
		ulog_info(`add station ${ addr }\n`);

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
			rrm,
			beacon_report: {},
		};
	}

	/* update device, seen and signal data */
	stations[addr].device = device;
	stations[addr].seen = seen;
	if (data.signal)
		stations[addr].signal = data.signal;
}

function station_del(addr) {
	ulog_info(`deleting ${ addr }\n`);
	delete stations[addr];
}

function stations_update() {
	try {
		/* lets not call time() multiple times */
		let seen = time();

		/* iterate over all ssids and ask hapd for the list of associations */
		for (let device in global.local.status()) {
			let clients = global.ubus.conn.call(`hostapd.${ device}`, 'get_clients');

			for (let client in clients.clients)
				if (clients.clients[client].auth)
					station_add(device, client, clients.clients[client], seen);
				else
					station_del(client);
		}

		/* purge all stations that have not been seen in a while */
		for (let station in stations) {
			if (seen - stations[station].seen <= +global.config.station_expiry)
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
		return false;
	}

	/* store the report */
	stations[report.address].beacon_report[report.bssid] = {
		seen: time(),
		channel: report.channel,
		rcpi: report.rcpi,
		rsni: report.rsni,
	};
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

	beacon_request: function(addr, channel, mode, op_class, duration) {
		let station = stations[addr];

		/* make sure that the station exists */
		if (!station) {
			ulog_err(`beacon request on unknown station ${addr}`);
			return false;
		}

		/* make sure that the station supports active beacon requests */
		if (!station.rrm?.beacon_active_measure) {
			ulog_err(`${addr} does not support beacon requests`);
			return false;
		}

		station.beacon_report = {};
		let payload = {
			addr,
			channel,
			mode: mode || 1,
			op_class: op_class || 128,
			duration: duration || 100,
		};
		global.ubus.conn.call(`hostapd.${station.device}`, 'rrm_beacon_req', payload);

		return true;
	},

	kick: function(addr, reason, ban_time) {
		if (!exists(stations, addr))
			return -1;

		let payload = {
			addr,
			reason: reason || 5,
			deauth: 1
		};

		if (ban_time)
			payload.ban_time = ban_time * 1000;

		/* tell hostapd to kick a station via ubus */
		global.ubus.conn.call(`hostapd.${stations[addr].device}`, 'del_client', payload);

		return 0;
	},

	list: function(msg) {
		if (msg?.mac)
			return stations[msg.mac] || {};
		return stations;
	},

	steer: function(addr, imminent, neighbors) {

	},
};
