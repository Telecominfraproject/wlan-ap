#!/usr/bin/ucode

import * as libubus from 'ubus';
import * as fs from 'fs';

let cmd = ARGV[0];
let ifname = getenv("interface");
let opt224 = getenv("opt224");

if (cmd != 'bound' && cmd != 'renew')
	exit(0);

/*let file = fs.readfile('/etc/ucentral/gateway.json');
if (file)
	file = json(file);
file ??= {};
if (file.server && file.port && file.valid)
	exit(0);
*/

let cloud = {
	lease: true,
};
if (opt224) {
	let dhcp = hexdec(opt224);
	dhcp = split(dhcp, ':');
	cloud.dhcp_server = dhcp[0];
	cloud.dhcp_port = dhcp[1] ?? 15002;
}
fs.writefile('/tmp/cloud.json', cloud);

if (opt224 && cmd == 'renew') {
	let ubus = libubus.connect();
	ubus.call('cloud', 'renew');
}

exit(0);
