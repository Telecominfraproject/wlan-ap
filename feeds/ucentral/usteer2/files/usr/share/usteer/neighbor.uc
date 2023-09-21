let local = { };
let remote = [ ];

return {
	init: function() {

	},

	local_add: function(dev, data) {
		local[dev] = data;
	},

	local_del: function(dev) {
		delete local[dev];
	},

	get: function(dev) {
		let result = [];

		for (let k, v in remote) {
			if (local[dev][1] != v[1])
				continue;
			push(result, v);
		}
		for (let k, v in local) {
			if (k == dev || local[dev][1] != v[1])
				continue;
			push(result, v);
		}

		return result;
	},

	update: function() {
		for (let dev in local) {
			let list = this.get(dev);
			global.ubus.conn.call('hostapd.' + dev, 'rrm_nr_set', { list }); 
		}
	},

	remote: function(msg) {
		for (let neighbor in msg.neighbors) {
			if (type(neighbor) != 'array' || length(neighbor) != 3)
				return false;
			for (let i = 0; i < 3; i++)
				if (type(neighbor[i]) != 'string')
					return false;
		}
		remote = msg.neighbors;
		this.update();
		return true;
	}
};
