#!/usr/bin/ucode
// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2022-2023 John Crispin <john@phrozen.org>
// SPDX-FileCopyrightText: 2023 Thibaut Varène <hacks@slashdirt.org>

'use strict';

let fs = require('fs');
let uloop = require('uloop');
let ubus = require('ubus');
let uconn = ubus.connect();
let uci = require('uci').cursor();
let interfaces = {};

let uciload = uci.foreach('uspot', 'uspot', (d) => {
	if (!d[".anonymous"]) {
		let accounting = !!(d.acct_server && d.acct_secret);
		interfaces[d[".name"]] = {
		settings: {
			accounting,
			auth_server: d.auth_server,
			auth_secret: d.auth_secret,
			auth_port: d.auth_port || 1812,
			auth_proxy: d.auth_proxy,
			acct_server: d.acct_server,
			acct_secret: d.acct_secret,
			acct_port: d.acct_port || 1813,
			acct_proxy: d.acct_proxy,
			acct_interval: d.acct_interval,
			nas_id: d.nasid,
			nas_mac: d.nasmac,
			mac_auth: d.mac_auth,
			mac_passwd: d.mac_passwd,
			mac_suffix: d.mac_suffix,
			mac_format: d.mac_format,
			location_name: d.location_name,
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

// mac re-formater
function format_mac(interface, mac) {
	let format = interfaces[interface].settings.mac_format;

	switch(format) {
	case 'aabbccddeeff':
	case 'AABBCCDDEEFF':
		mac = replace(mac, ':', '');
		break;
	case 'aa-bb-cc-dd-ee-ff':
	case 'AA-BB-CC-DD-EE-FF':
		mac = replace(mac, ':', '-');
		break;
	}

	switch(format) {
	case 'aabbccddeeff':
	case 'aa-bb-cc-dd-ee-ff':
	case 'aa:bb:cc:dd:ee:ff':
		mac = lc(mac);
		break;
	case 'AABBCCDDEEFF':
	case 'AA:BB:CC:DD:EE:FF':
	case 'AA-BB-CC-DD-EE-FF':
		mac = uc(mac);
		break;
	}

	return mac;
}

function json_cmd(cmd) {
	let stdout = fs.popen(cmd);
	if (!stdout)
		return null;

	let reply = null;
	try {
		reply = json(stdout.read('all'));
	} catch(e) {
	}
	stdout.close();
	return reply;
}

function generate_sessionid() {
	let math = require('math');
	let sessionid = '';

	for (let i = 0; i < 16; i++)
		sessionid += sprintf('%x', math.rand() % 16);

	return sessionid;
}

function radius_init(interface, mac, payload, auth) {
	let settings = interfaces[interface].settings;

	if (auth) {
		payload.server = sprintf('%s:%s:%s', settings.auth_server, settings.auth_port, settings.auth_secret);
		if (settings.auth_proxy)
			payload.auth_proxy = settings.auth_proxy;
		payload.nas_port_type = 19;	// wireless
	}
	else {
		payload.acct_server = sprintf('%s:%s:%s', settings.acct_server, settings.acct_port, settings.acct_secret);
		if (settings.acct_proxy)
			payload.acct_proxy = settings.acct_proxy;
	}

	payload.nas_id = settings.nas_id;	// XXX RFC says NAS-IP is not required when NAS-ID is set, but it's added by libradcli anyway
	if (settings.location_name)
		payload.location_name = settings.location_name;

	if (!auth && mac) {
		// dealing with client accounting
		let client = interfaces[interface].clients[mac];
		for (let key in [ 'acct_session', 'client_ip', 'called_station', 'calling_station', 'nas_ip', 'nas_port_type', 'username', 'location_name' ])
			if (client.radius[key])
				payload[key] = client.radius[key];
	}

	return payload;
}

function radius_call(interface, mac, payload) {
	let path = '/tmp/u' + (payload.acct ? "acct" : "auth") + (mac || payload.acct_session) + '.json';
	let cfg = fs.open(path, 'w');
	cfg.write(payload);
	cfg.close();

	let reply = json_cmd('/usr/bin/radius-client ' + path);

	if (!+interfaces[interface].settings.debug)
		fs.unlink(path);

	return reply;
}

// RADIUS Acct-Status-Type attributes
const radat_start = 1;		// Start
const radat_stop = 2;		// Stop
const radat_interim = 3;	// Interim-Update
const radat_accton = 7;		// Accounting-On
const radat_acctoff = 8;	// Accounting-Off

function radius_acct(interface, mac, payload) {
	let state = uconn.call('spotfilter', 'client_get', {
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
	let state = uconn.call('spotfilter', 'client_get', {
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

	uconn.call('ratelimit', 'client_set', args);
	syslog(interface, mac, 'ratelimiting client: ' + maxdown + '/' + maxup);
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

	uconn.call('spotfilter', remove ? 'client_remove' : 'client_set', payload);

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
	uconn.call('ratelimit', 'client_delete', { address: mac });
}

function client_reset(interface, mac, reason) {
	syslog(interface, mac, reason);
	client_kick(interface, mac, false);
}

function radius_accton(interface)
{
	// assign a global interface session ID for Accounting-On/Off messages
	let sessionid = generate_sessionid();

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
	let list = uconn.call('spotfilter', 'client_list', { interface });
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

// give a client access to the internet
function allow_client(interface, address, data) {
	syslog(interface, address, 'allow client to pass traffic');
	ubus.call('spotfilter', 'client_set', {
		interface,
		address,
		state: 1,
		dns_state: 1,
		accounting: [ "dl", "ul"],
		data: {
			... data || {},
			connect: time(),
		}
	});
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
}

function stop()
{
	for (let interface, data in interfaces) {
		if (data.sessionid)	// we have previously sent Accounting-On
			radius_acctoff(interface);
	}
}

function run_service() {
	uconn.publish("uspot", {
		client_auth: {
			call: function(req) {
				let interface = req.args.interface;
				let address = req.args.address;
				let client_ip = req.args.client_ip;
				let username = req.args.username;
				let password = req.args.password;
				let challenge = req.args.challenge;
				let ssid = req.args.ssid;
				let sessionid = req.args.sessionid || generate_sessionid();
				let reqdata = req.args.reqdata;

				let try_macauth = false;

				if (!interface || !address || !client_ip)
					return { 'access-accept': 0 };

				if (!(interface in interfaces))
					return { 'access-accept': 0 };

				let settings = interfaces[interface].settings;

				if (!username && !password) {
					if  (!+settings.mac_auth)	// don't try mac-auth if not allowed
						return { 'access-accept': 0 };
					else
						try_macauth = true;
				}

				let fmac = format_mac(interface, address);

				let request = {
					username,
					calling_station: fmac,
					called_station: settings.nas_mac + ':' + ssid,
					acct_session: sessionid,
					client_ip,
					... reqdata || {},
				};

				if (try_macauth) {
					request.username = fmac + (settings.mac_suffix || '');
					request.password = settings.mac_passwd || fmac;
					request.service_type = 10;	// Call-Check, see https://wiki.freeradius.org/guide/mac-auth#web-auth-safe-mac-auth
				}
				else if (challenge) {
					request.chap_password = password;
					request.chap_challenge = challenge;
				}
				else
					request.password = password;

				request = radius_init(interface, address, request, true);

				let radius = radius_call(interface, address, request);

				if (radius['access-accept']) {
					delete request.server;	// don't publish RADIUS server secret
					allow_client(interface, address, {
						username,	// XXX does anything use this? comes from handler.uc radius & credentials auth
						radius: { reply: radius.reply, request }
					});	// XXX && client_add()
				}

				delete radius.reply;
				return radius;
			},
			/*
			 @param interface: REQUIRED: uspot interface
			 @param address: REQUIRED: client MAC address
			 @param client_ip: REQUIRED: client IP
			 @param username: OPTIONAL: client username
			 @param password: OPTIONAL: client password or CHAP password
			 @param challenge: OPTIONAL: client CHAP challenge
			 @param ssid: OPTIONAL: client SSID
			 @param sessionid: OPTIONAL: accounting session ID
			 @param reqdata: OPTIONAL: additional RADIUS request data - to be passed verbatim to radius-client

			 operation:
			  - call with (interface, address, client_ip) -> RADIUS MAC authentication
			  - call with (interface, address, client_ip, username, password) -> RADIUS password auth
			  - call with (interface, address, client_ip, username, password, challenge) -> RADIUS CHAP challenge auth
			 */
			args: {
				interface:"",
				address:"",
				client_ip:"",
				username:"",
				password:"",
				challenge:"",
				ssid:"",
				sessionid:"",
				reqdata:{},
			}
		},
		client_add: {
			call: function(req) {
				let interface = req.args.interface;
				let address = req.args.address;

				if (!interface || !address)
					return ubus.STATUS_INVALID_ARGUMENT;
				if (!(interface in interfaces))
					return ubus.STATUS_INVALID_ARGUMENT;

				address = uc(address);	// spotfilter uses ether_ntoa() which is uppercase

				let state = uconn.call('spotfilter', 'client_get', {
					interface,
					address,
				});
				if (!state)
					return ubus.STATUS_INVALID_ARGUMENT;

				if (!interfaces[interface].clients[address])
					client_add(interface, address, state);

				return 0;
			},
			args: {
				interface:"",
				address:"",
			}
		},
		client_remove: {
			call: function(req) {
				let interface = req.args.interface;
				let address = req.args.address;

				if (!interface || !address)
					return ubus.STATUS_INVALID_ARGUMENT;
				if (!(interface in interfaces))
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
