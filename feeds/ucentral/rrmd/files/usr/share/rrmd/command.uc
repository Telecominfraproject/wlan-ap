function result(error, event, text, data) {
	let res = {
		error: error,
		text: text || 'unknown',
		...(data ? data : {}),
	};
	if (event)
		global.event.send('rrm.bss.command', res);
	return res;
}

const actions = {
	// ubus call rrm command '{"action": "kick", "addr": "1c:57:dc:37:3c:b1", "reason": 5, "ban_time": 30, "global_ban": true }'
	kick: function(msg) {
		if (!exists(msg, 'global_ban'))
			msg.global_ban = true;
		if (!global.station.kick(msg))
			return result(1, msg.event, 'station ' + msg.addr + ' is unknown', { action: 'kick', addr: msg.addr });

		return result(0, 0, 'station ' + msg.addr + ' was kicked', { action: 'kick', addr: msg.addr });
	},

	// ubus call rrm command '{"action": "beacon_request", "addr": "4e:7f:3e:2c:8a:68", "params": "channel": 1}'
	// ubus call rrm command '{"action": "beacon_request", "addr": "4e:7f:3e:2c:8a:68", "params": "ssid": "Pluto" }'
	// ubus call rrm get_beacon_request '{"addr": "4e:7f:3e:2c:8a:68"}'
	beacon_request: function(msg) {
		if (!global.station.beacon_request(msg))
			return result(1, msg.event, 'station ' + msg.addr + ' is unknown', { action: 'beacon_request', addr: msg.addr });

		return result(0, 0, 'station ' + msg.addr + ' beacon-request sent', { action: 'beacon_request', addr: msg.addr });
	},

	// ubus call rrm command '{"action": "channel_switch", "bssid": "34:eF:b6:aF:48:b1", "channel": 4 }'
	channel_switch: function(msg) {
		if (!global.local.switch_chan(msg))
			return result(1, msg.event, 'BSS ' + msg.bssid + ' failed to trigger channel switch', { action: 'channel_switch', bssid: msg.bssid });

		return result(0, msg.event, 'BSS ' + msg.bssid + ' triggered channel switch', { action: 'channel_switch', bssid: msg.bssid });
	},

	// ubus call rrm command '{"action": "tx_power", "bssid": "34:eF:b6:aF:48:b1", "level": 20}'
	tx_power: function(msg) {
		if (!global.phy.txpower(msg))
			return result(1, msg.event, 'BSS ' + msg.bssid + ' failed to set TX power', { action: 'tx_power', bssid: msg.bssid });
	
		let level = global.local.txpower(msg.bssid) / 100;
		return result(0, msg.event, 'BSS ' + msg.bssid + ' changed TX power', { action: 'tx_power', bssid: msg.bssid, level });
	},

	// ubus call rrm command '{"action": "bss_transition", "addr": "4e:7f:3e:2c:8a:68", "neighbors": ["34:ef:b6:af:48:b1"] }'
	bss_transition: function(msg) {
		if (!global.station.bss_transition(msg))
			return result(1, msg.event, 'BSS transition ' + msg.addr + ' failed to trigger', { action: 'bss_transition', addr: msg.addr });

		return result(0, 0, 'BSS transition ' + msg.addr + ' triggered');
	},
	
	// ubus call rrm command '{"action": "neighbors", "neighbors": [ [ "00:11:22:33:44:55", "OpenWifi", "34efb6af48b1af4900005301070603010300" ], [ "aa:bb:cc:dd:ee:ff", "OpenWifi2", "34efb6af48b1af4900005301070603010300" ] ] }'
	neighbors: function(msg) {
		if (!global.neighbor.remote(msg))
			return result(1, msg.event, 'Failed to set neighbors', { action: 'neighbors' });
		return result(0, 0, 'applied neighbor reports');
	},
};

return {
	handle: function(msg) {
		if (!actions[msg.action])
			return result(1, msg.event, 'unknown action ' + msg.action);
		try {
			return actions[msg.action](msg);
		} catch(e) {
			printf('%.J\n', e.stacktrace[0].context);
			return result(1, msg.event, 'action ' + msg.action + ' failed to execute');
		}
	},
};
