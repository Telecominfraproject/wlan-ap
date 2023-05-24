#!/usr/bin/ucode

'use strict';

let fs = require('fs');
let uloop = require('uloop');
let ubus = require('ubus').connect();
let uci = require('uci').cursor();
let config = uci.get_all('uspot');
let clients = {};

if (!config) {
	let log = 'uspot: failed to load config';
	system('logger ' + log);
	warn(log + '\n');
	exit(1);
}

uci.foreach('uspot', 'uspot', (d) => {
	if (!d[".anonymous"])
		clients[d[".name"]] = {};
});

function syslog(interface, mac, msg) {
	let log = sprintf('uspot: %s %s %s', interface, mac, msg);

	system('logger ' + log);
	warn(log + '\n');
}

function debug(interface, mac, msg) {
	if (+config[interface].debug)
		syslog(interface, mac, msg);
}

function radius_init(interface, mac, payload) {
	for (let key in [ 'server', 'acct_server', 'acct_session', 'client_ip', 'called_station', 'calling_station', 'nas_ip', 'nas_id', 'username', 'location_name' ])
		if (clients[interface][mac].radius[key])
			payload[key] = clients[interface][mac].radius[key];

	if (config[interface].acct_proxy)
		payload.acct_proxy = config[interface].acct_proxy;

	return payload;
}

function radius_call(interface, mac, payload) {
	let path = '/tmp/uacct' + mac + '.json';
	let cfg = fs.open(path, 'w');
	cfg.write(payload);
	cfg.close();

	system('/usr/bin/radius-client ' + path);

	if (!+config[interface].debug)
		fs.unlink(path);
}

function radius_acct(interface, mac, payload) {
	let state = ubus.call('spotfilter', 'client_get', {
		interface,
		address: mac
	});
	if (!state)
		return;

	payload = radius_init(interface, mac, payload);
	payload.acct = true;
	payload.session_time = time() - state.data.connect;
	payload.output_octets = state.bytes_dl & 0xffffffff;
	payload.input_octets = state.bytes_ul & 0xffffffff;
	payload.output_gigawords = state.bytes_dl >> 32;
	payload.input_gigawords = state.bytes_ul >> 32;
	payload.output_packets = state.packets_dl;
	payload.input_packets = state.packets_ul;
	if (state.data?.radius?.reply?.Class)
		payload.class = state.data.radius.reply.Class;

	radius_call(interface, mac, payload);
}

// RADIUS Acct-Terminate-Cause attributes
const radtc_logout = 1;		// User Logout
const radtc_lostcarrier = 2;	// Lost Carrier
const radtc_idleto = 4;		// Idle Timeout
const radtc_sessionto = 5;	// Session Timeout
const radtc_adminreset = 6;	// Admin Reset

function radius_terminate(interface, mac, cause) {
	if (!clients[interface][mac].radius)
		return;

	const acct_type_stop = 2;
	let payload = {
		acct_type: acct_type_stop,
		terminate_cause: cause,
	};
	debug(interface, mac, 'acct terminate: ' + cause);
	radius_acct(interface, mac, payload);
}

function radius_interim(interface, mac) {
	const acct_type_interim = 3;
	let payload = {
		acct_type: acct_type_interim,
	};
	radius_acct(interface, mac, payload);
	debug(interface, mac, 'iterim acct call');
	clients[interface][mac].timeout.set(clients[interface][mac].interval);
}

function client_add(interface, mac, state) {
	if (state.state != 1)
		return;

	let defval = 0;

	let accounting = !!(config[interface].acct_server && config[interface].acct_secret);

	// RFC: NAS local interval value *must* override RADIUS attribute
	defval = config[interface].acct_interval;
	let interval = +(defval || state.data?.radius?.reply['Acct-Interim-Interval'] || 0) * 1000;

	defval = config[interface].session_timeout || 0;
	let session = +(state.data?.radius?.reply['Session-Timeout'] || defval);

	defval = config[interface].idle_timeout || 600;
	let idle = +(state.data?.radius?.reply['Idle-Timeout'] || defval);

	let max_total = +(state.data?.radius?.reply['ChilliSpot-Max-Total-Octets'] || 0);

	clients[interface][mac] = {
		accounting,
		interval,
		session,
		idle,
		max_total,
	};
	if (state.ip4addr)
		clients[interface][mac].ip4addr = state.ip4addr;
	if (state.ip6addr)
		clients[interface][mac].ip6addr = state.ip6addr;
	if (state.data?.radius?.request) {
		clients[interface][mac].radius = state.data.radius.request;
		if (accounting && interval)
			clients[interface][mac].timeout = uloop.timer(interval, () => radius_interim(interface, mac));
	}
	syslog(interface, mac, 'adding client');
}

function client_kick(interface, mac, remove) {
	debug(interface, mac, 'stopping accounting');
	let payload = {
		interface,
		address: mac,
		...(remove ? {} : {
			state: 0,
			dns_state: 1,
			accounting: [],
			flush: true,
		}),
	};

	ubus.call('spotfilter', remove ? 'client_remove' : 'client_set', payload);

	if (clients[interface][mac].ip4addr)
		system('conntrack -D -s ' + clients[interface][mac].ip4addr + ' -m 2');
	if (clients[interface][mac].ip6addr)
		system('conntrack -D -s ' + clients[interface][mac].ip6addr + ' -m 2');

	if (clients[interface][mac].accounting)
		clients[interface][mac].timeout.cancel();
	delete clients[interface][mac];
}

function client_remove(interface, mac, reason) {
	syslog(interface, mac, reason);
	client_kick(interface, mac, true);
}

function client_reset(interface, mac, reason) {
	syslog(interface, mac, reason);
	client_kick(interface, mac, false);
}


function accounting(interface) {
	let list = ubus.call('spotfilter', 'client_list', { interface });
	let t = time();

	for (let mac, payload in list)
		if (!clients[interface][mac])
			client_add(interface, mac, payload);

	for (let mac in clients[interface]) {
		if (!list[mac] || !list[mac].state) {
			radius_terminate(interface, mac, radtc_lostcarrier);
			client_remove(interface, mac, 'disconnect event');
			continue;
		}

		if (list[mac].data.logoff) {
			radius_terminate(interface, mac, radtc_logout);
			client_remove(interface, mac, 'logoff event');
			continue;
		}

		if (+list[mac].idle > +clients[interface][mac].idle) {
			radius_terminate(interface, mac, radtc_idleto);
			client_reset(interface, mac, 'idle event');
			continue;
		}
		let timeout = +clients[interface][mac].session;
		if (timeout && ((t - list[mac].data.connect) > timeout)) {
			radius_terminate(interface, mac, radtc_sessionto);
			client_reset(interface, mac, 'session timeout');
			continue;
		}
		let maxtotal = +clients[interface][mac].max_total;
		if (maxtotal && ((list[mac].bytes_ul + list[mac].bytes_dl) >= maxtotal)) {
			radius_terminate(interface, mac, radtc_sessionto);
			client_reset(interface, mac, 'max octets reached');
		}
	}
}

uloop.init();

uloop.timer(1000, function() {
	for (let interface in clients)
		accounting(interface); 
	this.set(1000);
});

uloop.run();
