{%

'use strict';

push(REQUIRE_SEARCH_PATH, "/usr/share/uspot/*.uc");

let portal = require('common');
let uam = require('uam');

// log the client in via radius
function auth_client(ctx) {
	let password;
	let payload = portal.radius_init(ctx);

	if (ctx.query_string.username && ctx.query_string.response) {
		let challenge = uam.md5(portal.config.uam.challenge, ctx.mac);

		payload.type = 'uam-chap-auth';
		payload.username = ctx.query_string.username;
		payload.chap_password = ctx.query_string.response;
		if (portal.config.uam.secret)
			payload.chap_challenge = uam.chap_challenge(challenge, portal.config.uam.uam_secret);
		else
			payload.chap_challenge = challenge;
	} else if (ctx.query_string.username && ctx.query_string.password) {
		payload.type = 'uam-auth';
		payload.username = ctx.mac;
		payload.password = uam.password(uam.md5(portal.config.uam.challenge, ctx.mac), ctx.query_string.password, portal.config.uam.uam_secret);
	}

        let reply = portal.radius_call(ctx, payload);
	if (reply['access-accept']) {
                portal.allow_client(ctx);
                return;
        }
	include('error.uc', ctx);
}

global.handle_request = function(env) {
	let ctx = portal.handle_request(env);

	if (ctx)
		auth_client(ctx);
};

%}
