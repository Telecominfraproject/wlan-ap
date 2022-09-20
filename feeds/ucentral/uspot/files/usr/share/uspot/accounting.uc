#!/usr/bin/ucode

'use strict';

let fs = require('fs');
let uloop = require('uloop');
let ubus = require('ubus').connect();
let uci = require('uci').cursor();
let config = uci.get_all('uspot');
let clients = {};

let acct_interval = config.radius?.acct_interval || 600;
let idle_timeout = config.config.idle_timeout || 600;
let session_timeout = config.config.session_timeout || 0;

function syslog(mac, msg) {
	let log = sprintf('uspot: %s %s', mac, msg);

	system('logger ' + log);
	warn(log + '\n');
}

function debug(mac, msg) {
	if (config.config.debug)
		syslog(mac, msg);
}

function get_idle_timeout(mac) {
	if (clients[mac])
		return clients[mac].idle;
	return idle_timeout;
}

function get_session_timeout(mac) {
	if (clients[mac]?.session_timeout)
		return clients[mac].session_timeout;
	return session_timeout;
}

function radius_init(mac, payload) {
	for (let key in [ 'server', 'acct_server', 'acct_session', 'client_ip', 'called_station', 'calling_station', 'nas_ip', 'nas_id', 'username' ])
		if (clients[mac].radius[key])
			payload[key] = clients[mac].radius[key];
	return payload;
}

function radius_call(mac, payload) {
	let cfg = fs.open('/tmp/acct' + mac + '.json', 'w');
	cfg.write(payload);
	cfg.close();

	system('/usr/bin/radius-client /tmp/acct' + mac + '.json');
}

function radius_stop(mac) {
	debug(mac, 'stopping accounting');

	let payload = {
		acct: true,
		acct_type: 8,
		terminate_cause: 0,
	};
	radius_init(mac, payload);
	radius_call(mac, payload);
}

function radius_acct(mac, payload) {
	let state = ubus.call('spotfilter', 'client_get', {
		interface: 'hotspot',
		address: mac
	});
	if (!state) {
		return false;
	}

	payload = radius_init(mac, payload);
	payload.acct = true;
	payload.session_time = time() - state.data.connect;
	payload.output_octets = state.bytes_dl & 0xffffffff;
	payload.input_octets = state.bytes_ul & 0xffffffff;
	payload.output_gigawords = state.bytes_dl >> 32;
	payload.input_gigawords = state.bytes_ul >> 32;
	payload.output_packets = state.packets_dl;
	payload.input_packets = state.packets_ul;

	radius_call(mac, payload);
	return true;
}

function radius_idle_time(mac) {
	let payload = {
		acct_type: 2,
		terminate_cause: 4,
	};
	radius_acct(mac, payload);
}

function radius_session_time(mac) {
	let payload = {
		acct_type: 2,
		terminate_cause: 5,
	};
	radius_acct(mac, payload);
}

function radius_disconnect(mac) {
	let payload = {
		acct_type: 2,
		terminate_cause: 1,
	};
	radius_acct(mac, payload);
}

function radius_interim(mac) {
	let payload = {
		acct_type: 3,
	};
	if (radius_acct(mac, payload))
		debug(mac, 'iterim acct call');
	else
		syslog(mac, 'failed to sent interim accounting frame\n');
	clients[mac].timeout.set(clients[mac].interval);
}

function client_add(mac, state) {
	if (state.state != 1)
		return;

	let interval = (state.data?.radius?.reply['Acct-Interim-Interval'] || acct_interval) * 1000;
	let idle = (state.data?.radius?.reply['Idle-Timeout'] || idle_timeout);
	let session = (state.data?.radius?.reply['Session-Timeout'] || session_timeout);
	let accounting = (config.radius?.acct_server && config.radius?.acct_secret);

	clients[mac] = {
		accounting,
		radius: state.data.radius.request,
		interval,
		idle,
	};
	syslog(mac, 'adding client');
	if (accounting)
		clients[mac].timeout = uloop.timer(interval, () => radius_interim(mac));
}

function client_remove(mac, reason) {
	syslog(mac, reason);
	if (clients[mac]) {
		radius_stop(mac);
		if (clients[mac].accounting)
			clients[mac].timeout.cancel();
		delete clients[mac];
	}
	ubus.call('spotfilter', 'client_remove', {
			interface: "hotspot",
			address: mac
		});
}

function client_timeout(mac) {
	syslog(mac, 'session timeout');
	if (clients[mac]) {
		radius_stop(mac);
		if (clients[mac].accounting)
			clients[mac].timeout.cancel();
		delete clients[mac];
	}
	ubus.call('spotfilter', 'client_set', {
			interface: "hotspot",
			state: 0,
			address: mac,
			accounting: [],
			flush: true,
		});
}

uloop.init();

uloop.timer(1000, function() {
	let list = ubus.call('spotfilter', 'client_list', { interface: 'hotspot'});
	let t = time();

	for (let k, v in list)
		if (!clients[k])
			client_add(k, v);

	for (let k, v in clients)
		if (!list[k] || !list[k].state) {
			radius_disconnect(k);
			client_remove(k, 'disconnect event');
		}

	for (let k, v in list) {
		if (v.idle > get_idle_timeout(k)) {
			if (clients[k])
				radius_idle_time(k);
			client_remove(k, 'idle event');

		}
		let timeout = get_session_timeout(k);
		if (timeout && ((t - v.data.connect) > timeout)) {
			if (clients[k])
				radius_session_time(k);
			client_timeout(k);

		}
	}

	this.set(5000);
});

uloop.run();
