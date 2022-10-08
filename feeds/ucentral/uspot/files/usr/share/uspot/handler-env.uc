{%

'use strict';

global.handle_request = function(env) {
	include("dump-env.uc", { env });
};
