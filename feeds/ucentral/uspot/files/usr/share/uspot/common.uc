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

	// syslog helper
	syslog: function(ctx, msg) {
		warn('uspot: ' + ctx.env.REMOTE_ADDR + ' - ' + msg + '\n');
	},

	debug: function(ctx, msg) {
		if (config.config.debug)
			this.syslog(ctx, msg);
	},

	// mac re-formater
	format_mac: function(mac) {
		switch(config.uam.mac_format) {
		case 'aabbccddeeff':
		case 'AABBCCDDEEFF':
			mac = replace(mac, ':', '');
			break;
		case 'aa-bb-cc-dd-ee-ff':
		case 'AA-BB-CC-DD-EE-FF':
			mac = replace(mac, ':', '-');
			break;
		}

		switch(config.uam.mac_format) {
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
	},

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
	allow_client: function(ctx, data) {
		this.syslog(ctx, 'allow client to pass traffic');
		ctx.ubus.call('spotfilter', 'client_set', {
			"interface": "hotspot",
			"address": ctx.mac,
			"state": 1,
			"dns_state": 1,
			"accounting": [ "dl", "ul"],
			"data": {
				... data || {},
				"connect": time(),
			}
		});
		if (ctx.query_string.userurl)
			include('redir.uc', { redir_location: ctx.query_string.userurl });
		else
			include('allow.uc', ctx);
		//data.radius.reply['WISPr-Bandwidth-Max-Up'] = "20000000";
		//data.radius.reply['WISPr-Bandwidth-Max-Down'] = "10000000";
		if (data?.radius?.reply && (+data.radius.reply['WISPr-Bandwidth-Max-Up'] && +data.radius.reply['WISPr-Bandwidth-Max-Down']))
			ctx.ubus.call('ratelimit', 'client_set', {
				device: ctx.device,
				address: ctx.mac,
				rate_egress: sprintf('%s', data.radius.reply['WISPr-Bandwidth-Max-Down']),
				rate_ingress: sprintf('%s', data.radius.reply['WISPr-Bandwidth-Max-Up']),
			 });
	},

	// put a client back into pre-auth state
	logoff: function(ctx, uam) {
		this.syslog(ctx, 'logging client off');
		ctx.ubus.call('spotfilter', 'client_set', {
			interface: 'hotspot',
			address: ctx.mac,
			state: 0,
			dns_state: 1,
			accounting: [],
			data: {
				logoff : 1
			}
		});

		if (uam)
			include('redir.uc', { redir_location: this.uam_url(ctx, 'logoff') });
		else
			include('logoff.uc', ctx);
	},

	// generate the default radius auth payload
	radius_init: function(ctx, acct_session) {
		let math = require('math');
		if (!acct_session) {
			acct_session = '';

			for (let i = 0; i < 16; i++)
			        acct_session += sprintf('%d', math.rand() % 10);
		}

		return {
			server: sprintf('%s:%s:%s', this.config.radius.auth_server, this.config.radius.auth_port, this.config.radius.auth_secret),
			acct_server: sprintf('%s:%s:%s', this.config.radius.acct_server, this.config.radius.acct_port, this.config.radius.acct_secret),
			acct_session,
			client_ip: ctx.env.REMOTE_ADDR,
			called_station: this.config.uam.nasmac + ':' + ctx.ssid,
			calling_station: this.format_mac(ctx.mac),
			nas_ip: ctx.env.SERVER_ADDR,
			nas_id: this.config.uam.nasid
		};
	},

	radius_call: function(ctx, payload) {
		let type = payload.acct ? 'acct' : 'auth';
		let cfg = fs.open('/tmp/' + type + ctx.mac + '.json', 'w');
		cfg.write(payload);
		cfg.close();

		return this.fs_popen('/usr/bin/radius-client /tmp/' + type + ctx.mac + '.json');
	},

	uam_url: function(ctx, res) {
		let uam_url = this.config.uam.uam_server +
			'?res=' + res +
			'&uamip=' + ctx.env.SERVER_ADDR +
			'&uamport=' + this.config.uam.uam_port +
			'&challenge=' + this.uam.md5(this.config.uam.challenge, ctx.format_mac) +
			'&mac=' + ctx.format_mac +
			'&ip=' + ctx.env.REMOTE_ADDR +
			'&called=' + this.config.uam.nasmac +
			'&nasid=' + this.config.uam.nasid +
			'&ssid=' + ctx.ssid;
		if (ctx.query_string?.redir)
			uam_url += '&userurl=' + ctx.query_string.redir;
		if (this.config.uam.uam_secret)
			uam_url += '&md=' + this.uam.md5(uam_url, this.config.uam.uam_secret);
		return uam_url;
	},

	handle_request: function(env, uam) {
		let mac;
		let form_data = {};
		let query_string = {};
		let post_data = '';
		let ctx = { env, header: this.header, footer: this.footer, mac, form_data, post_data, query_string, config: this.config, PO };

		// lookup the peers MAC
		let macs = this.rtnl.request(this.rtnl.const.RTM_GETNEIGH, this.rtnl.const.NLM_F_DUMP, { });
		for (let m in macs)
			if (m.dst == env.REMOTE_HOST && m.lladdr)
				ctx.mac = m.lladdr;

		// if the MAC lookup failed, go to the error page
		if (!ctx.mac) {
			this.syslog(ctx, 'failed to look up mac');
			include('error.uc', ctx);
			return NULL;
		}
		ctx.format_mac = this.format_mac(ctx.mac);

		// check if a client is already connected
		ctx.ubus = ubus.connect();
		let connected = ctx.ubus.call('spotfilter', 'client_get', {
			interface: 'hotspot',
			address: ctx.mac,
		});
		if (!uam && connected?.state) {
			include('connected.uc', ctx);
			return NULL;
		}
		if (!connected.data.ssid) {
			let hapd = ctx.ubus.call('hostapd.' + connected.device, 'get_status');
			ctx.ubus.call('spotfilter', 'client_set', {
				interface: 'hotspot',
				address: ctx.mac,
				data: {
					ssid: hapd.ssid || 'unknown'
				}
			});
			connected.data.ssid = hapd.ssid;
		}
		ctx.device = connected.device;
		ctx.ssid = connected.data.ssid;

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
