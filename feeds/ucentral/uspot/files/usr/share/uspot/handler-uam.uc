{%

'use strict';

push(REQUIRE_SEARCH_PATH, "/usr/share/uspot/*.uc");

let portal = require('common');
let uam = require('uam');

// log the client in via radius
function auth_client(ctx) {
	let password;
	let payload = portal.radius_init(ctx);

	payload.logoff_url = sprintf('http://%s:3990/', ctx.env.SERVER_ADDR);
	if (ctx.query_string.username && ctx.query_string.response) {
		let challenge = uam.md5(portal.config.uam.challenge, ctx.format_mac);

		payload.username = ctx.query_string.username;
		payload.chap_password = ctx.query_string.response;
		if (portal.config.uam.secret)
			payload.chap_challenge = uam.chap_challenge(challenge, portal.config.uam.uam_secret);
		else
			payload.chap_challenge = challenge;
	} else if (ctx.query_string.username && ctx.query_string.password) {
		payload.username = ctx.query_string.username;
		payload.password = uam.password(uam.md5(portal.config.uam.challenge, ctx.format_mac), ctx.query_string.password, portal.config.uam.uam_secret);
	} else
		include('error.uc', ctx);

        let radius = portal.radius_call(ctx, payload);
	if (radius['access-accept']) {
		if (portal.config.uam.final_redirect_url == 'uam')
			ctx.query_string.userurl = portal.uam_url(ctx, 'success');
		portal.allow_client(ctx, { radius: { reply: radius.reply, request: payload } } );

		payload = portal.radius_init(ctx, payload.acct_session);
		payload.acct = true;
		payload.username = ctx.query_string.username;
		payload.acct_type = 1;
		if (radius.reply.Class)
			payload.class = radius.reply.Class;
		portal.radius_call(ctx, payload);
		return;
	}

	if (portal.config.uam.final_redirect_url == 'uam')
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
