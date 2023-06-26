function result(error, text, data) {
	let res = {
		error: error,
		text: text || 'unknown',
		...(data ? { data } : {}),
	};
	return res;
}

const actions = {
	// ubus call usteer2 command '{"action": "kick", "addr": "1c:57:dc:37:3c:b1", "params": {"reason": 5, "ban_time": 30}}'
	kick: function(msg) {
		if (global.station.kick(msg))
			return result(1, 'station ' + msg.mac + ' is unknown');

		return result(0, 'station ' + msg.mac + ' was kicked');
	},

	// ubus call usteer2 command '{"action": "beacon_request", "addr": "4e:7f:3e:2c:8a:68", "params": {"channel": 36}}'
	// ubus call usteer2 command '{"action": "beacon_request", "addr": "4e:7f:3e:2c:8a:68", "params": {"ssid": "Cockney"}}'
	// ubus call usteer2 get_beacon_request '{"addr": "4e:7f:3e:2c:8a:68"}'
	beacon_request: function(msg) {
		if (!global.station.beacon_request(msg))
			return result(1, 'station ' + msg.addr + ' is unknown');

		return result(0, 'station ' + msg.addr + ' beacon-request sent');
	},

	// ubus call usteer2 command '{"action": "channel_switch", "addr": "34:eF:b6:aF:48:b1", "params": {"channel": 4, "band": "2G"}}'
	channel_switch: function(msg) {
		if (!global.local.switch_chan(msg))
			return result(1, 'BSS ' + msg.bssid + ' failed to trigger channel switch');

		return result(0, 'BSS ' + msg.bssid + ' triggered channel switch');
	},

	// ubus call usteer2 command '{"action": "tx_power", "phy": "platform/soc/c000000.wifi+1", "params": {"level": 20}}'
	tx_power: function(msg) {
		if (!global.phy.txpower(msg))
			return result(1, 'PHY ' + msg,phy + ' failed to set TX power');

		return result(0, 'PHY ' + msg.phy + ' changed TX power');
	},

	// ubus call usteer2 command '{"action": "bss_transition", "addr": "4e:7f:3e:2c:8a:68", "params": { "neighbors": ["34:ef:b6:af:48:b1"]}}'
	bss_transition: function(msg) {
		if (!global.station.bss_transition(msg))
			return result(1, 'BSS transition ' + msg.addr + ' failed to trigger');

		return result(0, 'BSS transition ' + msg.addr + ' triggered');
	},
};

return {
	handle: function(msg) {
		if (!actions[msg.action])
			return result(1, 'unknown action ' + msg.action);
		try {
			return actions[msg.action](msg);
		} catch(e) {
			return result(1, 'action ' + msg.action + ' failed to execute');
		}
	},
};
