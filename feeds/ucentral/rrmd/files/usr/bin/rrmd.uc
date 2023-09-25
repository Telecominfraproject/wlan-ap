#!/usr/bin/ucrun

push(REQUIRE_SEARCH_PATH, '/usr/share/rrmd/*.uc');

global.nl80211 = require("nl80211");
global.uloop = require("uloop");
global.fs = require('fs');

global.ulog = {
	identity: 'rrm',
	channels: [ 'stdio', 'syslog' ],
};

global.ubus = {
	object: 'rrm',

	connect: function() {
		printf('connected to ubus\n');
	},

	methods: {
		phys: {
			cb: function(msg) {
				return global.phy.status();
			}
		},

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

		reload: {
			cb: function(msg) {
				global.config.init();
				for (let module in [ 'local', 'station' ])
					global[module].reload();
			},
		},

		scan_dump: {
			cb: function(msg) {
				return global.scan.beacons;
			},
		},
	},
};

global.start = function() {
	try {
		global.uci = require('uci').cursor();
		global.ubus.conn = require('ubus').connect();

		for (let module in [ 'config', 'event', 'phy', 'scan', 'neighbor', 'local', 'station', 'command', 'policy' ]) {
			printf('loading ' + module + '\n');
			global[module] = require(module);
			if (exists(global[module], 'init'))
				global[module].init();
		}
	} catch(e) {
                printf('exception %.J %.J\n', e, e.stacktrace[0].context);
	}
};

global.stop = function() {
	ulog_info('stopping\n');
};
