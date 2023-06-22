// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2022-2023 John Crispin <john@phrozen.org>
// SPDX-FileCopyrightText: 2023 Thibaut Varène <hacks@slashdirt.org>

'use strict';

return {
	// mac re-formater
	format_mac: function(format, mac) {
		switch(format) {
			case 'aabbccddeeff':
			case 'AABBCCDDEEFF':
				mac = replace(mac, ':', '');
				break;
			case 'aa-bb-cc-dd-ee-ff':
			case 'AA-BB-CC-DD-EE-FF':
				mac = replace(mac, ':', '-');
				break;
		}
		
		switch(format) {
			case 'aabbccddeeff':
			case 'aa-bb-cc-dd-ee-ff':
			case 'aa:bb:cc:dd:ee:ff':
				mac = lc(mac);
				break;
			case 'AABBCCDDEEFF':
			case 'AA:BB:CC:DD:EE:FF':
			case 'AA-BB-CC-DD-EE-FF':
				mac = uc(mac);
				break;
		}
		
		return mac;
	},

	// session id generator - 16-char hex string
	generate_sessionid: function() {
		let math = require('math');
		let sessionid = '';

		for (let i = 0; i < 16; i++)
			sessionid += sprintf('%x', math.rand() % 16);

		return sessionid;
	}
};
