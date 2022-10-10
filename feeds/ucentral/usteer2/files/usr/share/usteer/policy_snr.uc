let config = {
	/* how many seconds must a client be below the thershold before we kick it */
	min_snr_kick_delay: 5,
	/* the reson code sent when triggering the deauth (IEEE Std 802.11-2016, 9.4.1.7, Table 9-45) */
	kick_reason: 5,
	/* the periodicity for checking client kick conditions */
	interval: 1000,
};

/* counter of how often a station was kicked */
let kick_count = 0;

let foo = 0;

function snr_update() {
	try {
		let iface = global.local.status();
		let stations = global.station.list();
		let now = time();

		/* iterate over all stations and kick anything that had a signal worse than the threshold for too long */
		for (let addr in stations) {
			let station = stations[addr];
			if (!station.signal || !station.device)
				continue;
			let device = iface[station.device].config;

			if (!device?.client_kick_rssi)
				continue;

			if (0) {
				foo++;
				if (foo > 10)
					station.signal = -80;
				printf(`snr check ${addr} ${station.seen} ${station.signal} ${device.client_kick_rssi}\n`);
			}
			printf(`${addr} ${station.signal} ${device.client_kick_rssi}\n`);

			/* ignore old stations and ones that have a good signal */
			if (now - station.seen > 2 || station.signal >= device.client_kick_rssi) {
				station.snr_kick_timer = 0;
				continue;
			}

			/* find out how long the station had a bad signal for */
			if (!station.snr_kick_timer)
				station.snr_kick_timer = now;
			if (now - station.snr_kick_timer < config.min_snr_kick_delay)
				continue;

			printf(`${now - station.seen}\n`);
			if ((now - station.seen) > 2)
				return;

			/* kick the station and ban it for the configured timeout */
			ulog_info(`kick ${addr} as signal (${station.signal}) is too low\n`);
			global.station.kick(addr, config.kick_reason, device.client_kick_ban_time);
			kick_count++;
		}
	} catch(e) {
		printf(`snr exception ${e}\n`);
	}

	return config.interval;
}

function probe_handler(type, data) {
	/* only send a probe request if the signal is good enough */
	return (data.signal > config.min_connect_snr)
}

return {
	init: function(data) {
		/* load config and override defaults if they were set in UCI */
		for (let key in config)
			if (data[key])
				config[key] = +data[key];

		/* register a callback that will inspect probe-requests and prevent a reply if SNR is too low */
		//global.local.register_handler('probe', probe_handler);

		/* register the timer that periodically checks if a client should be kicked */
		uloop_timeout(snr_update, config.interval);
	},

	status: function(data) {
		/* dump the status of this policy */
		return { config, kick_count };
	},

};
