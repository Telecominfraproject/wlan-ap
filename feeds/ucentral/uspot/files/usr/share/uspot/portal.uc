// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2022-2023 John Crispin <john@phrozen.org>
// SPDX-FileCopyrightText: 2023 Thibaut Varène <hacks@slashdirt.org>

'use strict';

let ubus = require('ubus');
let fs = require('fs');
let uci = require('uci').cursor();
let config = uci.get_all('uspot');
let nl = require("nl80211");
let lib = require('uspotlib');

let file = fs.open('/usr/share/uspot/header', 'r');
let header = file.read('all');
file.close();

file = fs.open('/usr/share/uspot/footer', 'r');
let footer = file.read('all');
file.close();

let header_custom;
let footer_custom;

file = fs.open('/tmp/ucentral/www-uspot/header.html', 'r');
if (file) {
	header_custom = file.read('all');
	file.close();
}

file = fs.open('/tmp/ucentral/www-uspot/footer.html', 'r');
if (file) {
	footer_custom = file.read('all');
	file.close();
}

let devices = {};
uci.foreach('uspot', 'uspot', (d) => {
	function adddev(ifname, sname) {
		if (ifname in devices)
			warn('uspot: ignoring duplicate entry for ifname: "' + ifname + '"\n');
		else
			devices[ifname] = sname;
	}

	if (d[".anonymous"]) {
		warn('uspot: ignoring invalid anonymous section at index ' + d[".index"] + '\n');
		return;
	}

	let spotname = d[".name"];
	if (!d.ifname) {
		warn('uspot: missing ifname in section "' + spotname + '"\n');
		return;
	}

	if (type(d.ifname) == "array") {
		for (let n in d.ifname)
			adddev(n, spotname);
	}
	else
		adddev(d.ifname, spotname);
});

function lookup_station(mac) {
	let wifs = nl.request(nl.const.NL80211_CMD_GET_INTERFACE, nl.const.NLM_F_DUMP);
	for (let wif in wifs) {
		if (!(wif.ifname in devices))
			continue;
		let res = nl.request(nl.const.NL80211_CMD_GET_STATION, nl.const.NLM_F_DUMP, { dev: wif.ifname });
		for (let sta in res) {
			if (sta.mac != lc(mac))
				continue;
			return devices[wif.ifname]
		}
	}
}

function spotfilter_device(spotfilter, mac)
{
	let uconn = ubus.connect();
	let spot = uconn.call('spotfilter', 'client_get', {
		interface: spotfilter,
		address: mac,
	});
	return (spot?.device);
}

function PO(id, english) {
	return english;
}

return {
	fs,
	rtnl: require('rtnl'),
	uam: require('uam'),
	uci,
	config,
	header,
	footer,

	// syslog helper
	syslog: function(ctx, msg) {
		warn('uspot: ' + ctx.env.REMOTE_ADDR + ' - ' + msg + '\n');
	},

	debug: function(ctx, msg) {
		if (+config.def_captive?.debug)
			this.syslog(ctx, msg);
	},

	// give a client access to the internet
	allow_client: function(ctx) {
		if (ctx.query_string.userurl)
			include('redir.uc', { redir_location: ctx.query_string.userurl });
		else
			include('allow.uc', ctx);

		// start accounting
		ctx.ubus.call('uspot', 'client_enable', {
			uspot: ctx.spotfilter,
			address: ctx.mac,
		});
	},

	// put a client back into pre-auth state
	logoff: function(ctx, uam) {
		this.syslog(ctx, 'logging client off');
		if (uam)
			include('redir.uc', { redir_location: this.uam_url(ctx, 'logoff') });
		else
			include('logoff.uc', ctx);

		ctx.ubus.call('uspot', 'client_remove', {
			uspot: ctx.spotfilter,
			address: ctx.mac,
		});
	},

	// request authentication from uspot backend, return reply 'access-accept': 0 or 1
	uspot_auth: function(ctx, username, password, challenge, extra) {
		let payload = {
			uspot: ctx.spotfilter,
			address: ctx.mac,
			client_ip: ctx.env.REMOTE_ADDR,
			ssid: ctx.ssid,
			sessionid: ctx.sessionid,
			reqdata: { ... extra || {} },
		};
		if (username)
			payload.username = username;
		if (password)
			payload.password = password;
		if (challenge)
			payload.challenge = challenge;

		return ctx.ubus.call('uspot', 'client_auth', payload);
	},


	uam_url: function(ctx, res) {
		let uam_url = ctx.config.uam_server +
			'?res=' + res +
			'&uamip=' + ctx.env.SERVER_ADDR +
			'&uamport=' + ctx.config.uam_port +
			'&challenge=' + this.uam.md5(ctx.config.challenge, ctx.format_mac) +
			'&mac=' + ctx.format_mac +
			'&ip=' + ctx.env.REMOTE_ADDR +
			'&called=' + ctx.config.nasmac +
			'&nasid=' + ctx.config.nasid +
			'&ssid=' + ctx.ssid +
			'&sessionid=' + ctx.sessionid;
		if (ctx.query_string?.redir)
			uam_url += '&userurl=' + ctx.query_string.redir;
		if (ctx.config.uam_secret)
			uam_url += '&md=' + this.uam.md5(uam_url, ctx.config.uam_secret);
		return uam_url;
	},

	handle_request: function(env, uam) {
		let mac;
		let form_data = {};
		let query_string = {};
		let post_data = '';
		let ctx = { env, header: this.header, footer: this.footer, mac, form_data, post_data, query_string, config: this.config, PO };
		let dev;

		// lookup the peers MAC
		let macs = this.rtnl.request(this.rtnl.const.RTM_GETNEIGH, this.rtnl.const.NLM_F_DUMP, { });
		for (let m in macs) {
			if (m.dst == env.REMOTE_HOST && m.lladdr) {
				ctx.mac = m.lladdr;
				dev = m.dev;
			}
		}

		// if the MAC lookup failed, go to the error page
		if (!ctx.mac) {
			this.syslog(ctx, 'failed to look up mac');
			include('error.uc', ctx);
			return NULL;
		}
		ctx.spotfilter = lookup_station(ctx.mac) || devices[dev];	// fallback to rtnl device
		ctx.config = config[ctx.spotfilter] || {};
		ctx.format_mac = lib.format_mac(ctx.config.mac_format, ctx.mac);
		if (+ctx.config.web_root) {
			ctx.header = header_custom;
			ctx.footer = footer_custom;
		}

		// check if a client is already connected
		ctx.ubus = ubus.connect();
		let cdata;
		cdata = ctx.ubus.call('uspot', 'client_get', {
			uspot: ctx.spotfilter,
			address: ctx.mac,
		});

		// stop if backend doesn't reply
		if (!cdata) {
			this.syslog(ctx, 'uspot error');
			include('error.uc', ctx);
			return NULL;
		}

		if (!uam && length(cdata)) {	// cdata is empty for disconnected clients
			include('connected.uc', ctx);
			return;
		}
		if (!cdata.ssid) {
			let device = spotfilter_device(ctx.spotfilter, ctx.mac);
			let hapd = ctx.ubus.call('hostapd.' + device, 'get_status');
			cdata.ssid = hapd?.ssid || 'unknown';
		}
		if (!cdata.sessionid) {
			let sessionid = lib.generate_sessionid();
			cdata.sessionid = sessionid;
		}
		ctx.ssid = cdata.ssid;
		ctx.sessionid = cdata.sessionid;

		// split QUERY_STRING
		if (env.QUERY_STRING)
			for (let chunk in split(env.QUERY_STRING, '&')) {
				let m = match(chunk, /^([^=]+)=(.*)$/);
				if (!m) continue;
				ctx.query_string[m[1]] = replace(m[2], /%([[:xdigit:]][[:xdigit:]])/g, (m, h) => chr(hex(h) || 0x20));
			}

		// recv POST data
		if (env.CONTENT_LENGTH > 0)
			for (let chunk = uhttpd.recv(64); chunk != null; chunk = uhttpd.recv(64))
				post_data += replace(chunk, /[^[:graph:]]/g, '.');

		// split POST data into an array
		if (post_data)
			for (let chunk in split(post_data, '&')) {
				let var = split(chunk, '=');
				if (length(var) != 2)
					continue;
				ctx.form_data[var[0]] = var[1];
			}
		return ctx;
	}
};
