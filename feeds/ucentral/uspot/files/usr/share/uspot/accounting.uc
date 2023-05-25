#!/usr/bin/ucode

'use strict';

let fs = require('fs');
let uloop = require('uloop');
let ubus = require('ubus').connect();
let uci = require('uci').cursor();
let interfaces = {};

let uciload = uci.foreach('uspot', 'uspot', (d) => {
	if (!d[".anonymous"]) {
		let accounting = !!(d.acct_server && d.acct_secret);
		interfaces[d[".name"]] = {
		settings: {
			accounting,
			acct_server: d.acct_server,
			acct_secret: d.acct_secret,
			acct_port: d.acct_port || 1813,
			acct_proxy: d.acct_proxy,
			acct_interval: d.acct_interval,
			nas_id: d.nasid,
			idle_timeout: d.idle_timeout || 600,
			session_timeout: d.session_timeout || 0,
			debug: d.debug,
		},
		clients: {},
		};
	}
});

if (!uciload) {
	let log = 'uspot: failed to load config';
	system('logger ' + log);
	warn(log + '\n');
	exit(1);
}

function syslog(interface, mac, msg) {
	let log = sprintf('uspot: %s %s %s', interface, mac, msg);

	system('logger \'' + log + '\'');
	warn(log + '\n');
}

function debug(interface, mac, msg) {
	if (+interfaces[interface].settings.debug)
		syslog(interface, mac, msg);
}

function radius_init(interface, mac, payload) {
	let settings = interfaces[interface].settings;

	payload.acct_server = sprintf('%s:%s:%s', settings.acct_server, settings.acct_port, settings.acct_secret);
	payload.nas_id = settings.nas_id;
	if (settings.acct_proxy)
		payload.acct_proxy = settings.acct_proxy;

	if (mac) {
		// dealing with client accounting
		let client = interfaces[interface].clients[mac];
		for (let key in [ 'acct_session', 'client_ip', 'called_station', 'calling_station', 'nas_ip', 'nas_port_type', 'username', 'location_name' ])
			if (client.radius[key])
				payload[key] = client.radius[key];
	}

	return payload;
}

function radius_call(interface, mac, payload) {
	let path = '/tmp/uacct' + (mac || payload.acct_session) + '.json';
	let cfg = fs.open(path, 'w');
	cfg.write(payload);
	cfg.close();

	system('/usr/bin/radius-client ' + path);

	if (!+interfaces[interface].settings.debug)
		fs.unlink(path);
}

function radius_acct(interface, mac, payload) {
	let state = ubus.call('spotfilter', 'client_get', {
		interface,
		address: mac
	}) || interfaces[interface].clients[mac];	// fallback to last known state
	if (!state)
		return;

	payload = radius_init(interface, mac, payload);
	payload.acct = true;
	payload.session_time = time() - state.data.connect;
	payload.output_octets = state.acct_data.bytes_dl & 0xffffffff;
	payload.input_octets = state.acct_data.bytes_ul & 0xffffffff;
	payload.output_gigawords = state.acct_data.bytes_dl >> 32;
	payload.input_gigawords = state.acct_data.bytes_ul >> 32;
	payload.output_packets = state.acct_data.packets_dl;
	payload.input_packets = state.acct_data.packets_ul;
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
	if (!interfaces[interface].clients[mac].radius)
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
}

function client_interim(interface, mac, time) {
	let client = interfaces[interface].clients[mac];

	// preserve a copy of last spotfilter stats for use in disconnect case
	let state = ubus.call('spotfilter', 'client_get', {
		interface,
		address: mac
	});
	if (!state)
		return;

	client.acct_data = state.acct_data;

	if (!client.interval)
		return;

	if (time >= client.next_interim) {
		radius_interim(interface, mac);
		client.next_interim += client.interval;
	}
}

function client_add(interface, mac, state) {
	if (state.state != 1)
		return;

	let defval = 0;

	let settings = interfaces[interface].settings;
	let accounting = settings.accounting;

	// RFC: NAS local interval value *must* override RADIUS attribute
	defval = settings.acct_interval;
	let interval = +(defval || state.data?.radius?.reply['Acct-Interim-Interval'] || 0);

	defval = settings.session_timeout;
	let session = +(state.data?.radius?.reply['Session-Timeout'] || defval);

	defval = settings.idle_timeout;
	let idle = +(state.data?.radius?.reply['Idle-Timeout'] || defval);

	let max_total = +(state.data?.radius?.reply['ChilliSpot-Max-Total-Octets'] || 0);

	let clients = interfaces[interface].clients;
	clients[mac] = {
		interval,
		session,
		idle,
		max_total,
		data: {
			connect: state.data.connect,
		}
	};
	if (state.ip4addr)
		clients[mac].ip4addr = state.ip4addr;
	if (state.ip6addr)
		clients[mac].ip6addr = state.ip6addr;
	if (state.data?.radius?.request) {
		clients[mac].radius = state.data.radius.request;
		if (accounting && interval)
			clients[mac].next_interim = state.data.connect + interval;
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

	let client = interfaces[interface].clients[mac];

	if (client.ip4addr)
		system('conntrack -D -s ' + client.ip4addr + ' -m 2');
	if (client.ip6addr)
		system('conntrack -D -s ' + client.ip6addr + ' -m 2');

	delete interfaces[interface].clients[mac];
}

function client_remove(interface, mac, reason) {
	syslog(interface, mac, reason);
	client_kick(interface, mac, true);
}

function client_reset(interface, mac, reason) {
	syslog(interface, mac, reason);
	client_kick(interface, mac, false);
}

function radius_accton(interface)
{
	// assign a global interface session ID for Accounting-On/Off messages
	let math = require('math');
	let sessionid = '';

	for (let i = 0; i < 16; i++)
		sessionid += sprintf('%x', math.rand() % 16);

	interfaces[interface].sessionid = sessionid;

	const acct_type_accton = 7;	// Accounting-On
	let payload = {
		acct_type: acct_type_accton,
		acct_session: sessionid,
	};
	payload = radius_init(interface, null, payload);
	payload.acct = true;
	radius_call(interface, null, payload);
	debug(interface, null, 'acct-on call');
}

function radius_acctoff(interface)
{
	const acct_type_acctoff = 8;	// Accounting-Off
	let payload = {
		acct_type: acct_type_acctoff,
		acct_session: interfaces[interface].sessionid,
	};
	payload = radius_init(interface, null, payload);
	payload.acct = true;
	radius_call(interface, null, payload);
	debug(interface, null, 'acct-off call');
}

function accounting(interface) {
	let list = ubus.call('spotfilter', 'client_list', { interface });
	let t = time();
	let accounting = interfaces[interface].settings.accounting;

	for (let mac, payload in list)
		if (!interfaces[interface].clients[mac])
			client_add(interface, mac, payload);

	for (let mac, client in interfaces[interface].clients) {
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

		if (+list[mac].idle > +client.idle) {
			radius_terminate(interface, mac, radtc_idleto);
			client_reset(interface, mac, 'idle event');
			continue;
		}
		let timeout = +client.session;
		if (timeout && ((t - list[mac].data.connect) > timeout)) {
			radius_terminate(interface, mac, radtc_sessionto);
			client_reset(interface, mac, 'session timeout');
			continue;
		}
		let maxtotal = +client.max_total;
		if (maxtotal && ((list[mac].acct_data.bytes_ul + list[mac].acct_data.bytes_dl) >= maxtotal)) {
			radius_terminate(interface, mac, radtc_sessionto);
			client_reset(interface, mac, 'max octets reached');
			continue;
		}

		if (accounting)
			client_interim(interface, mac, t);
	}
}

function start()
{
	let seen = {};

	for (let interface, data in interfaces) {
		if (!data.settings.acct_server || (data.settings.acct_server in seen))
			continue;	// avoid sending duplicate requests to the same server
		seen[data.settings.acct_server] = 1;
		radius_accton(interface);
	}
}

function stop()
{
	for (let interface, data in interfaces) {
		if (data.sessionid)	// we have previously sent Accounting-On
			radius_acctoff(interface);
	}
}

uloop.init();

start();

uloop.timer(10000, function() {
	for (let interface in interfaces)
		accounting(interface); 
	this.set(10000);
});

uloop.run();

stop();
