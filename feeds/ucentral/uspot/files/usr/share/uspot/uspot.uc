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
let uspots = {};

let uciload = uci.foreach('uspot', 'uspot', (d) => {
	if (!d[".anonymous"]) {
		let accounting = !!(d.acct_server && d.acct_secret);
		uspots[d[".name"]] = {
		settings: {
			accounting,
			auth_mode: d.auth_mode,
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

function syslog(uspot, mac, msg) {
	let log = sprintf('uspot: %s %s %s', uspot, mac, msg);

	system('logger \'' + log + '\'');
	warn(log + '\n');
}

function debug(uspot, mac, msg) {
	if (+uspots[uspot].settings.debug)
		syslog(uspot, mac, msg);
}

// mac re-formater
function format_mac(uspot, mac) {
	let format = uspots[uspot].settings.mac_format;

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

// wrapper for scraping external tools JSON stdout
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

/**
 * Augment and return RADIUS payload with necessary fields.
 * This function adds to an existing RADIUS payload the necessary Authentication or Accounting requests fields:
 * server, proxy, NAS-ID, and in the case of client accounting, client identification data.
 *
 * @param {string} uspot the target uspot
 * @param {?string} mac the client MAC address (for client accounting)
 * @param {object} payload the RADIUS payload
 * @param {?boolean} auth true for Radius Authentication, else Accounting request.
 * @returns {object) the augmented payload
 */
function radius_init(uspot, mac, payload, auth) {
	let settings = uspots[uspot].settings;

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
		let client = uspots[uspot].clients[mac];
		let radius = client.radius.request;
		for (let key in [ 'acct_session', 'client_ip', 'called_station', 'calling_station', 'nas_ip', 'nas_port_type', 'username', 'location_name', 'cui' ])
			if (radius[key])
				payload[key] = radius[key];
	}

	return payload;
}

/**
 * Execute "radius-client" with the provided RADIUS payload, return reply.
 *
 * @param {string} uspot the target uspot (used for debugging only)
 * @param {?string} mac the optional client MAC address
 * @param {object} payload the RADIUS payload
 * @returns {object} "radius-client" reply
 */
function radius_call(uspot, mac, payload) {
	let path = '/tmp/u' + (payload.acct ? "acct" : "auth") + (mac || payload.acct_session) + '.json';
	let cfg = fs.open(path, 'w');
	cfg.write(payload);
	cfg.close();

	let reply = json_cmd('/usr/bin/radius-client ' + path);

	if (!+uspots[uspot].settings.debug)
		fs.unlink(path);

	return reply;
}

// RADIUS Acct-Status-Type attributes
const radat_start = 1;		// Start
const radat_stop = 2;		// Stop
const radat_interim = 3;	// Interim-Update
const radat_accton = 7;		// Accounting-On
const radat_acctoff = 8;	// Accounting-Off

/**
 * Send RADIUS client accounting.
 * This function computes and populates the following RADIUS payload fields:
 * session_time, {input,output}_{octets,gigawords,packets} and class;
 * it then executes a RADIUS Accounting request with these elements.
 *
 * @param {string} uspot the target uspot
 * @param {string} mac the client MAC address
 * @param {object} payload the RADIUS payload
 */
function radius_acct(uspot, mac, payload) {
	let client = uspots[uspot].clients[mac];
	let state = uconn.call('spotfilter', 'client_get', {
		interface: uspot,
		address: mac
	}) || client;	// fallback to last known state
	if (!state)
		return;

	payload = radius_init(uspot, mac, payload);
	payload.acct = true;

	if (payload.acct_type != radat_start) {
		payload.session_time = time() - client.connect;
		payload.output_octets = state.acct_data.bytes_dl & 0xffffffff;
		payload.input_octets = state.acct_data.bytes_ul & 0xffffffff;
		payload.output_gigawords = state.acct_data.bytes_dl >> 32;
		payload.input_gigawords = state.acct_data.bytes_ul >> 32;
		payload.output_packets = state.acct_data.packets_dl;
		payload.input_packets = state.acct_data.packets_ul;
	}
	if (state.data?.radius?.reply?.Class)
		payload.class = state.data.radius.reply.Class;

	radius_call(uspot, mac, payload);
}

// RADIUS Acct-Terminate-Cause attributes
const radtc_logout = 1;		// User Logout
const radtc_lostcarrier = 2;	// Lost Carrier
const radtc_idleto = 4;		// Idle Timeout
const radtc_sessionto = 5;	// Session Timeout
const radtc_adminreset = 6;	// Admin Reset

/**
 * Terminate a client RADIUS accounting.
 *
 * @param {string} uspot the target uspot
 * @param {string} mac the client MAC address
 * @param {number} cause the RADIUS Acct-Terminate-Cause value
 */
function radius_terminate(uspot, mac, cause) {
	if (!uspots[uspot].clients[mac].radius)
		return;

	let payload = {
		acct_type: radat_stop,
		terminate_cause: cause,
	};
	debug(uspot, mac, 'acct terminate: ' + cause);
	radius_acct(uspot, mac, payload);
}

/**
 * Start a client RADIUS accounting.
 *
 * @param {string} uspot the target uspot
 * @param {string} mac the client MAC address
 */
function radius_start(uspot, mac) {
	let payload = {
		acct_type: radat_start,
	};
	debug(uspot, mac, 'acct start');
	radius_acct(uspot, mac, payload);
}

/**
 * Send interim client RADIUS accounting.
 *
 * @param {string} uspot the target uspot
 * @param {string} mac the client MAC address
 */
function radius_interim(uspot, mac) {
	let payload = {
		acct_type: radat_interim,
	};
	radius_acct(uspot, mac, payload);
	debug(uspot, mac, 'iterim acct call');
}

/**
 * Uspot internal client accounting.
 * This function keeps track of the last known spotfilter accounting data,
 * and optionally sends interim RADIUS reports if configured
 *
 * @param {string} uspot the target uspot
 * @param {string} mac the client MAC address
 * @param {?number} time the UNIX time of the report (only for RADIUS requests)
 */
function client_interim(uspot, mac, time) {
	let client = uspots[uspot].clients[mac];

	// preserve a copy of last spotfilter stats for use in disconnect case
	let state = uconn.call('spotfilter', 'client_get', {
		interface: uspot,
		address: mac
	});
	if (!state)
		return;

	client.acct_data = state.acct_data;

	if (!client.interval)
		return;

	if (time >= client.next_interim) {
		radius_interim(uspot, mac);
		client.next_interim += client.interval;
	}
}

/**
 * Apply RADIUS-provided client bandwidth limits.
 * This function parses a radius reply for the following attributes:
 * 'WISPr-Bandwidth-Max-{Up,Down}' or 'ChilliSpot-Bandwidth-Max-{Up,Down}'
 * and enforces these limits if present.
 *
 * @param {string} uspot the target uspot
 * @param {string} mac the client MAC address
 */
function client_ratelimit(uspot, mac) {
	let client = uspots[uspot].clients[mac];

	if (!(client.radius?.reply))
		return;

	let reply = client.radius.reply;

	// check known attributes - WISPr: bps, ChiliSpot: kbps
	let maxup = reply['WISPr-Bandwidth-Max-Up'] || (reply['ChilliSpot-Bandwidth-Max-Up']*1000);
	let maxdown = reply['WISPr-Bandwidth-Max-Down'] || (reply['ChilliSpot-Bandwidth-Max-Down']*1000);

	if (!(+maxdown || +maxup))
		return;

	let args = {
		device: client.device,
		address: mac,
	};
	if (+maxdown)
		args.rate_egress = sprintf('%s', maxdown);
	if (+maxup)
		args.rate_ingress = sprintf('%s', maxup);

	uconn.call('ratelimit', 'client_set', args);
	syslog(uspot, mac, 'ratelimiting client: ' + maxdown + '/' + maxup);
}

/**
 * Add an authenticated but not yet validated client to the backend.
 * This function adds a client that passed authentication, but hasn't yet been enabled.
 * If the client isn't subsequently enabled, it will be purged after a 60s grace period.
 *
 * @param {string} uspot the target uspot
 * @param {string} mac the client MAC address
 */
function client_create(uspot, mac, payload)
{
	let client = {
		connect: time(),
		data: {},
		...payload,
		state: 0,
	};

	uspots[uspot].clients[mac] = client;

	// if debug, save entire client payload to spotfilter
	if (uspots[uspot].settings.debug)
		uconn.call('spotfilter', 'client_set', {
			interface: uspot,
			address: mac,
			data: client,
		});

	debug(uspot, mac, 'creating client');
}

/**
 * Enable an authenticated client to pass traffic.
 * This function authorizes a client, applying RADIUS-provided limits if any:
 * 'Acct-Interim-Interval', 'Session-Timeout', 'Idle-Timeout' and 'ChilliSpot-Max-Total-Octets'.
 * It updates spotfilter state for authorization, starts RADIUS accounting and ratelimit as needed.
 *
 * @param {string} uspot the target uspot
 * @param {string} mac the client MAC address
 */
function client_enable(uspot, mac) {
	let defval = 0;

	let settings = uspots[uspot].settings;
	let accounting = settings.accounting;

	let radius = uspots[uspot].clients[mac]?.radius;

	// RFC: NAS local interval value *must* override RADIUS attribute
	defval = settings.acct_interval;
	let interval = +(defval || radius?.reply?.['Acct-Interim-Interval'] || 0);

	defval = settings.session_timeout;
	let session = +(radius?.reply?.['Session-Timeout'] || defval);

	defval = settings.idle_timeout;
	let idle = +(radius?.reply?.['Idle-Timeout'] || defval);

	let max_total = +(radius?.reply?.['ChilliSpot-Max-Total-Octets'] || 0);

	let cui = radius?.reply?.['Chargeable-User-Identity'];

	let client = {
		... uspots[uspot].clients[mac] || {},
		state: 1,
		interval,
		session,
		idle,
		max_total,
	};
	if (radius?.request && accounting && interval)
		client.next_interim = time() + interval;
	if (cui)
		client.radius.request['cui'] = cui;

	let spotfilter = uconn.call('spotfilter', 'client_get', {
		interface: uspot,
		address: mac,
	});

	client.device = spotfilter.device;
	if (spotfilter?.ip4addr)
		client.ip4addr = spotfilter.ip4addr;
	if (spotfilter?.ip6addr)
		client.ip6addr = spotfilter.ip6addr;

	// tell spotfilter this client is allowed
	uconn.call('spotfilter', 'client_set', {
		interface: uspot,
		address: mac,
		state: 1,
		dns_state: 1,
		accounting: accounting ? [ "dl", "ul"] : [],
		data: uspots[uspot].settings.debug ? client : {},
	});

	uspots[uspot].clients[mac] = client;
	syslog(uspot, mac, 'adding client');

	// start RADIUS accounting
	if (accounting && radius?.request)
		radius_start(uspot, mac);

	// apply ratelimiting rules, if any
	client_ratelimit(uspot, mac);
}

/**
 * Kick a client from the system.
 * This function deletes a client from the backend, and removes existing connection flows.
 *
 * @param {string} uspot the target uspot
 * @param {string} mac the client MAC address
 * @param {boolean} remove calls 'spotfilter client_remove' if true, otherwise client state is only reset to 0
 */
function client_kick(uspot, mac, remove) {
	debug(uspot, mac, 'stopping accounting');
	let payload = {
		interface: uspot,
		address: mac,
		...(remove ? {} : {
			state: 0,
			dns_state: 1,
			accounting: [],
			flush: true,
		}),
	};

	uconn.call('spotfilter', remove ? 'client_remove' : 'client_set', payload);

	let client = uspots[uspot].clients[mac];

	if (client.ip4addr)
		system('conntrack -D -s ' + client.ip4addr);
	if (client.ip6addr)
		system('conntrack -D -s ' + client.ip6addr);

	delete uspots[uspot].clients[mac];
}

function client_remove(uspot, mac, reason) {
	syslog(uspot, mac, reason);
	client_kick(uspot, mac, true);
	// delete ratelimit rules if any
	uconn.call('ratelimit', 'client_delete', { address: mac });
}

function client_reset(uspot, mac, reason) {
	syslog(uspot, mac, reason);
	client_kick(uspot, mac, false);
}

/**
 * Signal beginning of RADIUS accounting for this NAS.
 *
 * @param {string} uspot the target uspot
 */
function radius_accton(uspot)
{
	// assign a global uspot session ID for Accounting-On/Off messages
	let sessionid = generate_sessionid();

	uspots[uspot].sessionid = sessionid;

	let payload = {
		acct_type: radat_accton,
		acct_session: sessionid,
	};
	payload = radius_init(uspot, null, payload);
	payload.acct = true;
	radius_call(uspot, null, payload);
	debug(uspot, null, 'acct-on call');
}

/**
 * Signal end of RADIUS accounting for this NAS.
 *
 * @param {string} uspot the target uspot
 */
function radius_acctoff(uspot)
{
	let payload = {
		acct_type: radat_acctoff,
		acct_session: uspots[uspot].sessionid,
	};
	payload = radius_init(uspot, null, payload);
	payload.acct = true;
	radius_call(uspot, null, payload);
	debug(uspot, null, 'acct-off call');
}

/**
 * Perform accounting housekeeping for a uspot.
 * This function goes throught the list of known clients and performs:
 * - cleanup authenticated but not enabled clients after 60s grace period;
 * - cleanup when a client is no longer known to spotfilter;
 * - client termination on idle timeout, session timeout or data budget expiration.
 * - RADIUS interim accounting report
 *
 * @param {string} uspot the target uspot
 */
function accounting(uspot) {
	let list = uconn.call('spotfilter', 'client_list', { interface: uspot });
	let t = time();
	let accounting = uspots[uspot].settings.accounting;

	for (let mac, client in uspots[uspot].clients) {
		if (!client.state) {
			const stale = 60;	// 60s timeout for (new) unauth clients
			if ((t - client.connect) > stale)
				client_remove(uspot, mac, 'stale client');
			continue;
		}

		if (!list[mac] || !list[mac].state) {
			radius_terminate(uspot, mac, radtc_lostcarrier);
			client_remove(uspot, mac, 'disconnect event');
			continue;
		}

		if (+list[mac].idle > +client.idle) {
			radius_terminate(uspot, mac, radtc_idleto);
			client_reset(uspot, mac, 'idle event');
			continue;
		}
		let timeout = +client.session;
		if (timeout && ((t - client.connect) > timeout)) {
			radius_terminate(uspot, mac, radtc_sessionto);
			client_reset(uspot, mac, 'session timeout');
			continue;
		}
		let maxtotal = +client.max_total;
		if (maxtotal && ((list[mac].acct_data.bytes_ul + list[mac].acct_data.bytes_dl) >= maxtotal)) {
			radius_terminate(uspot, mac, radtc_sessionto);
			client_reset(uspot, mac, 'max octets reached');
			continue;
		}

		if (accounting)
			client_interim(uspot, mac, t);
	}
}

function start()
{
	let seen = {};

	// for each unique uspot/nasid with configured RADIUS, send Accounting-On
	for (let uspot, data in uspots) {
		let server = data.settings.acct_server;
		let nasid = data.settings.nas_id;

		if (!server || !nasid)
			continue;
		if ((server in seen) && (nasid in seen[server]))
			continue;	// avoid sending duplicate requests to the same server for the same nasid
		if (!seen[server])
			seen[server] = {};
		seen[server][nasid] = 1;
		radius_accton(uspot);
	}
}

function stop()
{
	for (let uspot, data in uspots) {
		if (data.sessionid)	// we have previously sent Accounting-On
			radius_acctoff(uspot);
	}
}

function run_service() {
	uconn.publish("uspot", {
		client_auth: {
			call: function(req) {
				let uspot = req.args.uspot;
				let address = req.args.address;
				let client_ip = req.args.client_ip;
				let username = req.args.username;
				let password = req.args.password;
				let challenge = req.args.challenge;
				let ssid = req.args.ssid;
				let sessionid = req.args.sessionid || generate_sessionid();
				let reqdata = req.args.reqdata;

				let try_macauth = false;

				if (!uspot || !address || !client_ip)
					return { 'access-accept': 0 };

				if (!(uspot in uspots))
					return { 'access-accept': 0 };

				let settings = uspots[uspot].settings;
				address = uc(address);	// spotfilter uses ether_ntoa() which is uppercase

				// prepare client payload
				let payload = {
					data: {
						sessionid,
						username,	// XXX does anything use this? comes from handler.uc radius & credentials auth
						client_ip,	// not used, could be useful
					},
				};
				if (ssid)
					payload.data.ssid = ssid;

				// click-to-continue: always accept - portal is responsible for checking conditions are met
				if (settings.auth_mode == 'click-to-continue') {
					client_create(uspot, address, payload);
					return { 'access-accept': 1 };
				}

				// credentials
				if (settings.auth_mode == 'credentials') {
					let match = false;
					uci.foreach('uspot', 'credentials', (d) => {
						// check if the credentials are valid
						if (d.uspot != uspot)
							return;
						if (d.username == username && d.password == password)
							match = true;
					});
					if (match)
						client_create(uspot, address, payload);
					return { 'access-accept': match };
				}

				// else, radius/uam - give up early if no server set
				if (!settings.auth_server)
					return { 'access-accept': 0 };

				if (!username && !password) {
					if  (!+settings.mac_auth)	// don't try mac-auth if not allowed
						return { 'access-accept': 0 };
					else
						try_macauth = true;
				}

				let fmac = format_mac(uspot, address);

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

				request = radius_init(uspot, address, request, true);

				let radius = radius_call(uspot, address, request);

				if (radius['access-accept']) {
					delete request.server;	// don't publish RADIUS server secret
					payload.radius = { reply: radius.reply, request };	// save RADIUS payload for later use
					client_create(uspot, address, payload);
				}

				delete radius.reply;
				return radius;
			},
			/*
			 @param uspot: REQUIRED: target uspot
			 @param address: REQUIRED: client MAC address
			 @param client_ip: REQUIRED: client IP
			 @param username: OPTIONAL: client username
			 @param password: OPTIONAL: client password or CHAP password
			 @param challenge: OPTIONAL: client CHAP challenge
			 @param ssid: OPTIONAL: client SSID
			 @param sessionid: OPTIONAL: accounting session ID
			 @param reqdata: OPTIONAL: additional RADIUS request data - to be passed verbatim to radius-client
			 @param return {object} "{"access-accept":N}" where N==1 if auth succeeded, 0 otherwise

			 operation:
			  - call with (uspot, address, client_ip) -> click-to-continue or RADIUS MAC authentication
			  - call with (uspot, address, client_ip, username, password) -> credentials or RADIUS password auth
			  - call with (uspot, address, client_ip, username, password, challenge) -> RADIUS CHAP challenge auth
			 */
			args: {
				uspot:"",
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
		client_enable: {
			call: function(req) {
				let uspot = req.args.uspot;
				let address = req.args.address;

				if (!uspot || !address)
					return ubus.STATUS_INVALID_ARGUMENT;
				if (!(uspot in uspots))
					return ubus.STATUS_INVALID_ARGUMENT;

				address = uc(address);

				// enabling clients can only be done for known ones (i.e. those which passed authentication)
				if (!uspots[uspot].clients[address])
					return ubus.STATUS_NOT_FOUND;

				address = uc(address);	// spotfilter uses ether_ntoa() which is uppercase

				client_enable(uspot, address);

				return 0;
			},
			/*
			 Enable a previously authenticated client to pass traffic.
			 XXX REVIEW: we might want to squash that into client_auth()
			 @param uspot: REQUIRED: target uspot
			 @param address: REQUIRED: client MAC address
			 */
			args: {
				uspot:"",
				address:"",
			}
		},
		client_remove: {
			call: function(req) {
				let uspot = req.args.uspot;
				let address = req.args.address;

				if (!uspot || !address)
					return ubus.STATUS_INVALID_ARGUMENT;
				if (!(uspot in uspots))
					return ubus.STATUS_INVALID_ARGUMENT;

				address = uc(address);

				if (uspots[uspot].clients[address]) {
					radius_terminate(uspot, address, radtc_logout);
					client_remove(uspot, address, 'client_remove event');
				}

				return 0;
			},
			/*
			 Delete a client from the system.
			 @param uspot: REQUIRED: target uspot
			 @param address: REQUIRED: client MAC address
			 */
			args: {
				uspot:"",
				address:"",
			}
		},
		client_get: {
			call: function(req) {
				let uspot = req.args.uspot;
				let address = req.args.address;

				if (!uspot || !address)
					return ubus.STATUS_INVALID_ARGUMENT;
				if (!(uspot in uspots))
					return ubus.STATUS_INVALID_ARGUMENT;

				address = uc(address);

				if (!uspots[uspot].clients[address])
					return {};

				let client = uspots[uspot].clients[address];

				return client.data || {};
			},
			/*
			 Get a client public state.
			 @param uspot: REQUIRED: target uspot
			 @param address: REQUIRED: client MAC address
			 */
			args: {
				uspot:"",
				address:"",
			}
		},
		client_list: {
			call: function(req) {
				let uspot = req.args.uspot;

				if (!uspot)
					return ubus.STATUS_INVALID_ARGUMENT;
				if (!(uspot in uspots))
					return ubus.STATUS_INVALID_ARGUMENT;

				let clients = uspots[uspot].clients;

				let payload = {};
				payload[uspot] = keys(clients);

				return payload;
			},
			/*
			 List all clients for a given uspot.
			 @param uspot: REQUIRED: target uspot
			 */
			args: {
				uspot:"",
			}
		},
	});

	try {
		start();
		uloop.timer(10000, function() {
			for (let uspot in uspots)
				accounting(uspot);
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
