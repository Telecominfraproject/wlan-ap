'use strict';

let ubus = require('ubus');
let fs = require('fs');
let uci = require('uci').cursor();
let config = uci.get_all('uspot');

let file = fs.open(config.config.web_root == 1 ? '/tmp/ucentral/www-uspot/header.html' : '/usr/share/uspot/header', 'r');
let header = file.read('all');
file.close();

file = fs.open(config.config.web_root == 1 ? '/tmp/ucentral/www-uspot/footer.html' : '/usr/share/uspot/footer', 'r');
let footer = file.read('all');
file.close();

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

	// wrapper for scraping external tools stdout
	fs_popen: function(cmd) {
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
	},

	// give a client access to the internet
	allow_client: function(ctx) {
		ctx.ubus.call('spotfilter', 'client_set', {
			"interface": "hotspot",
			"address": replace(ctx.mac, '-', ':'),
			"state": 1,
			"dns_state": 1,
			"accounting": [ "dl", "ul"],
			"data": {
				"connect": time()
			}
		});
		if (ctx.query_string.userurl)
			include('redir.uc', { redir_location: ctx.query_string.userurl });
		else
			include('allow.uc', ctx);
	},

	// generate the default radius auth payload
	radius_init: function(ctx) {
		return {
			server: sprintf('%s:%s:%s', this.config.radius.auth_server, this.config.radius.auth_port, this.config.radius.auth_secret),
			acct_session: "0123456789",
			client_ip: ctx.env.REMOTE_ADDR,
			called_station: ctx.mac,
			calling_station: this.config.uam.nasmac,
			nas_ip: ctx.env.SERVER_ADDR,
			nas_id: this.config.uam.nasid
		};
	},

	radius_call: function(ctx, payload) {
		let cfg = fs.open('/tmp/' + ctx.mac + '.json', 'w');
		cfg.write(payload);
		cfg.close();

		return this.fs_popen('/usr/bin/radius-client /tmp/' + ctx.mac + '.json');
	},

	handle_request: function(env) {
		let mac;
		let form_data = {};
		let query_string = {};
		let post_data = '';
		let ctx = { env, header: this.header, footer: this.footer, mac, form_data, post_data, query_string, config: this.config, PO };

		// lookup the peers MAC
		let macs = this.rtnl.request(this.rtnl.const.RTM_GETNEIGH, this.rtnl.const.NLM_F_DUMP, { });
		for (let m in macs)
			if (m.dst == env.REMOTE_HOST)
				ctx.mac = replace(m.lladdr, ':', '-');

		// if the MAC lookup failed, go to the error page
		if (!ctx.mac) {
			include('error.uc', ctx);
			return NULL;
		}

		// check if a client is already connected
		ctx.ubus = ubus.connect();
		let connected = ctx.ubus.call('spotfilter', 'client_get', {
			'interface': 'hotspot',
			'address': ctx.mac
		});
		if (connected?.state) {
			include('connected.uc', ctx);
			return NULL;
		}

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
