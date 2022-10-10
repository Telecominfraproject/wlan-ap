function result(error, text, data) {
	return {
		error: error,
		text: text || 'unknown',
		...(data ? { data } : {}),
	};
}

const actions = {
	// ubus call usteer2 command '{"action": "kick", "mac": "1c:57:dc:37:3c:b1", "params": {"reason": 5, "ban_time": 30}}'
	kick: function(msg) {
		if (global.station.kick(msg.mac, msg.params?.reason, msg.params?.ban_time))
			return result(1, 'station ' + msg.mac + ' is unknown');

		return result(0, 'station ' + msg.mac + ' was kicked');
	},

	// ubus call usteer2 command '{"action": "beacon_request", "mac": "1c:57:dc:37:3c:b1", "params": {"channel": 36}}'
	// ubus call usteer2 get_beacon_request '{"mac": "1c:57:dc:37:3c:b1"}'
	beacon_request: function(msg) {
		if (!global.station.beacon_request(msg.mac, msg.params?.channel, msg.params?.op_class, msg.param?.duration))
			return result(1, 'station ' + msg.mac + ' is unknown');

		return result(0, 'station ' + msg.mac + ' beacon-request sent');
	},
};

return {
	handle: function(msg) {
		if (!actions[msg.action])
			return result(1, 'unknown action ' + msg.action);

		return actions[msg.action](msg);
	},
};
