#!/usr/bin/ucode 

let nl = require("nl80211");
let def = nl.const;

function phy_get() {
	let res = nl.request(def.NL80211_CMD_GET_WIPHY, def.NLM_F_DUMP, { split_wiphy_dump: true });

	if (res === false)
		warn("Unable to lookup phys: " + nl.error() + "\n");

	return res;
}

let phys = phy_get();
printf("%d\n", phys[0].max_ap_assoc || 32);
