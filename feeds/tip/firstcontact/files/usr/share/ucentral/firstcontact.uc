let config = {};

function store_config() {
	let redir = split(config.Redirector, ":");
	let gw = {
		server: redir[0],
		port: redir[1] || 15002
	};
	fs.writefile('/etc/ucentral/gateway.json', gw);
}

function store_config_uci(path) {
        let cursor = uci.cursor(path);
        let redir = split(config.Redirector, ":");

        cursor.load("ucentral");
        cursor.set("ucentral", "config", "server", redir[0]);
        cursor.set("ucentral", "config", "port", redir[1] || 15002);
        cursor.commit();
}

function digicert() {
	let devid;
	let fd = fs.open("/etc/ucentral/dev-id", "r");
	if (!fd) {
		warn("firstcontact: failed to find device id");
		exit(1);
	}
	devid = fd.read("all");
	fd.close();

	ret = system(sprintf('/usr/sbin/digicert -i %s', devid));
	if (ret) {
		warn("firstcontact failed to contact redirector, check DHCP option\n");
		let fd = fs.open("/tmp/capwap/dhcp_opt.txt", "r");
		if (!fd) {
			warn("No redirector found\n");
			exit(1);
		} else {
			config.Redirector = fd.read("all");
			fd.close();
		}
	} else {
		let redirector = { };
		let fd = fs.open("/etc/ucentral/redirector.json", "r");
		if (fd) {
			let data = fd.read("all");
			fd.close();

			try {
				redirector = json(data);
			}
			catch (e) {
				warn("firstcontact: Unable to parse JSON data in %s: %s", path, e);

				exit(1);
			}
		}

		for (let r in redirector.fields)
			if (r.name && r.value)
				config[r.name] = r.value;
		if (!config.Redirector) {
			warn("Reply is missing Redirector field\n");
			exit(1);
		}

	}
}

if (!fs.stat('/etc/ucentral/gateway.json')) {
	digicert();
	store_config();
	store_config_uci();
	store_config_uci("/etc/config-shadow/");
	warn("firstcontact: managed to look up redirector\n");
}

system("/etc/init.d/ucentral enable");
system("/etc/init.d/firstcontact disable");
system("reload_config");
system("/etc/init.d/ucentral start");
system("/etc/init.d/firstcontact stop");
