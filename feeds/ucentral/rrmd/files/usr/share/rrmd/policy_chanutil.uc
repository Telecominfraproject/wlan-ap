let board_info = global.ubus.conn.call('system', 'board');
let _pdev_stats = {};
let config = {
    /* Channel utilization threshold: When Utilization of the current channel or adjacent channel reaches the configured threshold (in %), the AP switches to a different Channel.
    Set this field to 0 to disable this feature. Range: 0-99 */
    threshold: 0,
    /* the periodicity for checking channel utilization (ms)*/
    interval: 86400*1000,
    /* the channel optimization algorithm: RCS (1)/ ACS(2) */
    algo: 1,
    /* the number of consecutive utilization threshold breaches allowed before Channel selection is triggered */
    consecutive_threshold_breach: 1,
};

// total number of radios: default=2
let num_radios = 2;

let board_name = board_info.board_name;
switch(board_name) {
case 'edgecore,eap105':
case 'edgecore,oap101-6e':
case 'edgecore,oap101e-6e':
    num_radios = 3;
    break;
}

function stats_info_read(path) {
    let res = trim(fs.readfile(path));
    return res || 0;
}

function stats_info_write(path, value) {
	let file = fs.open(path, 'w+');
	if (!file)
		return;
	file.write(value);
	file.close();
}

function cool_down_check(iface_num, cool_down_period) {
    let now_t = time();
    let cool_down_f= 0;

    // check cool_down_period passed or not
    let deltaTS = now_t - stats_info_read("/tmp/chan_switch_t" + iface_num);
    ulog_info(`[wlan%d] Cool down check: current_time=%d, chan_switch_time=%d, deltaTS(time passed)=%d \n`, iface_num, now_t, stats_info_read("/tmp/chan_switch_t" + iface_num), deltaTS);

    if (deltaTS < (cool_down_period/1000)) {
        ulog_info(`[wlan%d] Need to cool down (%d seconds hasn't passed) before switching channel \n`, iface_num, cool_down_period/1000);
        // Flag cool down
        cool_down_f = 1;
    }

    return cool_down_f;
}

function update_channel_switch_time(iface_num) {
    stats_info_write("/tmp/chan_switch_t" + iface_num, time());
}

function update_breach_count(iface_num, breach_count) {
    stats_info_write("/tmp/threshold_breach_count_phy" + iface_num , breach_count);
}

function random_time_calc() {
    // calculate random wait time before ACS is triggered based on the device SN
    let device_SN;
    let sn_flag = 0;
    let unique_random;
    let random_wait_time = 25;
    let command = "fw_printenv SN";

    let fp_SN = fs.popen(command);
    device_SN = fp_SN.read('all');
    fp_SN.close();

    if (device_SN) {
        unique_random = (match(trim(device_SN), /^(SN=EC)(\d+)/)[2]);
    } else {
        command = "fw_printenv serial_number";
        let fp_serial_num = fs.popen(command);
        device_SN = fp_serial_num.read('all');
        fp_serial_num.close();

        if (device_SN) {
            unique_random = (match(trim(device_SN), /^(serial_number=EC)(\d+)/)[2]);
        } else {
            // If there's no valid SN, set a flag to get fixed random
            sn_flag = 1;
        }
    }

    if (sn_flag != 1) {
        random_wait_time = unique_random % 25;
    }

    return random_wait_time;
}

function channel_to_freq(band, channel) {
    if (band == '2g' && channel >= 1 && channel <= 13)
        return 2407 + channel * 5;
    else if (band == '2g' && channel == 14)
        return 2484;
    else if (band == '5g' && channel >= 7 && channel <= 177)
        return 5000 + channel * 5;
    else if (band == '5g' && channel >= 183 && channel <= 196)
        return 4000 + channel * 5;
    else if (band == '6g' && channel >= 5)
        return 5950 + channel * 5;
    else if (band == '60g' && channel >= 1 && channel <= 6)
        return 56160 + channel * 2160;

       return null;
}

function center_freq_calc(band, freq, bandwidth) {
    if (band == '5g') {
        if (bandwidth == 40)
            return +freq + 10;
        if (bandwidth == 80)
            return +freq + 30;
        if (bandwidth == 160)
            return +freq + 70;
    }

    return +freq;
}

function channel_offset_calc(band, bandwidth) {
    let offset=null;

    if (band == '5g') {
        if (bandwidth == 40)
            offset = 1;
        if (bandwidth == 80)
            offset = 1;
    }

    return offset;
}

function interface_status_check(iface_num) {
    ulog_info(`[wlan%d] Checking interface status ... \n`, iface_num);
    let radio_status = 'DISABLED';
    let radio_down_f = 0;

    let curr_stat = global.ubus.conn.call(`hostapd.wlan${iface_num}`, 'get_status');
    if (curr_stat) {
        radio_status = curr_stat.status;
    } else {
        radio_status = "DISABLED";
        radio_down_f = 1;
    }
    ulog_info(`[wlan%d] status: %s \n`, iface_num, radio_status);

    return radio_down_f;
}

function check_current_channel(iface_num) {
    // get wireless interface's live status & channel using "ubus call hostapd.wlan<iface_num> get_status"
    let curr_stat = global.ubus.conn.call(`hostapd.wlan${iface_num}`, 'get_status');
    let current_channel = curr_stat.channel;
    if (curr_stat && current_channel) {
        ulog_info(`[wlan%d] Current channel (from hostapd) = %d \n`, iface_num, current_channel);
    }

    return current_channel;
}

function rrmd_switch_channel(msg) {
    ulog_info(`[wlan%d] Start switch channel to %d (%s)\n`, msg.phy, msg.channel, msg.bssid);

    let chan_switch_status = 0;

    let mode = lc(replace(msg.htmode, /[^a-zA-Z]/g, ''));
    let bandwidth = replace(msg.htmode, /[^0-9]/g, '');

    let target_freq = channel_to_freq(msg.band, msg.channel);
    let center_freq = center_freq_calc(msg.band, target_freq, bandwidth);
    let sec_channel_offset = channel_offset_calc(msg.band, bandwidth);

    // use hostadp_cli command
    if (target_freq != null) {
        ulog_info(`Sending to hostapd (Chan %d):: freq=%d, center_freq=%d, sec_channel_offset=%d, bandwidth=%d, mode=%s \n`, msg.channel, target_freq, center_freq, sec_channel_offset, bandwidth, mode);
        // system(`/usr/sbin/hostapd_cli -i wlan${msg.phy} chan_switch 5 ${target_freq} center_freq1=${center_freq} sec_channel_offset=${sec_channel_offset} bandwidth=${bandwidth} ${mode}`);

        let command_hostapd = `/usr/sbin/hostapd_cli -i wlan${msg.phy} chan_switch 5 ${target_freq} center_freq1=${center_freq} sec_channel_offset=${sec_channel_offset} bandwidth=${bandwidth} ${mode}`;
        let res = fs.popen(command_hostapd);
        let res_check = trim(res.read('all'));
        res.close();

        if (res_check == "OK") {
            ulog_info(`hostapd_cli chan_switch status: OK \n`);
            chan_switch_status = 1;
            update_channel_switch_time(msg.phy);

            // reset breach count back to 0 as we are calling the channel selection algo
            update_breach_count(msg.phy, 0);
        } else {
            ulog_info(`hostapd_cli chan_switch status: FAIL \n`);
        }
    }
    else {
        ulog_info(`Channel to frequency conversion fail \n`);
    }

    return chan_switch_status;
}

function switch_status_check(iface_num, radio_5G_index, dfs_enabled_5g_index) {
    // need to wait for radio 5GHz interface to be UP, when DFS is enabled
    if (dfs_enabled_5g_index == 1) {
        ulog_info(`[wlan%d] 5G radio might need some time to be UP (DFS enabled) \n`, radio_5G_index);

        let p = 0;
        // Max 65 seconds wait for the DFS enabled interface to be UP
        let timer = 70;

        while (p < timer) {
            ulog_info(`[wlan%d] Check#%d \n `, iface_num, p);

            if (interface_status_check(iface_num) == 1) {
                ulog_info(`[wlan%d] Interface not UP yet ... wait for 1 second \n`, iface_num);
                sleep(1000);
                p++;

                if (p >= timer) {
                    ulog_info(`[wlan%d] Interface not UP yet ... cac_time is long \n`, iface_num);
                    return;
                }
            } else {
                ulog_info(`[wlan%d] Interface is UP! \n`, iface_num);
                break;
            }
        }
    }

    let current_chan = check_current_channel(iface_num);
    return current_chan;
}

function dfs_chan_check(iface_num, rcs_channel) {
    let phy_id = "phy" + iface_num;
    let dfs_enabled_5g = 0;
    let dfs_chan_list =  global.phy.phys[phy_id].dfs_channels;

    // check if rcs_channel is in dfs_channel list
    for (let dfs_chan in dfs_chan_list) {
        if (dfs_chan == rcs_channel) {
            // flag up if dfs channel detected
            dfs_enabled_5g = 1;
            break;
        }
    }

    return dfs_enabled_5g;
}

function fixed_channel_algo(iface_num, fixed_channel_f, auto_channel_f, fixed_chan_bkp, channel_config) {
    // if fixed channel config is stored in the /tmp/fixed_channel_phyX file
    if (fixed_channel_f == 1) {
        if (auto_channel_f == 1) {
            // if current channel is auto => change to fixed
            ulog_info(`[wlan%d] Current channel is "auto"; Configured fixed channel was %d, reassigning fixed channel ...\n`, iface_num, fixed_chan_bkp);

            // Set channel to the fixed channel
            global.uci.set('wireless', 'radio' + iface_num, 'channel', fixed_chan_bkp);
            global.uci.commit('wireless');
            global.uci.apply;
        } else if (auto_channel_f == 0) {
            // if current channel is not auto => change to auto
            ulog_info(`[wlan%d] Current channel is fixed (=%d), changing it to "auto" (=0) to trigger ACS \n`, iface_num, channel_config);

            // Set channel to "auto"
            global.uci.set('wireless', 'radio' + iface_num, 'channel', '0');
            global.uci.commit('wireless');
            global.uci.apply;
        }
    }
}

function get_chan_util(iface_num, sleep_time) {
	let pdev_stats = {};
	let chan_util = 0;

    let prev_values = {
		txFrameCount: null,
		rxFrameCount: null,
		rxClearCount: null,
		cycleCount: null,
	};

    for (let c = 0; c < 2; c++) {
        // Check tx and tx stats for wlanX interface
        _pdev_stats[iface_num] = '/tmp/pdev_stats_phy' + iface_num;

        let curr_values = {
            txFrameCount: null,
            rxFrameCount: null,
            rxClearCount: null,
            cycleCount: null,
        };

        pdev_stats = split(stats_info_read(_pdev_stats[iface_num]), "\n");

        if (pdev_stats != null) {
            for (let curr_value in pdev_stats) {
                let txFrameCount = match(trim(curr_value), /^TX frame count(\s+\d+)/);
                if (txFrameCount)
                    curr_values.txFrameCount = trim(txFrameCount[1]);

                let rxFrameCount = match(trim(curr_value), /^RX frame count(\s+\d+)/);
                if (rxFrameCount)
                    curr_values.rxFrameCount = trim(rxFrameCount[1]);

                let rxClearCount = match(trim(curr_value), /^RX clear count(\s+\d+)/);
                if (rxClearCount)
                    curr_values.rxClearCount = trim(rxClearCount[1]);

                let cycleCount = match(trim(curr_value), /^Cycle count(\s+\d+)/);
                if (cycleCount)
                    curr_values.cycleCount = trim(cycleCount[1]);

                if (curr_values.txFrameCount && curr_values.rxFrameCount && curr_values.rxClearCount && curr_values.cycleCount) {
                    break;
                }
            }

            let ignore = 0;

            if (!prev_values.txFrameCount || !prev_values.rxFrameCount || !prev_values.rxClearCount || !prev_values.cycleCount) {
                ignore = 1;
            }

            if ((curr_values.cycleCount) <= (prev_values.cycleCount) || (curr_values.txFrameCount) < (prev_values.txFrameCount) ||
                (curr_values.rxFrameCount) < (prev_values.rxFrameCount) || (curr_values.rxClearCount) < (prev_values.rxClearCount)) {
                ignore = 1;
            }

            if (ignore != 1) {
                let cycle_count_delta = curr_values.cycleCount - prev_values.cycleCount;
                /* let tx_count_delta = curr_values.txFrameCount - prev_values.txFrameCount;
                let rx_count_delta = curr_values.rxFrameCount - prev_values.rxFrameCount; */
                let rx_clear_delta = curr_values.rxClearCount - prev_values.rxClearCount;

                let total_usage = (rx_clear_delta * 100) / cycle_count_delta;
                /* let tx_usage = (tx_count_delta * 100) / cycle_count_delta;
                let rx_usage = (rx_count_delta * 100) / cycle_count_delta; */

                chan_util = total_usage;
            }

            prev_values.txFrameCount=curr_values.txFrameCount;
            prev_values.rxFrameCount=curr_values.rxFrameCount;
            prev_values.rxClearCount=curr_values.rxClearCount;
            prev_values.cycleCount=curr_values.cycleCount;
        }
        sleep(sleep_time);
    }

    return chan_util;
}

function channel_selection(iface_num, band, htmode, chan_list_valid) {
    let math = require('math');
    let phy_id = "phy" + iface_num;
    let bw = replace(htmode, /[^0-9]/g, '');

    // channel list from the driver based on the country code
    let chan_list_cc = global.phy.phys[phy_id].channels;
    // complete channel list
    let chan_list_default = {};
    // allowed channel list to select random channel from
    let chan_list_allowed = [];

    let chan_list_init = [];
    let chan_list_legal = [];

    ulog_info(`[wlan%d] Channel list from the driver = %s \n`, iface_num, chan_list_cc);
    ulog_info(`[wlan%d] Selected channel list from config (default channel list shall be used in case channels haven't been selected) = %s \n`, iface_num, chan_list_valid);

    // default channel list
    if (band == '5g') {
        chan_list_default = {
            "320": [ 0 ],
            "160": [ 36, 100 ],
            "80": [
                36,
                52,
                100,
                116,
                132,
                149,
                165
            ],
            "40": [
                36, 44,
                52, 60,
                100, 108,
                116, 124,
                132, 140,
                149, 157,
                165
            ],
            "20": [
                36, 40, 44, 48,
                52, 56, 60, 64,
                100, 104, 108, 112,
                116, 120, 124, 128,
                132, 136, 140, 144,
                149, 153, 157, 161,
                165
            ]
        };
    } else if (band == '2g') {
        chan_list_default = {
            "20": [
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
            ]
        };
    }

    // initial channel list after comparing the default chan list based on current bw, and country code
    for (let default_chan in chan_list_default[bw]) {
        for (let cc_chan in chan_list_cc) {
            if (default_chan == cc_chan){
                push(chan_list_init, default_chan);
            }
        }
    }

    if (bw == "80" || bw == "40") {
        // exclude last channels from the channel list when bw is 80MHz or 40MHz to avoid selecting a channel with a secondary channel that cannot be supported
        chan_list_legal = slice(chan_list_init, 0, length(chan_list_init)-1) ;
    } else {
        chan_list_legal = chan_list_init;
    }

    if (chan_list_valid) {
        // if channel list is provided in the wireless config, check if it is valid based on the bandwidth selected and the country code
        for (let valid_chan in chan_list_valid) {
            for (let legal_chan in chan_list_legal) {
                if (valid_chan == legal_chan) {
                    push(chan_list_allowed, valid_chan);
                }
            }
        }
    } else {
        // if channel list is not provided in the wireless config, check if the channel from the default channel list is valid based on the country code
        chan_list_allowed = chan_list_legal;
    }

    ulog_info(`[wlan%d] Allowed channel list = %s \n`, iface_num, chan_list_allowed);

    // select random channel from chan_list_allowed
    let random_channel_idx = sprintf('%d', math.rand() % length(chan_list_allowed));
    let random_channel = chan_list_allowed[random_channel_idx];
    ulog_info(`[wlan%d] Selected random channel = %d \n`, iface_num, random_channel);

    return random_channel;
}

function algo_rcs(iface_num, current_channel, band, htmode, selected_channels) {
    let chosen_random_channel = 0;
    let res = 0;

    // channel_selection script will help to select random channel
    chosen_random_channel = channel_selection(iface_num, band, htmode, selected_channels);
    stats_info_write("/tmp/phy" + iface_num + "_rrmchan", chosen_random_channel);

    if (chosen_random_channel == current_channel) {
        ulog_info(`[wlan%d] RCS assigned the same channel = %d; Skip channel switch \n`, iface_num, chosen_random_channel);
        res = 0;
    } else if (chosen_random_channel > 0) {
        ulog_info(`[wlan%d] RCS done ... random channel found = %d\n`, iface_num, chosen_random_channel);
        res = 1;
    } else {
        ulog_info(`[wlan%d] RCS scan FAIL. Retry Channel optimization at next cycle \n`, iface_num);
        res = 0;
    }

    return res;
}

function channel_optimize() {
    let selected_algo;

    switch (config.algo) {
        case 1:
            selected_algo = "RCS";
            break;
        case 2:
            selected_algo = "ACS";
            break;
        default:
            ulog_info(`No RRM algorithm based on Channel Utilization selected \n`);
            return config.interval;
    }

    ulog_info(`Selected algorithm for RRM = %s \n`, selected_algo);
    if (config.threshold == 0) {
        ulog_info(`RRM config threshold is 0, RRM channel optimization will not be executed \n`);
        return config.interval;
    }

    let current_rf_down = {};
	let cool_down_f = {};
    let check_all_cool_down = 0;
    let cool_down_period = config.interval * 10;

    let htmode = {};
    let radio_band = {};
    let acs_exclude_dfs = {};
    let channel_config = {};
    let selected_channels = {};
    let radio_5G_index = 0;
    let dfs_enabled_5g = {};
    let bssid_mac = {};

    // check the channel config used by the customer
    let fixed_chan_bkp = {};
    let fixed_channel_f = {};
    let current_channel = {};
    let auto_channel_f ={};

    // sleep time between channel stats check (default: 5 sec)
    let sleep_time = 5000;

    // check chan util
    let chan_util_value = {};
    let check_all_chan_util = 0;

    // check threshold breach
    let threshold_breach_count = {};
    let threshold_breach_f = {};
    let check_all_threshold_breach = 0;
    let check_threshold_breach_idx = {};

    ulog_info(`Interval for checking Channel Utilization = %d seconds; Interval for cooling down = %d seconds \n`, config.interval/1000, cool_down_period/1000);

    for (let j = 0; j < num_radios; j++) {
        let radio_id = "radio" + j;

        // check wlan interface status
        current_rf_down[j] = interface_status_check(j);
        if (current_rf_down[j] == 0) {
            cool_down_f[j] = cool_down_check(j, cool_down_period);

            if (cool_down_f[j] != 1) {
                // default HT mode = HT20
                htmode[j] = 'HT20';

                // get wireless interface uci config from "ubus call network.wireless status"
                let wireless_status = global.ubus.conn.call('network.wireless', 'status');
                htmode[j] = wireless_status[radio_id].config.htmode;
                radio_band[j] = wireless_status[radio_id].config.band;
                acs_exclude_dfs[j] = wireless_status[radio_id].config.acs_exclude_dfs;
                channel_config[j] = wireless_status[radio_id].config.channel;
                selected_channels[j] = wireless_status[radio_id].config.channels;

                if (radio_band[j] == '5g') {
                    radio_5G_index = j;

                    // check if DFS is enabled for 5G radio
                    if (acs_exclude_dfs[j] == false) {
                        dfs_enabled_5g[j] = 1;
                    }
                }

                if (selected_algo == "RCS") {
                    if (channel_config[j] == '0') {
                        ulog_info(`[wlan%d] Configured channel is "auto" \n`, j);
                    } else {
                        ulog_info(`[wlan%d] Configured channel is fixed at %d \n`, j, channel_config[j]);
                    }

                    if (selected_channels[j]) {
                        ulog_info(`[wlan%d] Selected channel list (please update the radio config, if not correct) = %s \n`, j, selected_channels[j]);
                        // should I check the validity of the chan list selected by the user??
                    }
                } else if (selected_algo == "ACS") {
                    if (channel_config[j] != '0') {
                        ulog_info(`[wlan%d] Configured channel is fixed at %d \n`, j, channel_config[j]);
                        auto_channel_f[j] = 0;

                        stats_info_write("/tmp/fixed_channel_phy" + j, channel_config[j]);
                        fixed_channel_f[j] = 1;
                    } else {
                        ulog_info(`[wlan%d] Configured channel is "auto" \n`, j);
                        auto_channel_f[j] = 1;

                        // check if fixed channel exists
                        fixed_chan_bkp[j] = stats_info_read("/tmp/fixed_channel_phy" + j);
                        if (fixed_chan_bkp[j] > 0) {
                            ulog_info(`[wlan%d] Configured fixed channel was %d \n`, j, fixed_chan_bkp[j]);
                            fixed_channel_f[j] = 1;
                        }
                    }
                }

                // check current channel
                current_channel[j] = check_current_channel(j);

                // check current breach_count
                let current_threshold_breach_count = 0;
                current_threshold_breach_count = stats_info_read("/tmp/threshold_breach_count_phy" + j);
                ulog_info(`[wlan%d] Allowed consecutive Channel Utilization threshold breach count = %d \n`, j, config.consecutive_threshold_breach);

                if (!current_threshold_breach_count || current_threshold_breach_count == null || current_threshold_breach_count == 'NaN') {
                    // /tmp/phyX_breachcount file doesn't exist yet or has invalid value
                    current_threshold_breach_count = 0;
                }
                ulog_info(`[wlan%d] Previous consecutive Channel Utilization threshold breach count = %d \n`, j, current_threshold_breach_count);

                // channel util at this channel (auto/fixed)
                chan_util_value[j] = get_chan_util(j, sleep_time);
                ulog_info(`[wlan%d] Allowed Channel Utilization threshold = %d \n`, j, config.threshold, current_channel[j]);
                ulog_info(`[wlan%d] Current Channel Utilization (Channel %d at %d) = %d \n`, j, current_channel[j], time(), chan_util_value[j]);

                if (chan_util_value[j] >= config.threshold) {
                    check_all_chan_util++;

                    // Channel Utilization threshold exceeded, increase breach count
                    current_threshold_breach_count++;
                    threshold_breach_count[j] = current_threshold_breach_count;
                    ulog_info(`[wlan%d] New consecutive Channel Utilization threshold breach count = %d \n`, j, threshold_breach_count[j]);

                    if (threshold_breach_count[j] >= config.consecutive_threshold_breach) {
                        // threshold breach flag up!
                        threshold_breach_f[j] = 1;
                        // get index of iface which exceeded the threshold breach count
                        check_threshold_breach_idx[j] = j;
                        check_all_threshold_breach++;
                    }
                } else {
                    // reset the threshold breach count
                    ulog_info(`[wlan%d] Current Channel Utilization (%d) < Allowed Channel Utilization threshold (%d) \n`, j, chan_util_value[j], config.threshold);
                    ulog_info(`[wlan%d] Reset consecutive Channel Utilization threshold breach count \n`, j);

                    threshold_breach_count[j] = 0;
                    // threshold breach flag down!
                    threshold_breach_f[j] = 0;
                }

                update_breach_count(j, threshold_breach_count[j]);
            } else {
                check_all_cool_down++;
            }
        } else {
            ulog_info(`[wlan%d] Interface not UP, will be checked in the next interval \n`, j);
        }
    }

    for (let l = 0; l < num_radios; l++) {
        if (current_rf_down[l] == 0) {
            cool_down_f[l] = cool_down_check(l, cool_down_period);

            if (cool_down_f[l] != 1) {
                // start algo only if threshold breach count, and chan util threshold exceeded from configured values
                if (threshold_breach_f[l] == 1 && chan_util_value[l] >= config.threshold) {
                    ulog_info(`[wlan%d] Consecutive Channel Utilization threshold breached = %d; %s Algorithm STARTS \n`, l, threshold_breach_count[l], selected_algo);

                    if (selected_algo == "RCS") {
                        // no. of channel utils to be compared
                        let max_chan = 2;
                        let curr_chan_list = {};
                        let chan_util_list = {};
                        let test_payload = {};
                        let final_payload = {};
                        let long_cac_time = 0;

                        bssid_mac[l] = stats_info_read("/sys/class/ieee80211/phy" + l + "/addresses");

                        sleep_time = 3000;

                        ulog_info(`[wlan%d] Total of %d channel utils will be compared \n `, l, max_chan);

                        /* Collect the chan util info of #max_chan channels*/
                        for (let num_chan = 0; num_chan < max_chan; num_chan++) {
                            ulog_info(`[wlan%d] Channel utilization check ROUND#%d \n`, l, num_chan);

                            if (num_chan == 0) {
                                curr_chan_list[num_chan] = current_channel[l];
                                chan_util_list[num_chan] = chan_util_value[l];

                                ulog_info(`[wlan%d] Current channel %d has Channel utilization = %d \n`, l, curr_chan_list[num_chan], chan_util_list[num_chan]);
                            } else {
                                // flag to assign max chan util value
                                let assign_max_chan_util = 0;

                                // call RCS for multiple random chan
                                let chan_scan = algo_rcs(l, curr_chan_list[num_chan-1], radio_band[l], htmode[l], selected_channels[l]);
                                curr_chan_list[num_chan] = stats_info_read("/tmp/phy" + l + "_rrmchan");

                                if (chan_scan == 1) {
                                    // assign channel from RCS to interface
                                    test_payload = {
                                        action: 'chan_switch',
                                        bssid: bssid_mac[l],
                                        channel: curr_chan_list[num_chan],
                                        phy: l,
                                        band: radio_band[l],
                                        htmode: htmode[l],
                                    };

                                    if (l == radio_5G_index) {
                                        dfs_enabled_5g[radio_5G_index] = dfs_chan_check(radio_5G_index, test_payload.channel);
                                    }

                                    ulog_info(`[wlan%d] Initiated channel switch to random channel %d for comparing Channel utilization \n`, l, test_payload.channel);
                                    let test_switch_status = rrmd_switch_channel(test_payload);

                                    if (test_switch_status != 0) {
                                        let actual_channel = switch_status_check(l, radio_5G_index, dfs_enabled_5g[radio_5G_index]);

                                        if (actual_channel == test_payload.channel) {
                                            ulog_info(`[wlan%d] Channel Switch success; Checking Channel utilization ... \n`, l);
                                            // get chan util for current assigned random channel
                                            chan_util_list[num_chan] = get_chan_util(l, sleep_time);
                                        } else {
                                            if (dfs_enabled_5g[radio_5G_index] == 1 && interface_status_check(l) == 1) {
                                                // dfs channel not up yet
                                                ulog_info(`[wlan%d] DFS channel %d taking too long to be UP. Interface status/Channel utilization will be checked in the next interval\n`, l, test_payload.channel);
                                                // jump back to original channel
                                                long_cac_time = 1;
                                                break;
                                            }
                                            assign_max_chan_util = 1;
                                        }
                                    } else {
                                        assign_max_chan_util = 1;
                                    }
                                } else {
                                    // RCS failed
                                    assign_max_chan_util = 1;
                                }

                                if (assign_max_chan_util == 1) {
                                    ulog_info(`[wlan%d] Channel switch fail; assign Channel utilization = 100 \n`, l);
                                    // assign highest util value for invalid channel
                                    chan_util_list[num_chan] = 100;
                                }

                                ulog_info(`[wlan%d] Channel utilization of random channel#%d (%s) = %d \n`, l, num_chan, curr_chan_list[num_chan], chan_util_list[num_chan] );
                            }
                        }

                        /* Switch to the channel with the lowest chan util; if long_cac_time flag is up then switch back to the previous channel */
                        if (long_cac_time != 1) {
                            ulog_info(`[wlan%d] Channel utilization of all %d channels checked \n`, l, max_chan);

                            // find the minimum chan util and select that as the next channel
                            let min_util = chan_util_list[0];
                            let index_min_util = 0;
                            for (let x = 0; x < max_chan; x++) {
                                ulog_info(`[wlan%d] Channel#%d = %s; Channel utilization#%d = %d \n`, l, x, curr_chan_list[x], x, chan_util_list[x]);

                                if (chan_util_list[x] < min_util) {
                                    min_util = chan_util_list[x];
                                    index_min_util = x;
                                }
                            }

                            ulog_info(`[wlan%d] Channel %d has the least Channel utilization of %d; switching to this channel \n`, l, curr_chan_list[index_min_util], min_util );

                            let _current_channel = check_current_channel(l);

                            if (_current_channel != curr_chan_list[index_min_util]) {
                                // switch channel to min_util
                                final_payload = {
                                    action: 'chan_switch',
                                    bssid: bssid_mac[l],
                                    channel: curr_chan_list[index_min_util],
                                    phy: l,
                                    band: radio_band[l],
                                    htmode: htmode[l],
                                };

                                if (l == radio_5G_index) {
                                    dfs_enabled_5g[radio_5G_index] = dfs_chan_check(radio_5G_index, final_payload.channel);
                                }

                                if (final_payload.channel != curr_chan_list[max_chan-1] || min_util != chan_util_list[max_chan-1]) {
                                    ulog_info(`[wlan%d] Initiated final channel switch to Channel %d \n`, l, final_payload.channel);
                                    let final_switch_status = rrmd_switch_channel(final_payload);

                                    if (final_switch_status != 0) {
                                        let final_channel = switch_status_check(l, radio_5G_index, dfs_enabled_5g[radio_5G_index]);

                                        if (final_channel == final_payload.channel) {
                                            ulog_info(`[wlan%d] Final channel switch success \n`, l);
                                        } else {
                                            ulog_info(`[wlan%d] RCS algo fail (final channel switch failure), wait until next interval to retry\n`, l);
                                        }
                                    } else {
                                        ulog_info(`[wlan%d] RCS algo fail (final channel switch failure at hostapd_cli), wait until next interval to retry\n`, l);
                                    }
                                } else {
                                    ulog_info(`[wlan%d] Channel switch not necessary, current channel %d has the least channel utilization value \n`, l, final_payload.channel);
                                    // reset breach count
                                    update_breach_count(l, 0);
                                }
                            } else {
                                ulog_info(`[wlan%d] Channel switch not necessary, current channel %d is already assigned to the interface \n`, l, _current_channel);
                                // reset breach count
                                update_breach_count(l, 0);
                            }
                        } else {
                            // revert back to the original channel
                            ulog_info(`[wlan%d] Channel %d has a cac_time longer than 60 seconds, RRM failed for this interval (you might want to avoid selecting this channel) \n`, l, test_payload.channel);
                        }
                    } else if (selected_algo == "ACS") {
                        let random_wait_time = random_time_calc();

                        /*
                            check_all_chan_util == num_radios: chan util threshold exceeded for all interfaces
                            check_all_chan_util >= 1: chan util threshold exceeded for one or more interfaces

                            check_all_cool_down == 0: cool down time for all interfaces is over
                            check_all_cool_down < num_radios: cool down time has passed for one or more interfaces

                            check_all_threshold_breach == num_radios: threshold breach count exceeded for all interfaces
                            check_all_threshold_breach >= 1: threshold breach count exceeded for one or more interfaces
                        */

                        // flag to check if 5G radio was restarted
                        let radio_5g_restarted = 0;

                        if (check_all_chan_util == num_radios && check_all_cool_down == 0 && check_all_threshold_breach == num_radios) {
                            // Channel util high for all interfaces && cool down period over && threshold breach count exceeded for all interfaces: restart all interfaces

                            for (let m = 0; m < num_radios; m++) {
                                fixed_channel_algo(m, fixed_channel_f[m], auto_channel_f[m], fixed_chan_bkp[m], channel_config[m]);
                            }

                            ulog_info(`[all wlan interfaces] Initiating channel switch in %d seconds ... \n`, random_wait_time);
                            sleep(random_wait_time*1000);
                            ulog_info(`[all wlan interfaces] %s Algorithm will start; Turning DOWN/UP \n`, selected_algo);

                            // 5G radio was restarted
                            radio_5g_restarted = 1;

                            for (let x = 0; x < num_radios; x++) {
                                ulog_info(`[wlan%d] Channel will be switched \n`, x);
                                // timestamp for all interfaces must be saved
                                update_channel_switch_time(x);

                                // reset breach count back to 0 as we are calling the channel selection algo
                                update_breach_count(x, 0);
                            }

                            // restart all wlan interfaces
                            system(`wifi down`);
                            system(`wifi up`);
                        } else if (check_all_chan_util >= 1 && check_all_cool_down < num_radios && check_all_threshold_breach >= 1) {
                            // Channel util high for one or more interfaces && cool down period over for one or more interfaces && threshold breach count exceeded for one or more interface: restart that interface
                            for (let high_util_iface in check_threshold_breach_idx) {
                                fixed_channel_algo(high_util_iface, fixed_channel_f[high_util_iface], auto_channel_f[high_util_iface], fixed_chan_bkp[high_util_iface], channel_config[high_util_iface]);

                                ulog_info(`[wlan%d] Initiating channel switch in %d seconds ... \n`, high_util_iface, random_wait_time);
                                sleep(random_wait_time*1000);
                                ulog_info(`[wlan%d] %s Algorithm will start; Turning DOWN/UP \n`, high_util_iface, selected_algo);

                                ulog_info(`[wlan%d] Channel will be switched \n`, high_util_iface);
                                // timestamp for wlanX interfaces must be saved
                                update_channel_switch_time(high_util_iface);

                                // reset breach count back to 0 as we are calling the channel selection algo
                                update_breach_count(high_util_iface, 0);

                                if (high_util_iface == radio_5G_index) {
                                    radio_5g_restarted = 1;
                                }

                                // wifi down radioX
                                system(`wifi down radio${high_util_iface}`);
                                // wifi up radioX
                                system(`wifi up radio${high_util_iface}`);
                            }
                        }

                        // need to wait for radio 5GHz interface, when it is DFS enabled && restarted
                        if (radio_5g_restarted == 1 && dfs_enabled_5g[radio_5G_index] == 1) {
                            ulog_info(`[wlan%d] 5G radio might need some time to be UP (DFS enabled) ... wait for 30 seconds \n`, radio_5G_index);
                            // 30 sec delay for DFS scan to come finish
                            sleep(30000);
                        }
                    }
                } else {
                    if (threshold_breach_f[l] != 1) {
                        ulog_info(`[wlan%d] Threshold breach count (=%d) < Allowed consecutive Channel Utilization threshold breach count (=%d), will be checked again in the next interval \n`, l, threshold_breach_count[l], config.consecutive_threshold_breach);
                    } else {
                        ulog_info(`[wlan%d] Channel utilization (=%d) within threshold (=%d), will be checked again in the next interval \n`, l, chan_util_value[l], config.threshold);
                    }
                }
            } else {
                ulog_info(`[wlan%d] Need to cool down (%d seconds hasn't passed); will be checked again in the next interval \n`, l, cool_down_period/1000);
            }
        }
    }
    ulog_info(`Interference detection finish; next RRM round starts in %d seconds \n`, config.interval/1000);

    return config.interval;
}

return {
    init: function(data) {
        ulog_info('Policy - Channel utilization \n');
        /* load config and override defaults if they were set in UCI */
        for (let key in config)
            if (data[key])
                config[key] = +data[key];

        uloop_timeout(channel_optimize, 20000);
    },

};