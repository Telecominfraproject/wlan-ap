{%
// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2022-2023 John Crispin <john@phrozen.org>
// SPDX-FileCopyrightText: 2023 Thibaut Varène <hacks@slashdirt.org>

'use strict';

push(REQUIRE_SEARCH_PATH, "/usr/share/uspot/*.uc");

let portal = require('portal');
let uam = require('uam');

// log the client in via radius
function auth_client(ctx) {
	let username;
	let password;
	let challenge;
	let payload = {};

	payload['WISPr-Logoff-URL'] = sprintf('http://%s:%s/logoff', ctx.env.SERVER_ADDR, (ctx.config.uam_port || "3990"));
	if (ctx.query_string.username) {	// username must be set
		username = ctx.query_string.username;
		if (ctx.query_string.response) {	// try challenge first
			challenge = uam.md5(ctx.config.challenge, ctx.format_mac);
			password = ctx.query_string.response;
			challenge = ctx.config.uam_secret ? uam.chap_challenge(challenge, ctx.config.uam_secret) : challenge;
		} else if ("password" in ctx.query_string) {	// allow empty password
			password = !ctx.config.uam_secret ? ctx.query_string.password :
				uam.password(uam.md5(ctx.config.challenge, ctx.format_mac), ctx.query_string.password, ctx.config.uam_secret);
		}
	} else {
		include('error.uc', ctx);
		return;
	}

        let auth = portal.uspot_auth(ctx, username, password, challenge, payload);
	if (auth && auth['access-accept']) {
		if (ctx.config.final_redirect_url == 'uam')
			ctx.query_string.userurl = portal.uam_url(ctx, 'success');

		portal.allow_client(ctx);
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
