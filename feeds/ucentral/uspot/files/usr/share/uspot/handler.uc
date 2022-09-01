{%

'use strict';

let fs = require('fs');
let rtnl = require('rtnl');
let uam = require('uam');

let uci = require('uci').cursor();
let config = uci.get_all('uspot');

let file = fs.open(config.config.web_root == 1 ? '/tmp/ucentral/www-uspot/header.html' : '/usr/share/uspot/header', 'r');
let header = file.read('all');
file.close();

file = fs.open(config.config.web_root == 1 ? '/tmp/ucentral/www-uspot/footer.html' : '/usr/share/uspot/footer', 'r');
let footer = file.read('all');
file.close();

// fs.open wrapper
function fs_open(cmd) {
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

// give a client access to the internet
function allow_client(ctx) {
	system('ubus call spotfilter client_set \'{ "interface": "hotspot", "address": "' + replace(ctx.mac, '-', ':') + '", "state": 1, "dns_state": 1}\'');
	include('allow.uc', ctx);
}

// delegate an initial connection to the correct handler
function request_start(ctx) {
	switch (config?.config?.auth_mode) {
	case 'click-to-continue':
		include('click.uc', ctx);
		return;
	case 'credentials':
		include('credentials.uc', ctx);
		return;
	case 'radius':
		include('radius.uc', ctx);
		return;
	case 'uam':
		ctx.uam_location = config.uam.uam_server +
			'?res=notyet' +
			'&uamip=' + ctx.env.SERVER_ADDR +
			'&uamport=' + config.uam.uam_port +
			'&challenge=' + uam.md5(config.uam.challenge, ctx.mac) +
			'&mac=' + replace(ctx.mac, ':', '-') +
			'&ip=' + ctx.env.REMOTE_ADDR +
			'&called=' + config.uam.nasmac +
			'&nasid=' + config.uam.nasid;
		ctx.uam_location += '&md=' + uam.md5(ctx.uam_location, config.uam.uam_secret);
		include('uam.uc', ctx);
		return;
	default:
		include('error.uc', ctx);
		return;
	}
}

// delegate a local click-to-continue authentication
function request_click(ctx) {
	// make sure this is the right auth_mode
	if (config?.config?.auth_mode != 'click-to-continue') {
		include('error.uc', ctx);
                return;
	}

	// check if a username and password was provided
	if (ctx.form_data.accept_terms != 'clicked') {
		request_start({ ...ctx, error: 1 });
                return;
	}
	allow_client(ctx);
}

// delegate a local username/password authentication
function request_credentials(ctx) {
	// make sure this is the right auth_mode
	if (config?.config?.auth_mode != 'credentials') {
		include('error.uc', ctx);
                return;
	}

	// check if a username and password was provided
	if (!ctx.form_data.username || !ctx.form_data.password) {
		request_start({ ...ctx, error: 1 });
                return;
	}

	// check if the credentials are valid
	for (let k in config) {
		let cred = config[k];

		if (cred['.type'] != 'credentials')
			continue;
		if (ctx.form_data.username != cred.username ||
		    ctx.form_data.password != cred.password)
			continue;

		allow_client(ctx);
		return;
	}

	// auth failed
	request_start({ ...ctx, error: 1 });
}

// delegate a radius username/password authentication
function request_radius(ctx) {
	// make sure this is the right auth_mode
	if (config?.config?.auth_mode != 'radius') {
		include('error.uc', ctx);
                return;
	}

	// check if a username and password was provided
	if (!ctx.form_data.username || !ctx.form_data.password) {
		request_start({ ...ctx, error: 1 });
                return;
	}

	let cfg = fs.open('/tmp/' + ctx.mac + '.json', 'w');
	cfg.write({
		type: 'auth',
		server: sprintf('%s:%s:%s',config.radius.auth_server, config.radius.auth_port, config.radius.auth_secret),
		username: ctx.form_data.username,
		password: ctx.form_data.password,
		acct_session: "0123456789",
		client_ip: ctx.env.REMOTE_ADDR,
		called_station: ctx.mac,
		nas_ip: ctx.env.SERVER_ADDR,
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

	// auth failed
	request_start({ ...ctx, error: 1 });
}

function PO(id, english) {
	return english;
}

global.handle_request = function(env) {
	let mac;
	let form_data = {};
	let query_string = {};
	let post_data = '';
	let ctx = { env, header, footer, mac, form_data, post_data, query_string, config, PO };

	// lookup the peers MAC
	let macs = rtnl.request(rtnl.const.RTM_GETNEIGH, rtnl.const.NLM_F_DUMP, { });
	for (let m in macs)
		if (m.dst == env.REMOTE_HOST)
			ctx.mac = replace(m.lladdr, ':', '-');

	// if the MAC lookup failed, go to the error page
	if (!ctx.mac)
		include('error.uc', ctx);

	// check if a client is already connected
	let connected = fs_open('ubus call spotfilter client_get \'{"interface": "hotspot", "address": "' + ctx.mac + '"}\'');
	if (connected?.state) {
		include('connected.uc', ctx);
		return;
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

	switch (ctx.form_data.action) {
	case 'credentials':
		request_credentials(ctx);
		return;
	case 'radius':
		request_radius(ctx);
		return;
	case 'click':
		request_click(ctx);
		return;
	default:
		request_start(ctx);
		return;
	}
};

%}
