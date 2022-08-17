{%

'use strict';

global.handle_request = function(env) {
	include('cpd.uc', { env });
};
%}
