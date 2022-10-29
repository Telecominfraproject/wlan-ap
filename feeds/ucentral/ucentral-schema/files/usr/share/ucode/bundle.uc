push(REQUIRE_SEARCH_PATH, '/usr/share/ucentral/*.uc');

let ubus = require('ubus').connect();
let uci = require('uci').cursor();
let fs = require('fs');
let w_iface = require('wifi.iface');
let w_sta = require('wifi.station');

return {
	init: function(id) {
		this.id = id || 0;
		this.path = `/tmp/bundle.${this.id}/`;
		fs.mkdir(this.path);
	},

	complete: function() {
		if (!this.path)
			return;
		system(`tar cfz /tmp/bundle.${this.id}.tar.gz ${this.path}`);
		system(`rm -r ${this.path}`);
	},

	add: function(name, data) {
		if (!this.path)
			return;
		let file = fs.open(this.path + name, 'w');
		file.write(data);
		file.close();
	},

	ubus: function(object, method, args) {
		if (!object || !method)
			return;

		let data = ubus.call(object, method, args || {});
		this.add(`ubus-${object}:${method}`, data);
	},

	uci: function(config, section) {
		if (!config)
			return;

		let data = uci.get_all(config, section) || {};
		let name = `uci-${config}` + (section ? `.${section}` : '');
		this.add(name, data);
	},

	wifi: function() {
		this.add('wifi-iface', w_iface);
		this.add('wifi-station', w_sta);
	},

	shell: function(name, command) {
		if (!command)
			command = name;

		let fp = fs.popen(command);
	        let data = fp.read('all');
		fp.close();

		this.add(`shell-${name}`, data);
	},

};
