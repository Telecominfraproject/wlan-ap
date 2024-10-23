#!/usr/bin/ucode

'use strict';

let fs = require('fs');
let uloop = require('uloop');
let ubus = require('ubus').connect();
let uci = require('uci').cursor();
let interfaces = {};
let hapd_subscriber;

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

	fs.unlink(path);
}

// RADIUS Acct-Status-Type attributes
const radat_start = 1;		// Start
const radat_stop = 2;		// Stop
const radat_interim = 3;	// Interim-Update
const radat_accton = 7;		// Accounting-On
const radat_acctoff = 8;	// Accounting-Off

function radius_acct(interface, mac, payload) {
	let state = ubus.call('spotfilter', 'client_get', {
		interface,
		address: mac
	}) || interfaces[interface].clients[mac];	// fallback to last known state
	if (!state)
		return;

	payload = radius_init(interface, mac, payload);
	payload.acct = true;

	if (payload.acct_type != radat_start) {
		payload.session_time = time() - state.data.connect;
		payload.output_octets = state.acct_data.bytes_dl & 0xffffffff;
		payload.input_octets = state.acct_data.bytes_ul & 0xffffffff;
		payload.output_gigawords = state.acct_data.bytes_dl >> 32;
		payload.input_gigawords = state.acct_data.bytes_ul >> 32;
		payload.output_packets = state.acct_data.packets_dl;
		payload.input_packets = state.acct_data.packets_ul;
	}
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

function interface_find(mac) {
	for (let k, v in interfaces)
		if (v.clients[mac])
			return k;
	return null;
}

function radius_terminate(interface, mac, cause) {
	if (!interfaces[interface].clients[mac].radius)
		return;

	let payload = {
		acct_type: radat_stop,
		terminate_cause: cause,
	};
	debug(interface, mac, 'acct terminate: ' + cause);
	radius_acct(interface, mac, payload);
}

function radius_start(interface, mac) {
	let payload = {
		acct_type: radat_start,
	};
	debug(interface, mac, 'acct start');
	radius_acct(interface, mac, payload);
}

function radius_interim(interface, mac) {
	let payload = {
		acct_type: radat_interim,
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

// ratelimit a client from radius reply attributes
function client_ratelimit(interface, mac, state) {
	if (!(state.data?.radius?.reply))
		return;

	let reply = state.data.radius.reply;

	// check known attributes - WISPr: bps, ChiliSpot: kbps
	let maxup = reply['WISPr-Bandwidth-Max-Up'] || (reply['ChilliSpot-Bandwidth-Max-Up']*1000);
	let maxdown = reply['WISPr-Bandwidth-Max-Down'] || (reply['ChilliSpot-Bandwidth-Max-Down']*1000);

	if (!(+maxdown || +maxup))
		return;

	let args = {
		device: state.device,
		address: mac,
	};
	if (+maxdown)
		args.rate_egress = sprintf('%s', maxdown);
	if (+maxup)
		args.rate_ingress = sprintf('%s', maxup);

	ubus.call('ratelimit', 'client_set', args);
	syslog(interface, mac, 'ratelimiting client: ' + maxdown + '/' + maxup);
}

function client_add(interface, mac, state) {
	if (state.state != 1)
		return;

	let defval = 0;

	let settings = interfaces[interface].settings;
	let accounting = settings.accounting;

	// RFC: NAS local interval value *must* override RADIUS attribute
	let interval = settings.acct_interval;
	let session = settings.session_timeout;
	let idle = settings.idle_timeout;
	let max_total = 0;
	
	if (state.data?.radius?.reply) {
		interval = +(interval || state.data?.radius?.reply['Acct-Interim-Interval'] || 0);
		session = +(state.data.radius.reply['Session-Timeout'] || session);
		idle = +(state.data.radius.reply['Idle-Timeout'] || idle);
		max_total = +(state.data.radius.reply['ChilliSpot-Max-Total-Octets'] || 0);
	}

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
		if (accounting) {
			radius_start(interface, mac);
			if (interval)
				clients[mac].next_interim = time() + interval;
		}
	}
	syslog(interface, mac, 'adding client');

	// apply ratelimiting rules, if any
	client_ratelimit(interface, mac, state);
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
		system('conntrack -D -s ' + client.ip4addr);
	if (client.ip6addr)
		system('conntrack -D -s ' + client.ip6addr);

	delete interfaces[interface].clients[mac];
}

function client_remove(interface, mac, reason) {
	syslog(interface, mac, reason);
	client_kick(interface, mac, true);
	// delete ratelimit rules if any
	ubus.call('ratelimit', 'client_delete', { address: mac });
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

	let payload = {
		acct_type: radat_accton,
		acct_session: sessionid,
	};
	payload = radius_init(interface, null, payload);
	payload.acct = true;
	radius_call(interface, null, payload);
	debug(interface, null, 'acct-on call');
}

function radius_acctoff(interface)
{
	let payload = {
		acct_type: radat_acctoff,
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

	for (let mac, client in interfaces[interface].clients) {
		if (!list[mac] || !list[mac].state) {
			radius_terminate(interface, mac, radtc_lostcarrier);
			client_remove(interface, mac, 'disconnect event');
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

function hapd_subscriber_notify_cb(notify) {
	if (notify.type != 'coa')
		return 0;
	notify.data.address = uc(split(notify.data.address, ':')[0]);
	let iface = interface_find(notify.data.address);
	if (!iface)
		return 0;
	client_kick(iface, notify.data.address, true);
	return 1;
}

function hapd_subscriber_remove_cb(remove) {
	printf('remove: %.J\n', remove);
}

function unsub_object(add, id, path) {
	let object = split(path, '.');

	if (object[0] == 'hostapd' && object[1] && add) {
		printf('adding %s\n', path);
		hapd_subscriber.subscribe(path);
	}
}

function listener_cb(event, payload) {
	unsub_object(event == 'ubus.object.add', payload.id, payload.path);
}

function start()
{
	let seen = {};

	for (let interface, data in interfaces) {
		let server = data.settings.acct_server;
		let nasid = data.settings.nas_id;

		if (!server || !nasid)
			continue;
		if ((server in seen) && (nasid in seen[server]))
			continue;	// avoid sending duplicate requests to the same server for the same nasid
		if (!seen[server])
			seen[server] = {};
		seen[server][nasid] = 1;
		radius_accton(interface);
	}
	hapd_subscriber = ubus.subscriber(hapd_subscriber_notify_cb, hapd_subscriber_remove_cb);
	
	let list = ubus.list();
	for (let k, path in list)
	        unsub_object(true, 0, path);

	ubus.listener('ubus.object.add', listener_cb);
	ubus.listener('ubus.object.remove', listener_cb);
}

function stop()
{
	for (let interface, data in interfaces) {
		if (data.sessionid)	// we have previously sent Accounting-On
			radius_acctoff(interface);
	}
}

function run_service() {
	ubus.publish("uspot", {
		client_add: {
			call: function(req) {
				let interface = req.args.interface;
				let address = req.args.address;
				let state = req.args.state;
				printf('%s %s %s\n', interface, address, state);
				if (!interface || !address || !state)
					return ubus.STATUS_INVALID_ARGUMENT;

				if (!interfaces[interface].clients[address])
					client_add(interface, address, state);

				return 0;
			},
			args: {
				interface:"",
				address:"",
				state: {},
			}
		},
		client_remove: {
			call: function(req) {
				let interface = req.args.interface;
				let address = req.args.address;

				if (!interface || !address)
					return ubus.STATUS_INVALID_ARGUMENT;

				address = uc(address);

				if (interfaces[interface].clients[address]) {
					radius_terminate(interface, address, radtc_logout);
					client_remove(interface, address, 'client_remove event');
				}

				return 0;
			},
			args: {
				interface:"",
				address:"",
			},
		},
		interface_list: {
			call: function(req) {
				return interfaces;
			},
			args: {
			}
		},
	});

	try {
		start();
		uloop.timer(10000, function() {
			for (let interface in interfaces)
				accounting(interface);
			this.set(10000);
		});
		uloop.run();
	} catch (e) {
		warn(`Error: ${e}\n${e.stacktrace[0].context}`);
	}

	stop();
}

uloop.init();
run_service();
uloop.done();
