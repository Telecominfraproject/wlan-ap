let uci = require('uci').cursor();
let config = uci.get_all('uspot');
let fs = require('fs');

let stdout = fs.popen('env');
let raw = stdout.read('all');
let lines = split(raw, '\n');
let env = {};
for (let line in lines) {
        let e = split(line, '=');
        if (length(e) == 2)
                env[e[0]] = e[1];
}

let location;

switch(config.config.auth_mode) {
case 'uam':
	location = config.uam.server +
		   '?res=notyet' +
		   '&uamip=' + env.SERVER_ADDR +
		   '&uamport=' + config.uam.port +
		   //'&challenge=' +
		   //'&mac=' +
		   '&called=' + config.uam.nasmac +
		   '&nasid=' + config.uam.nasid +
//		   '&sessionid=' + +
//		   userurl
//		   md=
		"";
	break;
default:
        location = 'http://' + env.SERVER_ADDR + '/hotspot';
}

system('logger "' + location + '"');

printf('Status: 302 Found
Location: %s                  
Content-Type: text/html

', location);
