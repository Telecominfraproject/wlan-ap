{%

'use strict';

let fs = require('fs');
let rtnl = require('rtnl');

let file = fs.open('/usr/share/uspot/header', 'r');
let header = file.read('all');
file.close();

file = fs.open('/usr/share/uspot/footer', 'r');
let footer = file.read('all');
file.close();

let uci = require('uci').cursor();
let config = uci.get_all('uspot');

let uam = require('uam');

// give a client access to the internet
function allow_client(ctx) {
	system('ubus call spotfilter client_set \'{ "interface": "hotspot", "address": "' + replace(ctx.mac, '-', ':') + '", "state": 1, "dns_state": 1}\'');
	if (ctx.query_string.userurl)
		include('uam.uc', { uam_location: ctx.query_string.userurl });
	else
		include('allow.uc', ctx);
}

// log the client in via radius
function auth_client(ctx) {
	let password;
	let payload = {
		server: sprintf('%s:%s:%s',config.radius.auth_server, config.radius.auth_port, config.radius.auth_secret),
		acct_session: "0123456789",
		client_ip: ctx.env.REMOTE_ADDR,
		called_station: ctx.mac,
		calling_station: config.uam.nasmac,
		nas_ip: ctx.env.SERVER_ADDR,
		nas_id: config.uam.nasid
	};

	if (ctx.query_string.username && ctx.query_string.response) {
		let challenge = uam.md5(config.uam.challenge, ctx.mac);

		payload.type = 'uam-chap-auth';
		payload.username = ctx.query_string.username;
		payload.chap_password = ctx.query_string.response;
		if (config.uam.secret)
			payload.chap_challenge = uam.chap_challenge(challenge, config.uam.uam_secret);
		else
			payload.chap_challenge = challenge;
	} else if (ctx.query_string.username && ctx.query_string.password) {
		payload.type = 'uam-auth';
		payload.username = ctx.mac;
		payload.password = uam.password(uam.md5(config.uam.challenge, ctx.mac), ctx.query_string.password, config.uam.uam_secret);
	}

	let cfg = fs.open('/tmp/' + ctx.mac + '.json', 'w');
	cfg.write(payload);
	cfg.close();

	let stdout = fs.popen('/usr/bin/radius-client /tmp/' + ctx.mac + '.json');
        let reply;
        if (!stdout) {
		request_start({ ...ctx, error: 1 });
		return;
        }
	reply = json(stdout.read('all'));
	stdout.close();
	if (reply['access-accept']) {
                allow_client(ctx);
                return;
        }
	include('error.uc', ctx);
}

global.handle_request = function(env) {
	let mac;
	let form_data = {};
	let query_string = {};
	let post_data = '';
	let ctx = { env, header, footer, mac, form_data, post_data, query_string, config };

	// lookup the peers MAC
	let macs = rtnl.request(rtnl.const.RTM_GETNEIGH, rtnl.const.NLM_F_DUMP, { });
	for (let m in macs)
		if (m.dst == env.REMOTE_HOST)
			ctx.mac = replace(m.lladdr, ':', '-');

	// if the MAC lookup failed, go to the error page
	if (!ctx.mac)
		include('error.uc', ctx);

	// split QUERY_STRING
	if (env.QUERY_STRING)
		for (let chunk in split(env.QUERY_STRING, '&')) {
			let m = match(chunk, /^([^=]+)=(.*)$/);
			if (!m) continue;
			ctx.query_string[m[1]] = replace(m[2], /%([[:xdigit:]][[:xdigit:]])/g, (m, h) => chr(hex(h) || 0x20));
		}
	auth_client(ctx);
};

%}
