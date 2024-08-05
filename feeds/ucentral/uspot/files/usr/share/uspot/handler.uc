{%

'use strict';

push(REQUIRE_SEARCH_PATH, "/usr/share/uspot/*.uc");
let portal = require('portal');

// delegate an initial connection to the correct handler
function request_start(ctx) {
	portal.debug(ctx, 'start ' + (ctx.config.auth_mode || '') + ' flow');
	switch (ctx.config.auth_mode) {
	case 'click-to-continue':
		include('serve.uc', { location: '/click.html' });
		return;
	case 'credentials':
		include('serve.uc', { location: '/credentials.html' });
		return;
	case 'radius':
		include('serve.uc', { location: '/radius.html' });
		return;
	case 'uam':
		// try mac-auth first if enabled
		if (+ctx.config.mac_auth) {
			let payload = portal.radius_init(ctx);
			payload.username = ctx.format_mac + (ctx.config.mac_suffix || '');
			payload.password = ctx.config.mac_passwd || ctx.format_mac;
			payload.service_type = 10;	// Call-Check, see https://wiki.freeradius.org/guide/mac-auth#web-auth-safe-mac-auth
		        let radius = portal.radius_call(ctx, payload);
			if (radius['access-accept']) {
				if (ctx.config.final_redirect_url == 'uam')
					ctx.query_string.userurl = portal.uam_url(ctx, 'success');
				delete payload.server;	// don't publish radius secrets
				portal.allow_client(ctx, { radius: { reply: radius.reply, request: payload } } );
				return;
			}
		}
		ctx.redir_location = portal.uam_url(ctx, 'notyet');
		include('redir.uc', ctx);
		return;
	default:
		include('error.uc', ctx);
		return;
	}
}

// delegate a local click-to-continue authentication
function request_click(ctx) {
	// make sure this is the right auth_mode
	if (ctx.config.auth_mode != 'click-to-continue') {
		include('error.uc', ctx);
                return;
	}

	// check if a username and password was provided
	if (ctx.form_data.accept_terms != 'clicked') {
		portal.debug(ctx, 'user did not accept conditions');
		request_start({ ...ctx, error: 1 });
                return;
	}
	portal.allow_client(ctx);
}

// delegate a local username/password authentication
function request_credentials(ctx) {
	// make sure this is the right auth_mode
	if (ctx.config.auth_mode != 'credentials') {
		include('error.uc', ctx);
                return;
	}

	// check if a username and password was provided
	if (!ctx.form_data.username || !ctx.form_data.password) {
		portal.debug(ctx, 'missing credentials\n');
		request_start({ ...ctx, error: 1 });
                return;
	}

	// check if the credentials are valid
	for (let k in portal.config) {
		let cred = portal.config[k];

		if (cred['.type'] != 'credentials')
			continue;
		if (cred.interface != ctx.spotfilter)
			continue;
		if (ctx.form_data.username != cred.username ||
		    ctx.form_data.password != cred.password)
			continue;

		portal.allow_client(ctx, { username: ctx.form_data.username });
		return;
	}

	// auth failed
	portal.debug(ctx, 'invalid credentials\n');
	request_start({ ...ctx, error: 1 });
}

// delegate a radius username/password authentication
function request_radius(ctx) {
	// make sure this is the right auth_mode
	if (ctx.config.auth_mode != 'radius') {
		include('error.uc', ctx);
                return;
	}

	// check if a username and password was provided
	if (!ctx.form_data.username || !ctx.form_data.password) {
		portal.debug(ctx, 'missing credentials\n');
		request_start({ ...ctx, error: 1 });
                return;
	}

	// trigger the radius auth
	let payload = portal.radius_init(ctx);
	payload.username = ctx.form_data.username;
	payload.password = ctx.form_data.password;

        let radius = portal.radius_call(ctx, payload);
	if (radius['access-accept']) {
		delete payload.server;	// don't publish radius secrets
                portal.allow_client(ctx, { username: ctx.form_data.username, radius: { reply: radius.reply, request: payload } } );
                return;
        }

	// auth failed
	portal.debug(ctx, 'invalid credentials\n');
	request_start({ ...ctx, error: 1 });
}

global.handle_request = function(env) {
	let ctx = portal.handle_request(env);

	if (ctx)
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
