let uci = require("uci");
let cursor = uci ? uci.cursor() : null;

const SCAN_FLAG_AP = (1<<2);

function freq2channel(freq) {
	if (freq == 2484)
		return 14;
	else if (freq < 2484)
		return (freq - 2407) / 5;
	else if (freq >= 4910 && freq <= 4980)
		return (freq - 4000) / 5;
	else if(freq >= 56160 + 2160 * 1 && freq <= 56160 + 2160 * 6)
		return (freq - 56160) / 2160;
	else if (freq >= 5955 && freq <= 7115)
		return (freq - 5950) / 5;
	else
		return (freq - 5000) / 5;
}

function phy_get(wdev) {
        let res = global.nl80211.request(global.nl80211.const.NL80211_CMD_GET_WIPHY, global.nl80211.const.NLM_F_DUMP, { split_wiphy_dump: true });

        if (res === false)
                warn("Unable to lookup phys: " + global.nl80211.error() + "\n");

        return res;
}

let paths = {};

function add_path(path, phy, index) {
	if (!phy)
		return;
	phy = global.fs.basename(phy);
	paths[phy] = path;
	if (index)
		paths[phy] += '+' + index;
}

function lookup_paths() {
	let wireless = cursor.get_all('wireless');
	for (let k, section in wireless) {
		if (section['.type'] != 'wifi-device' || !section.path)
			continue;
		let phys = global.fs.glob(sprintf('/sys/devices/%s/ieee80211/phy*', section.path));
		if (!length(phys))
			phys = global.fs.glob(sprintf('/sys/devices/platform/%s/ieee80211/phy*', section.path));
		if (!length(phys))
			continue;
		sort(phys);
		let index = 0;
		for (let phy in phys)
			add_path(section.path, phy, index++);
	}
}

function get_hwmon(phy) {
	let hwmon = global.fs.glob(sprintf('/sys/class/ieee80211/%s/hwmon*/temp*_input', phy));
	if (!hwmon)
		return 0;
	let file = global.fs.open(hwmon[0], 'r');
	if (!file)
		return 0;
	let temp = +file.read('all');
	file.close();
	return temp;
}

function lookup_phys() {
	lookup_paths();

	let phys = phy_get();
	let ret = {};
	for (let phy in phys) {
		let phyname = 'phy' + phy.wiphy;
		let path = paths[phyname];
		if (!path)
			continue;

		let p = { path, phyname, index: phy.wiphy };
		p.htmode = [];
		p.band = [];
		for (let band in phy.wiphy_bands) {
			for (let freq in band?.freqs) {
				if (freq.disabled)
					continue;
				if (freq.freq >= 6000)
					push(p.band, '6G');
				else if (freq.freq <= 2484)
					push(p.band, '2G');
				else if (freq.freq >= 5160 && freq.freq <= 5885)
					push(p.band, '5G');
			}
			if (band?.ht_capa) {
				p.ht_capa = band.ht_capa;
				push(p.htmode, 'HT20');
				if (band.ht_capa & 0x2)
					push(p.htmode, 'HT40');
			}
			if (band?.vht_capa) {
				p.vht_capa = band.vht_capa;
				push(p.htmode, 'VHT20', 'VHT40', 'VHT80');
				let chwidth = (band?.vht_capa >> 2) & 0x3;
				switch(chwidth) {
				case 2:
					push(p.htmode, 'VHT80+80');
					/* fall through */
				case 1:
					push(p.htmode, 'VHT160');
				}
			}
			for (let iftype in band?.iftype_data) {
				if (iftype.iftypes?.ap) {
					p.he_phy_capa = iftype?.he_cap_phy;
					p.he_mac_capa = iftype?.he_cap_mac;
					push(p.htmode, 'HE20');
					let chwidth = (iftype?.he_cap_phy[0] || 0) & 0xff;
					if (chwidth & 0x2 || chwidth & 0x4)
						push(p.htmode, 'HE40');
					if (chwidth & 0x4)
						push(p.htmode, 'HE80');
					if (chwidth & 0x8 || chwidth & 0x10)
						push(p.htmode, 'HE160');
					if (chwidth & 0x10)
						push(p.htmode, 'HE80+80');
				}
			}
		}

		p.band = uniq(p.band);
		if (!length(p.dfs_channels))
			delete p.dfs_channels;
		ret[phyname] = p;
	}
	return ret;
}

return {
	phys: lookup_phys(),

	status: function() {
		return this.phys;
	},

	init: function() {
		/* send an event */
		global.event.send('rrm.phy', this.phys);
	},

	txpower: function(msg) {
		if (!msg.bssid || !msg.level)
			return false;
		let wiphy = global.local.bssid_to_phy(msg.bssid);
		if (wiphy < 0)
			return false;	
		global.nl80211.request(global.nl80211.const.NL80211_CMD_SET_WIPHY, 0, { wiphy, wiphy_tx_power_setting: 2, wiphy_tx_power_level: msg.level * 100});
		return true;
	},
};
