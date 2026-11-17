let uci = require("uci");
let cursor = uci ? uci.cursor() : null;

const SCAN_FLAG_AP = (1<<2);
const TX_POWER_FIXED = 2;	/* NL80211_TX_POWER_FIXED */

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
		let phyname;
		if (phy?.wiphy_name)
			phyname = phy.wiphy_name;
		else if (phy?.wiphy != null)
			phyname = 'phy' + phy.wiphy;
		else
			continue;

		let path = paths[phyname];
		if (!path)
			continue;

		let p = { path, phyname, index: phy.wiphy };
		p.channels = [];
		p.dfs_channels = [];
		p.htmode = [];
		p.band = [];
		for (let band in phy.wiphy_bands) {
			for (let freq in band?.freqs) {
				if (freq.disabled)
					continue;
				push(p.channels, freq2channel(freq.freq));
				if (freq.radar)
					push(p.dfs_channels, freq2channel(freq.freq));
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

/* tx power helpers (MLO / link-aware, for ath12k single-wiphy) */
function iface_get(dev) {
	let info = global.nl80211.request(global.nl80211.const.NL80211_CMD_GET_INTERFACE, 0, { dev });
	if (!info)
		warn(`txpower: GET_INTERFACE ${dev} failed: ${global.nl80211.error()}\n`);
	return info;
}

/* ucode versions differ in how the nested link id is named */
function link_id_of(l) {
	if (l?.link_id != null)
		return l.link_id;
	if (l?.mlo_link_id != null)
		return l.mlo_link_id;
	return null;
}

/*
 * Returns:
 *   { link: null }    interface has no mlo_links (legacy ath11k / MTK): do not send mlo_link_id
 *   { link: {...} }   link matching the BSSID
 *   { err: "..." }    several links but none matches the BSSID
 */
function find_link(info, bssid) {
	let links = info?.mlo_links;
	if (type(links) != "array" || !length(links))
		return { link: null };

	bssid = lc(bssid);
	for (let l in links)
		if (l?.mac && lc(l.mac) == bssid)
			return { link: l };

	/* single-link vif (e.g. phy5g-ap0 with only "link 0"): link MAC may not be reported */
	if (length(links) == 1)
		return { link: links[0] };

	return { err: `no MLO link matches ${bssid}` };
}

function txpower_set_mbm(dev, mbm, link_id) {
	let req = { dev, wiphy_tx_power_setting: TX_POWER_FIXED, wiphy_tx_power_level: mbm };
	if (link_id != null)
		req.mlo_link_id = link_id;
	let ret = global.nl80211.request(global.nl80211.const.NL80211_CMD_SET_WIPHY, 0, req);
	if (ret === false ) {
		warn(`txpower: SET_WIPHY ${dev} link=${link_id} ${mbm} mBm failed: ${global.nl80211.error()}\n`);
		return false;
	}
	return true;
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

		let dev = global.local.bssid_to_ifname(msg.bssid);
		if (dev == null)
			return false;

		let info = iface_get(dev);
		if (!info)
			return false;

		let r = find_link(info, msg.bssid);
		if (r.err) {
			warn(`txpower: ${dev}: ${r.err}\n`);
			return false;
		}

		let want_mbm = msg.level * 100;
		let link_id = link_id_of(r.link);

		if (!txpower_set_mbm(dev, want_mbm, link_id))
			return false;

		return true;
	},

	get_txpower: function(bssid) {
		let dev = global.local.bssid_to_ifname(bssid);
		if (dev == null)
			return false;

		let info = iface_get(dev);
		if (!info)
			return false;

		let r = find_link(info, bssid);
		if (r.link?.wiphy_tx_power_level != null)
			return r.link.wiphy_tx_power_level;

		return info.wiphy_tx_power_level;
	},
};
