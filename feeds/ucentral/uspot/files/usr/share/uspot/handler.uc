{%
// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2022-2023 John Crispin <john@phrozen.org>
// SPDX-FileCopyrightText: 2023 Thibaut Varène <hacks@slashdirt.org>

'use strict';

push(REQUIRE_SEARCH_PATH, "/usr/share/uspot/*.uc");
let portal = require('portal');

// delegate an initial connection to the correct handler
function request_start(ctx) {
	portal.debug(ctx, 'start ' + (ctx.config.auth_mode || '') + ' flow');
	switch (ctx.config.auth_mode) {
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
		// try mac-auth first if enabled
		if (+ctx.config.mac_auth) {
		        let auth = portal.uspot_auth(ctx);
			if (auth && auth['access-accept']) {
				if (ctx.config.final_redirect_url == 'uam')
					ctx.query_string.userurl = portal.uam_url(ctx, 'success');
				portal.allow_client(ctx);
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
	portal.uspot_auth(ctx);
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
	let auth = portal.uspot_auth(ctx, ctx.form_data.username, ctx.form_data.password);
	if (auth && auth['access-accept']) {
		portal.allow_client(ctx);
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
	let username = ctx.form_data.username;
	let password = ctx.form_data.password;

        let auth = portal.uspot_auth(ctx, username, password);
	if (auth && auth['access-accept']) {
                portal.allow_client(ctx);
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
