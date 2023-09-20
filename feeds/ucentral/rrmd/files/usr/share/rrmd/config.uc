return {
	station_update: 1000,
	station_expiry: 120,

	init: function() {
		let options = uci.get_all('rrmd', '@base[-1]');
		for (let key in options)
			this[key] = options[key];
	},
};
