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

function acct_interval(interface) {
	return config[interface]?.acct_interval || 600;
}

function idle_timeout(interface) {
	return config[interface].idle_timeout || 600;
}

function session_timeout(interface) {
	return config[interface].session_timeout || 0;
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

function get_idle_timeout(interface, mac) {
	if (clients[interface][mac])
		return +clients[interface][mac].idle;
	return idle_timeout(interface);
}

function get_session_timeout(interface, mac) {
	if (clients[interface][mac]?.session)
		return +clients[interface][mac].session;
	return session_timeout(interface);
}

function radius_available(interface,mac) {
	return !!clients[interface][mac]?.radius;
}

function radius_init(interface, mac, payload) {
	for (let key in [ 'server', 'acct_server', 'acct_session', 'client_ip', 'called_station', 'calling_station', 'nas_ip', 'nas_id', 'username' ])
		if (clients[interface][mac].radius[key])
			payload[key] = clients[interface][mac].radius[key];

	if (config[interface]?.acct_proxy)
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

function radius_stop(interface, mac, payload, remove) {
	if (!radius_available(interface, mac))
		return;
	debug(interface, mac, 'stopping accounting');
	ubus.call('spotfilter', remove ? 'client_remove' : 'client_set', payload);
	system('conntrack -D -s ' + clients[interface][mac].ip4addr  + ' -m 2');
	if (clients[interface][mac].accounting)
		clients[interface][mac].timeout.cancel();
	delete clients[interface][mac];
}

function radius_acct(interface, mac, payload) {
	let state = ubus.call('spotfilter', 'client_get', {
		interface,
		address: mac
	});
	if (!state) {
		return false;
	}

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
	return true;
}

function radius_idle_time(interface, mac) {
	if (!radius_available(interface, mac))
		return;
	let payload = {
		acct_type: 2,
		terminate_cause: 4,
	};
	radius_acct(interface, mac, payload);
}

function radius_session_time(interface, mac) {
	if (!radius_available(interface, mac))
		return;
	let payload = {
		acct_type: 2,
		terminate_cause: 5,
	};
	radius_acct(interface, mac, payload);
}

function radius_logoff(interface, mac) {
	if (!radius_available(interface, mac))
		return;
	let payload = {
		acct_type: 2,
		terminate_cause: 1,
	};
	radius_acct(interface, mac, payload);
}

function radius_disconnect(interface, mac) {
	if (!radius_available(interface, mac))
		return;
	let payload = {
		acct_type: 2,
		terminate_cause: 1,
	};
	radius_acct(interface, mac, payload);
}

function radius_interim(interface, mac) {
	if (!radius_available(interface, mac))
		return;
	let payload = {
		acct_type: 3,
	};
	if (radius_acct(interface, mac, payload))
		debug(interface, mac, 'iterim acct call');
	else
		syslog(interface, mac, 'failed to send interim accounting frame\n');
	clients[interface][mac].timeout.set(clients[interface][mac].interval);
}

function client_add(interface, mac, state) {
	if (state.state != 1)
		return;

	let interval = acct_interval(interface) * 1000;
	let idle = idle_timeout(interface);
	let session = session_timeout(interface);
	let accounting = (config[interface]?.acct_server && config[interface]?.acct_secret);
	let max_total = 0;

	if (state.data?.radius?.reply) {
		interval = (state.data?.radius?.reply['Acct-Interim-Interval'] || acct_interval(interface)) * 1000;
		idle = (state.data?.radius?.reply['Idle-Timeout'] || idle_timeout(interface));
		session = (state.data?.radius?.reply['Session-Timeout'] || session_timeout(interface));
		max_total = (state.data?.radius?.reply['ChilliSpot-Max-Total-Octets'] || 0);
	}

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
	if (state.data?.radius?.request)
		clients[interface][mac].radius = state.data.radius.request;
	syslog(interface, mac, 'adding client');
	if (accounting)
		clients[interface][mac].timeout = uloop.timer(interval, () => radius_interim(interface, mac));
}

function client_remove(interface, mac, reason) {
	syslog(interface, mac, reason);
	radius_stop(interface, mac, {
		interface,
		address: mac
	}, true);
}

function client_flush(interface, mac) {
	syslog(interface, mac, 'logoff event');
	radius_stop(interface, mac, {
		interface,
		address: mac,
		state: 0,
		dns_state: 1,
		accounting: [],
		flush: true
	});
}

function client_timeout(interface, mac, reason) {
	syslog(interface, mac, reason);
	radius_stop(interface, mac, {
		interface,
		state: 0,
		dns_state: 1,
		address: mac,
		accounting: [],
		flush: true,
	});
}


function accounting(interface) {
	let list = ubus.call('spotfilter', 'client_list', { interface });
	let t = time();

	for (let k, v in list)
		if (!clients[interface][k])
			client_add(interface, k, v);

	for (let k, v in clients[interface]) {
		if (list[k].data?.logoff) {
			radius_logoff(interface, k);
			client_flush(interface, k);
			continue;
		}

		if (!list[k] || !list[k].state) {
			radius_disconnect(interface, k);
			client_remove(interface, k, 'disconnect event');
			continue;
		}

		if (list[k].idle > get_idle_timeout(interface, k)) {
			if (clients[interface][k])
				radius_idle_time(interface, k);
			client_remove(interface, k, 'idle event');
			continue;
		}
		let timeout = get_session_timeout(interface, k);
		if (timeout && ((t - list[k].data.connect) > timeout)) {
			if (clients[interface][k])
				radius_session_time(interface, k);
			client_timeout(interface, k, 'session timeout');
			continue;
		}
		if (clients[interface][k].max_total) {
			let total = list[k].bytes_ul + list[k].bytes_dl;
			if (total >= clients[interface][k].max_total) {
				radius_session_time(interface, k);
				client_timeout(interface, k, 'max octets reached');
			}
		}
	}
}

uloop.init();

uloop.timer(1000, function() {
	for (let interface, v in clients)
		accounting(interface); 
	this.set(1000);
});

uloop.run();
