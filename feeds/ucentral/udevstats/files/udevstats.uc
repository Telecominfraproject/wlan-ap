#!/usr/bin/ucode
'use strict';
let bpf = require("bpf");
let struct = require("struct");
let fs = require("fs");
let ubus = require("ubus");
let uloop = require("uloop");
let PRIO_VAL = 0x200;

bpf.set_debug_handler(function(level, msg) { print(`[${level}] ${msg}`); });

let bpf_mod = bpf.open_module("/lib/bpf/udevstats.o", {
	"program-type": {
		udevstats_in: bpf.BPF_PROG_TYPE_SCHED_CLS,
		udevstats_out: bpf.BPF_PROG_TYPE_SCHED_CLS
	}
});
assert(bpf_mod, `Could not load BPF module: ${bpf.error()}`);
bpf.set_debug_handler(null);

let map = bpf_mod.get_map("vlans");
assert(map, `Could not find vlan map in BPF module`);

let prog = {
	ingress: bpf_mod.get_program("udevstats_in"),
	egress: bpf_mod.get_program("udevstats_out")
};
assert(prog.ingress && prog.egress, "Missing BPF program");

function device_list_init() {
	return {
		ingress: {},
		egress: {}
	};
}

let old_hooks = device_list_init();
let hooks = device_list_init();
let old_vlans = {};
let vlans = {};
let vlan_config;

function device_update_start() {
	old_hooks = hooks;
	hooks = device_list_init();
}

function dev_ifindex(name) {
	try {
		return int(fs.readfile(`/sys/class/net/${name}/ifindex`));
	} catch (e) {
		return 0
	}
}

function device_hook_get(name, tx) {
	let ifindex;
	let dev;

	let type = tx ? "ingress" : "egress";

	dev = hooks[type][name];
	if (dev)
		return dev;

	dev = old_hooks[type][name];
	if (dev) {
		delete old_hooks[type][name];
	} else {
		dev = {
			vlans: {}
		};
	}

	ifindex = dev_ifindex(name);
	if (!ifindex)
		return null;

	if (dev.ifindex != ifindex)
		prog[type].tc_attach(name, type, PRIO_VAL);

	dev.ifindex = ifindex;
	dev.tx = tx;

	hooks[type][name] = dev;

	return dev;
}

function device_update_end() {
	for (let type in [ "ingress", "egress" ])
		for (let dev in old_hooks[type])
			bpf.tc_detach(dev, type, PRIO_VAL);

	old_hooks = device_list_init();
}

function vlan_update_start() {
	old_vlans = vlans;
	vlans = {};
	device_update_start();
}

function vlan_update_end() {
	device_update_end();

	for (let key in old_vlans)
		map.delete(b64dec(key));
}

function vlan_key(ifindex, vid, tx, ad)
{
	return struct.pack("IH??", ifindex, vid, tx, ad);
}

function vlan_add(dev, vid, ad)
{
	if (!dev.ifindex)
		return;

	let key = vlan_key(dev.ifindex, vid, dev.tx, ad);
	let keystr = b64enc(key);

	if (old_vlans[keystr])
		delete old_vlans[keystr];
	else
		map.set(key, struct.pack("QQ", 0, 0));

	vlans[keystr] = true;
}

function vlan_config_push(vlan_config, dev, vid)
{
	let vlan_found = false;

	for (let v in vlan_config[dev]) {
		if (v[0] == vid) {
			vlan_found = true;
			break;
		}
	}

	if (!vlan_found)
		push(vlan_config[dev], [ vid, "rx", "tx"]);
}

function vlan_set_config(config)
{
	vlan_config = config;

	vlan_update_start();
	for (let dev in config) {
		for (let vlan in config[dev]) {
			vlan = [...vlan];
			let vid = shift(vlan);
			for (let type in vlan) {
				let tx;

				if (type == "tx")
					tx = true;
				else if (type == "rx")
					tx = false;
				else
					continue;

				let hook = device_hook_get(dev, tx);
				vlan_add(hook, vid, false);
			}
		}
	}
	vlan_update_end();
}

function vlan_dump_stats()
{
	let stats = {};
	for (let dev in vlan_config) {
		stats[dev] = [];
		let vlans = sort(vlan_config[dev], (a, b) => a[0] - b[0]);
		for (let vlan in vlans) {
			let vlan_stats = {
				vid: vlan[0]
			};
			for (let tx in [ true, false ]) {
				let hook = device_hook_get(dev, tx);

				if (!hook.ifindex)
					continue;

				let stats = map.get(vlan_key(hook.ifindex, vlan[0], tx, false));
				if (!stats)
					continue;

				stats = struct.unpack("QQ", stats);
				vlan_stats[tx ? "tx" : "rx"] = {
					packets: stats[0],
					bytes: stats[1]
				}
			}
			push(stats[dev], vlan_stats);
		}
	}

	return stats;
}


function run_service() {
	let uctx = ubus.connect();

	uctx.publish("udevstats", {
		config_set: {
			call: function(req) {
				if (!req.args.devices)
					return ubus.STATUS_INVALID_ARGUMENT;

				vlan_set_config(req.args.devices);
				return 0;
			},
			args: {
				"devices": {}
			}
		},
		check_devices: {
			call: function(req) {
				if (vlan_config)
					vlan_set_config(vlan_config);

				return 0;
			},
			args: {}
		},
		add_device: {   
			call: function(req) {                   
				if (!req.args.device || !req.args.vlan)
					return ubus.STATUS_INVALID_ARGUMENT;
				if (!vlan_config[req.args.device])
					vlan_config[req.args.device] = [];
				vlan_config_push(vlan_config, req.args.device, req.args.vlan);
				vlan_set_config(vlan_config);
				return 0;                           
			},                           
			args: {                     
				"device": "wlan0",
				"vlan": 100,               
			}                                                                       
		}, 
		reset: {
			call: function(req) {
				let old_config = vlan_config;

				vlan_set_config({});
				if (old_config)
					vlan_set_config(old_config);

				return 0;
			},
			args: {},
		},
		dump: {
			call: function(req) {
				return vlan_dump_stats();
			},
			args: {}
		}
	});

	try {
		uloop.run();
	} catch(e) {
		warn(`Error: ${e}\n${e.stacktrace[0].context}`);
	}

	vlan_set_config({});
}

uloop.init();
run_service();
uloop.done();
