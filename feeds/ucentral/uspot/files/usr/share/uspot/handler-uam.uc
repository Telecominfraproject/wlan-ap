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
	include('allow.uc', ctx);
}

// log the client in via radius
function auth_client(ctx) {
	if (!ctx.query_string.username || !ctx.query_string.password) {
		include('allow.uc', ctx);
		return false;
	}
	let password = uam.password(uam.md5(config.uam.challenge, ctx.mac), ctx.query_string.password, config.uam.uam_secret);

	let cfg = fs.open('/tmp/' + ctx.mac + '.json', 'w');
	cfg.write({
		type: 'uam-auth',
		server: sprintf('%s:%s:%s',config.radius.auth_server, config.radius.auth_port, config.radius.auth_secret),
		username: ctx.mac,
		password,
		acct_session: "0123456789",
		client_ip: ctx.env.REMOTE_ADDR,
		called_station: ctx.mac,
		calling_station: config.uam.nasmac,
		nas_ip: ctx.env.SERVER_ADDR,
		nas_id: config.uam.nasid
	});
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
			let var = split(chunk, '=');
			if (length(var) != 2)
				continue;
			ctx.query_string[var[0]] = var[1];
		}
	auth_client(ctx);
};

%}
