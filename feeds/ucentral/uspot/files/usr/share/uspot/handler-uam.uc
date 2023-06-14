{%

'use strict';

push(REQUIRE_SEARCH_PATH, "/usr/share/uspot/*.uc");

let portal = require('portal');
let uam = require('uam');

// log the client in via radius
function auth_client(ctx) {
	let password;
	let payload = portal.radius_init(ctx);

	payload.logoff_url = sprintf('http://%s:%s/logoff', ctx.env.SERVER_ADDR, ctx.config.uam_port);
	if (ctx.query_string.username) {	// username must be set
		payload.username = ctx.query_string.username;
		if (ctx.query_string.response) {	// try challenge first
			let challenge = uam.md5(ctx.config.challenge, ctx.format_mac);
			payload.chap_password = ctx.query_string.response;
			payload.chap_challenge = ctx.config.uam_secret ? uam.chap_challenge(challenge, ctx.config.uam_secret) : challenge;
		} else if ("password" in ctx.query_string) {	// allow empty password
			payload.password = !ctx.config.uam_secret ? ctx.query_string.password :
				uam.password(uam.md5(ctx.config.challenge, ctx.format_mac), ctx.query_string.password, ctx.config.uam_secret);
		}
	} else {
		include('error.uc', ctx);
		return;
	}

        let radius = portal.radius_call(ctx, payload);
	if (radius['access-accept']) {
		if (ctx.config.final_redirect_url == 'uam')
			ctx.query_string.userurl = portal.uam_url(ctx, 'success');

		delete payload.server;	// don't publish server secrets
		portal.allow_client(ctx, { radius: { reply: radius.reply, request: payload } } );
		return;
	}

	if (ctx.config.final_redirect_url == 'uam')
		include('redir.uc', { redir_location: portal.uam_url(ctx, 'reject') });
	else
		include('error.uc', ctx);
}

// disconnect client
function deauth_client(ctx) {
	portal.logoff(ctx, true);
}

global.handle_request = function(env) {
	let ctx = portal.handle_request(env, true);

	switch (split(ctx.env.REQUEST_URI, '?')[0] || '') {
	case '/logon':
		auth_client(ctx);
		break;
	case '/logout':
	case '/logoff':
		deauth_client(ctx);
		break;
	default:
		include('error.uc', ctx);
		break;
	}
};

%}
