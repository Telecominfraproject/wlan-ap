#!/usr/bin/ucode

let ubus = require('ubus').connect();
let uci = require('uci').cursor();
let uloop = require('uloop');
let rtnl = require('rtnl');
let fs = require('fs');
let log = require("log");
let hapd_subscriber;
let device_subscriber;
let ifaces = {};

/* load UCI */
let config;
function config_load() {
	config = uci.get_all('ieee8021x', '@config[0]');
	config.ports = {};
	for (let k, port in uci.get_all('ieee8021x')) {
		if (port['.type'] != 'port')
			continue;
		config.ports[k] = port;
	}
}

/* set a wired ports auth state */
function netifd_handle_iface(name, auth_status, vlan) {
	let msg = { name, auth_status, auth_vlans: [ ifaces[name].default_vlan + ':u' ]};
	if (ifaces[name].upstream && vlan)
		msg.auth_vlans = [ vlan + ':u' ];
	ubus.call('network.device', 'set_state', msg);
	if (auth_status && ifaces[name].upstream && vlan) {
		for (let wan in ifaces[name].wan_ports) {
			let msg = {
				name: wan,
				vlan: [ `${vlan}:t` ]
			};
			ubus.call('network.interface.up_none', 'add_device', msg);
			ubus.call('udevstats', 'add_device', { device: wan, vlan });
		}
	}
	ifaces[name].authenticated = auth_status;
}

/* handle events from hostapd */
function hapd_subscriber_notify_cb(notify) {
	switch(notify.type) {
	case 'sta-authorized':
		log.syslog(LOG_USER, "authenticated station");
		push(ifaces[notify.data.ifname].lladdr, notify.data.address);
		netifd_handle_iface(notify.data.ifname, true, notify.data.vlan);
		break;
	};

	return 0;
}

/* remove arp and rate limit for a client */
function flush_iface(name) {
	/* flush all arp entries */
	let neighs = rtnl.request(rtnl.const.RTM_GETNEIGH, rtnl.const.NLM_F_DUMP, { dev: name });
	for (let neigh in neighs)
		if (neigh.lladdr in ifaces[name].lladdr) {
			rtnl.request(rtnl.const.RTM_DELNEIGH, 0, { dst: neigh.dst, dev: neigh.dev, family: neigh.family });
			ubus.call('ratelimit', 'client_delete', { device: neigh.dev, address: neigh.lladdr });
		}
	ifaces[name].lladdr = [];
}

/* generate a hostapd configuration */
function hostapd_start(iface) {
	if (!fs.stat("/sys/class/net/" + iface)) {
		log.syslog(LOG_ERR, "Interface ${iface} does not exist yet");
		return;
	}
	if (!config.auth_server_addr) {
		log.syslog(LOG_ERR, "Auth server address is empty");
		return;
	}

	let path = '/var/run/hostapd-' + iface + '.conf';
	let file = fs.open(path, 'w+');

	file.write('driver=wired\n');
	file.write('ieee8021x=1\n');
	file.write('eap_reauth_period=0\n');
	file.write('ctrl_interface=/var/run/hostapd\n');
	file.write('interface=' + iface + '\n');
	file.write('ca_cert=' + config.ca + '\n');
	file.write('server_cert=' + config.cert + '\n');
	file.write('private_key=' + config.key + '\n');
	file.write('dynamic_vlan=1\n');
	file.write('vlan_no_bridge=1\n');
	file.write('vlan_naming=1\n');

	if (config.auth_server_addr) {
		file.write('dynamic_own_ip_addr=1\n');
		file.write('auth_server_addr=' + config.auth_server_addr + '\n');
		file.write('auth_server_port=' + config.auth_server_port + '\n');
		file.write('auth_server_shared_secret=' + config.auth_server_secret + '\n');
		if (config.acct_server_addr && config.acct_server_port && config.acct_server_secret + '\n') {
			file.write('acct_server_addr=' + config.acct_server_addr + '\n');
			file.write('acct_server_port=' + config.acct_server_port + '\n');
			file.write('acct_server_shared_secret=' + config.acct_server_secret + '\n');
		}
		if (config.nas_identifier)
			file.write('nas_identifier=${config.nas_identifier}\n');
		if (config.coa_server_addr && config.coa_server_port && config.coa_server_secret) {
			file.write('radius_das_client=' + config.coa_server_addr + ' ' + config.coa_server_secret + '\n');
			file.write('radius_das_port=' + config.coa_server_port + '\n');
		}
		if (+config.mac_address_bypass)
			file.write('macaddr_acl=2\n');
	} else {
		file.write('eap_server=1\n');
		file.write('eap_user_file=/var/run/hostapd-ieee8021x.eap_user\n');
	}
	file.close();

	/* is hostapd already running ? */
	/*
	 * For certains scenarios, we need to remove and add
	 * instead of reload (reload did not work).
	 * Consider the following scenario -
	 * Say on CIG 186w as an example
	 * eth0.4086 interface exists with some non-ieee8021x config.
	 * Push ieee8021x config. In general the flow is that
	 * reload_config is called followed by invocation of services (from ucentral-schema)
	 * Services inovation does n't wait until the config reloaded ie in this context
	 * ieee8021x service is invoked much before the network interfaces are recreated.
	 * That is not correct. To handle this, we capture link-up events
	 * and remove the existing interface (in hostapd as shown below) and add again
	 */

	if (ifaces[iface].hostapd) {
		log.syslog(LOG_USER, "Remove the config  ${iface}");
		ubus.call('hostapd', 'config_remove', { iface: iface });
	}
	log.syslog(LOG_USER, "Add config (clear the old one) " + iface);
	ubus.call('hostapd', 'config_add', { iface: iface, config: path });
	system('ifconfig ' + iface + ' up');
	// Subscribe to corresponding hostapd if it is (re)added
	hapd_subscriber.subscribe("hostapd." + iface);
}

/* build a list of all running and new interfaces */
/* handle events from netifd */
function device_subscriber_notify_cb(notify) {
	switch(notify.type) {
	case 'link_down':
		if (!ifaces[notify.data.name])
			break;

		/* de-auth all clients */
		ubus.call(ifaces[notify.data.name].path, 'del_clients');
		ifaces[notify.data.name].authenticated = false;
		flush_iface(notify.data.name);
		ubus.call('ratelimit', 'device_delete', { device: notify.data.name });
		break;
	case 'link_up':
		if (!ifaces[notify.data.name])
			break;
		log.syslog(LOG_USER, "starting iface ${notify.data.name}");
		hostapd_start(notify.data.name);
		break;
	};

	return 0;
}

function subscriber_remove_cb(remove) {
}

/* track and subscribe to a hostapd/netifd object */
function ubus_unsub_object(add, id, path) {
	let object = split(path, '.');

	if (path == 'network.device' && add) {
		device_subscriber.subscribe(path);
	}

	let object_hostapd = object[0];
	let object_ifaces = object[1];

	if (length(object) > 2) {
		// For swconfig platforms
		// The interface name is of the form eth0.4086 etc
		object_ifaces = object[1] + "." + object[2];
	}

	if (object_hostapd != 'hostapd' || !ifaces[object_ifaces])
		return;
	if (add) {
		log.syslog(LOG_USER, "adding " + path);
		hapd_subscriber.subscribe(path);
		ifaces[object_ifaces].hostapd = true;
		ifaces[object_ifaces].path = path;
	} else {
		// Mark the port as unauthorized. but dont delete it
		// ifaces  contains the configured (from uci config) ports
		netifd_handle_iface(object_ifaces, false);
	}
}

/* try to add all enumerated objects */
function ubus_listener_cb(event, payload) {
	ubus_unsub_object(event == 'ubus.object.add', payload.id, payload.path);
}     

/* setup the ubus object listener, allowing us to track hostapd objects that get added and removed */
function ubus_listener_init() {
	/* setup the notification listener */
	hapd_subscriber = ubus.subscriber(hapd_subscriber_notify_cb, subscriber_remove_cb);
	device_subscriber = ubus.subscriber(device_subscriber_notify_cb, subscriber_remove_cb);

	/* enumerate all existing ojects */
	let list = ubus.list();
	for (let k, path in list)
		ubus_unsub_object(true, 0, path);

	/* register add/remove handlers */
	ubus.listener('ubus.object.add', ubus_listener_cb);
	ubus.listener('ubus.object.remove', ubus_listener_cb);
}

function prepare_ifaces() {
	for (let k, v in ifaces)
		v.active = false;

	for (let k, port in config.ports) {
		ifaces[port.iface] ??= {};
		ifaces[port.iface].active = true;
		ifaces[port.iface].authenticated = false;
		ifaces[port.iface].default_vlan = +port.vlan;
		ifaces[port.iface].wan_ports = port.wan_ports;
		ifaces[port.iface].lladdr = [];
	}

	for (let iface, v in ifaces) {
		if (v.active)
			continue;
		ubus.call('hostapd', 'config_remove', { iface });
		delete ifaces[iface];
		system('ifconfig ' + iface + ' down');
	}
}

/* start all active interfaces */
function start_ifaces() {
	for (let iface, v in ifaces)
		hostapd_start(iface);
}

/* shutdown all interfaces */
function shutdown() {
	for (let iface, v in ifaces) {
		log.syslog(LOG_USER, "shutdown");
		ubus.call('hostapd', 'config_remove', { iface });
		netifd_handle_iface(iface, false);
		flush_iface(iface);
		ubus.call('ratelimit', 'device_delete', { device: iface });
	}
}

/* the object published on ubus */
let ubus_methods = {
	dump: {
		call: function(req) {
			return ifaces;
		},
		args: {
		}
	},

	reload: {
		call: function(req) {
			config_load();
			prepare_ifaces();
			start_ifaces();
			return 0;
		},
		args: {
	
		}
	},
};

/* handle rtnl events for carrier-down events */
function rtnl_cb(msg) {
	if (msg.cmd != rtnl.const.RTM_NEWNEIGH)
		return;
	if (!ifaces[msg.msg.dev] || ifaces[msg.msg.dev].authenticated)
		return;
	ubus.call(ifaces[msg.msg.dev].path, 'mac_auth', { addr: msg.msg.lladdr });
}

log.openlog("ieee8021x", log.LOG_PID, log.LOG_USER);
uloop.init();
ubus.publish('ieee8021x', ubus_methods);
config_load();
rtnl.listener(rtnl_cb, null, [ rtnl.const.RTNLGRP_NEIGH ]);
prepare_ifaces();
ubus_listener_init();
start_ifaces();
uloop.run();
uloop.done();
shutdown();
log.closelog();
