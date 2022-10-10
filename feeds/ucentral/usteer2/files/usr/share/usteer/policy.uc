let policies = {};
return {
	init: function() {
		let config = global.uci.get_all('usteer2');
		for (let section in config) {
			if (config[section]['.type'] != 'policy' || !config[section].name)
				continue;
			let policy = require(`policy_${config[section].name}`);
			if (type(policy) != 'object' || type(policy.init) != 'function') {
				ulog_info('failed to load policy "%s"\n', config[section].name);
				continue;
			}
			try {
				policy.init(config[section]);
			} catch(e) {
				ulog_info('failed to initialze policy "%s"\n', config[section].name);
				continue;
			}
			ulog_info('loaded policy "%s"\n', config[section].name);
			policies[config[section].name] = policy;
		}
	},

	status: function(msg) {
		/* if no specific policies state was requested, dump the list of loaded policies */
		if (msg?.name === null)
			return { policies: keys(policies) };

		/* check if the requested policy exists and dump its state */
		if (policies[msg.name])
			return policies[msg.name].status(msg);

		/* return an empty dictionary */
		return {};
	},
};
