#!/usr/bin/ucode 
import { readfile } from "fs";

let nl = require("nl80211");
let def = nl.const;
let board_name = rtrim(readfile('/tmp/sysinfo/board_name'), '\n');
let phy_index = 0;

function phy_get() {
	let res = nl.request(def.NL80211_CMD_GET_WIPHY, def.NLM_F_DUMP, { split_wiphy_dump: true });

	if (res === false)
		warn("Unable to lookup phys: " + nl.error() + "\n");

	return res;
}

switch(board_name) {
case 'edgecore,eap112':
        phy_index = 1;
        break;
}

let phys = phy_get();
printf("%d\n", phys[phy_index].max_ap_assoc || 32);
