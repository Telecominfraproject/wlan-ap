#!/usr/bin/ucrun

push(REQUIRE_SEARCH_PATH, '/usr/share/usteer/*.uc');

global.ulog = {
	identity: 'usteer',
	channels: [ 'stdio', 'syslog' ],
};

global.ubus = {
	object: 'usteer2',

	connect: function() {
		printf('connected to ubus\n');
	},

	methods: {
		interfaces: {
			cb: function(msg) {
				return global.local.status();
			}
		},

		stations: {
			cb: function(msg) {
				return global.station.list(msg);
			}
		},

		status: {
			cb: function(msg) {
				return global.station.status();
			}
		},

		command: {
			cb: function(msg) {
				return global.command.handle(msg);
			}
		},

		get_beacon_request: {
			cb: function(msg) {
				let val = global.station.list(msg);
				return val?.beacon_report || {};
			}
		},

		policy: {
			cb: function(msg) {
				return global.policy.status(msg);
			},
		},
	},
};

global.start = function() {
	try {
		global.uci = require('uci').cursor();
		global.ubus.conn = require('ubus').connect();

		for (let module in [ 'config', 'local', 'station', 'command', 'policy' ]) {
			printf('loading ' + module + '\n');
			global[module] = require(module);
			if (exists(global[module], 'init'))
				global[module].init();
		}
	} catch(e) {
		printf('exception %s\n', e);
	}
};

global.stop = function() {
	ulog_info('stopping\n');
};
