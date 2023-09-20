let nl80211 = require("nl80211");
let def = nl80211.const;
let hapd_subscriber;
let ucentral_subscriber;
let state = {};
let hapd = {};
let handlers = {};

function channel_to_freq(cur_freq, channel) {

	let band;
	if (cur_freq <= 2484)
		band = '2G';
	else if (cur_freq <= 5885)
		band = '5G';
	else
		band = '6G';

	if (band == '2G' && channel >= 1 && channel <= 13)
		return 2407 + channel * 5;
	else if (band == '2G' && channel == 14)
		return 2484;
	else if (band == '5G' && channel >= 7 && channel <= 177)
		return 5000 + channel * 5;
	else if (band == '5G' && channel >= 183 && channel <= 196)
		return 4000 + channel * 5;
	else if (band == '60G' && channel >= 1 && channel <= 6)
		return 56160 + channel * 2160;

       return null;
}

function channel_survey(dev) {
	/* trigger the nl80211 call that gathers channel survey data */
	let res = nl80211.request(def.NL80211_CMD_GET_SURVEY, def.NLM_F_DUMP, { dev });

	if (!res) {
		ulog_err(sprintf('failed to update survey for %s', dev));
		return;
	}

	/* iterate over the result and filter out the correct channel */
	for (let survey in res) {
		if (survey?.survey_info?.frequency != hapd[dev].freq)
			continue;
		if (survey.survey_info.noise)
			hapd[dev].noise = survey.survey_info.noise;
		if (survey.survey_info.time && survey.survey_info.busy) {
			let time = survey.survey_info.time - (state[dev].time || 0);
			let busy = survey.survey_info.busy - (state[dev].busy || 0);
			state[dev].time = survey.survey_info.time;
			state[dev].busy = survey.survey_info.busy;

			let load = (100 * busy) / time;
			if (hapd[dev].load)
				hapd[dev].load = 0.85 * hapd[dev].load + 0.15 * load;
			else
				hapd[dev].load = load;
		}
	}
}

function hapd_update() {
	/* todo: prefilter frequency */
	for (let key in state)
		channel_survey(key);
	return 5000;
}

function hapd_subunsub(path, sub) {
	/* check if this is a hostapd instance */
	let name = split(path, '.');

	if (length(name) != 2 || name[0] != 'hostapd')
		return;
	name = name[1];

	ulog_info(sprintf('%s %s\n', sub ? 'add' : 'remove', path));

	/* the hostapd instance disappeared */
	if (!sub) {
		global.event.send('rrm.bss.del', { bssid: hapd[name] });
		global.neighbor.local_del(name);
		delete hapd[name];
		delete state[name];
		return;
	}

	/* gather initial data from hostapd */
	let status = global.ubus.conn.call(path, 'get_status');
	if (!status)
		return;

	let cfg = uci.get_all('usteer2', status.uci_section);
	if (!cfg)
		cfg = {};

	/* subscibe to hostapd */
	hapd_subscriber.subscribe(path);

	/* tell hostapd to wait for a reply before answering probe requests */
	//global.ubus.conn.call(path, 'notify_response', { 'notify_response': 1 });

	/* tell hostapd to enable rrm/roaming */
	global.ubus.conn.call(path, 'bss_mgmt_enable', { 'neighbor_report': 1, 'beacon_report': 1, 'bss_transition': 1 });

	/* instantiate state */
	hapd[name] = { };
	state[name] = { };

	for (let prop in [ 'ssid', 'bssid', 'freq', 'channel', 'op_class', 'uci_section' ])
		if (status[prop])
			hapd[name][prop] = status[prop];
	hapd[name].config = cfg;

	/* ask hostapd for the local neighbourhood report data */
	let rrm = global.ubus.conn.call(path, 'rrm_nr_get_own');
	if (rrm && rrm.value) {
		hapd[name].rrm_nr = rrm.value;
		global.neighbor.local_add(name, rrm.value);
	}

	global.neighbor.update();
	
	/* trigger an initial channel survey */
	//channel_survey(name);

	/* send an event */
	global.event.send('rrm.bss.add', {
		bssid: hapd[name].bssid,
		ssid: hapd[name].ssid,
		freq: hapd[name].freq,
		channel: hapd[name].channel,
		op_class: hapd[name].op_class,
		rrm_nr: hapd[name].rrm_nr,
	});
}

function ucentral_subunsub(sub) {
	if (!sub)
		return;
	ucentral_subscriber.subscribe('ucentral');
}

function hapd_listener(event, msg) {
	hapd_subunsub(msg.path, event == 'ubus.object.add');

	if (msg.path == 'ucentral')
		ucentral_subunsub(event == 'ubus.object.add');
}

function hapd_handle_event(req) {
	/* iterate over all handlers for this event type, if 1 or more handlers replied with false, do not reply to the notification */
	let reply = true;
	for (let handler in handlers[req.type])
		if (!handler(req.type, req.data))
			reply = false;
	if (!reply)
		return;
	req.reply();
}

function ucentral_handle_event(req) {
	printf('%.J\n', req);
}

function channel_switch_handler(type, data) {
	global.event.send('rrm.bss.channel-switch', { bssid: data.bssid, freq: data.freq });
}

return {
	status: function() {
		return hapd;
	},

	init: function() {
		hapd_subscriber = global.ubus.conn.subscriber(
			hapd_handle_event,
			function(msg) {	});
		ucentral_subscriber = global.ubus.conn.subscriber(
			ucentral_handle_event,
			function(msg) { });

		/* register a callback that will monitor hostapd instances spawning and disappearing */
		global.ubus.conn.listener('ubus.object.add', hapd_listener);
		global.ubus.conn.listener('ubus.object.remove', hapd_listener);

		/* iterade over all existing hostapd instances and subscribe to them */
		for (let path in global.ubus.conn.list()) {
			hapd_subunsub(path, true);
			if (path == 'ucentral')
				ucentral_subunsub(true);
		}

//		uloop_timeout(hapd_update, 5000);
		global.local.register_handler('channel-switch', channel_switch_handler);
	},

	register_handler: function(event, handler) {
		/* a policy requested to be notified of action frames, register the callback */
		handlers[event] ??= [];
		push(handlers[event], handler);
	},

	switch_chan: function(msg) {
		if (!msg.bssid || !msg.channel)
			return false;
		for (let bss, v in hapd) {
			if (v.bssid != lc(msg.bssid))
				continue;
			return global.ubus.conn.call('hostapd.' + bss, 'switch_chan', {
				freq: channel_to_freq(v.freq, msg.channel),
				bcn_count: 10
			}) == null;
		}
		return false;
	},

	bssid_to_phy: function(bssid) {
		for (let bss, v in hapd) {
			if (v.bssid != lc(bssid))
				continue;
			let iface = global.nl80211.request(global.nl80211.const.NL80211_CMD_GET_INTERFACE, 0, { dev: bss });
			return iface.wiphy;
		}
		return -1;
	},

	txpower: function(bssid) {
		for (let bss, v in hapd) {
			if (v.bssid != lc(bssid))
				continue;
			let iface = global.nl80211.request(global.nl80211.const.NL80211_CMD_GET_INTERFACE, 0, { dev: bss });
			return iface.wiphy_tx_power_level;
		}
		return 0;
	},
};
