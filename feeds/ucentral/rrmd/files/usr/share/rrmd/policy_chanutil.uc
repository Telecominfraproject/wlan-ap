let board_info = global.ubus.conn.call('system', 'board');
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

function stats_info_read(path) {
    let res = trim(fs.readfile(path));
    return res || 0;
}

function stats_info_write(path, value) {
	let file = fs.open(path, 'w');
	if (!file)
		return;
	file.write(value);
	file.close();
}

function record_rrm_timestamp() {
    stats_info_write("/tmp/rrm_timestamp", time());
}

// total number of radios: default=2
let num_radios = 2;
let phy_count;

let board_name = board_info.board_name;
switch(board_name) {
case 'edgecore,eap105':
case 'edgecore,oap101-6e':
case 'edgecore,oap101e-6e':
case 'zyxel,nwa130be':
    num_radios = 3;
    break;
case 'edgecore,eap112':
    phy_count = stats_info_read("/tmp/phy_count");
    if (phy_count)
        num_radios = int(phy_count);
    break;
}

function cool_down_check(iface, cool_down_period) {
    let now_t = time();
    let cool_down_f= 0;

    // check cool_down_period passed or not
    let deltaTS = now_t - stats_info_read("/tmp/chan_switch_time_" + iface);
    ulog_info(`[%s] Cool down check: current_time=%d, chan_switch_time=%d, deltaTS(time passed)=%d \n`, iface, now_t, stats_info_read("/tmp/chan_switch_time_" + iface), deltaTS);

    if (deltaTS < (cool_down_period/1000)) {
        ulog_info(`[%s] Need to cool down (%d seconds hasn't passed) before switching channel \n`, iface, cool_down_period/1000);
        // Flag cool down
        cool_down_f = 1;
    }

    return cool_down_f;
}

function update_channel_switch_time(iface) {
    stats_info_write("/tmp/chan_switch_time_" + iface, time());
}

function update_breach_count(iface, breach_count) {
    stats_info_write("/tmp/threshold_breach_count_" + iface , breach_count);
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
    let freq = 0;

    switch (band) {
        case '2g':
            if (channel == 14)
                freq = 2484;
            else
                freq = 2407 + channel * 5;
            break;
        case '5g':
            if (channel >= 7 && channel <= 177)
                freq = 5000 + channel * 5;
            else if (channel >= 183 && channel <= 196)
                freq = 4000 + channel * 5;
            break;
        case '6g':
            if (channel == 2)
                freq = 5935;
            else
                freq = 5955 + (channel - 1) * 5;
            break;
        case '60g':
            if (channel >= 1 && channel <= 6)
                freq = 56160 + channel * 2160;
            break;
        default:
            break;
    }

    return freq;
}

// using mapping to get correct center channel, especially for 6G radio
function get_center_channel(channel, band, bw) {
    let center_channel = channel;
    let center_channel_map = {};

    switch (band) {
    case '5g':
        if (bw == 40) {
            center_channel_map = {
                "36": 38, "40": 38,
                "44": 46, "48": 46,
                "52": 54, "56": 54,
                "60": 62, "64": 62,
                "100": 102, "104": 102,
                "108": 110, "112": 110,
                "116": 118, "120": 118,
                "124": 126, "128": 126,
                "132": 134, "136": 134,
                "140": 142, "144": 142,
                "149": 151, "153": 151,
                "157": 159, "161": 159,
                "165": 167
            };
        } else if (bw == 80) {
            center_channel_map = {
                "36": 42, "40": 42, "44": 42, "48": 42,
                "52": 58, "56": 58, "60": 58, "64": 58,
                "100": 106, "104": 106, "108": 106, "112": 106,
                "116": 122, "120": 122, "124": 122, "128": 122,
                "132": 138, "136": 138, "140": 138, "144": 138,
                "149": 155, "153": 155, "157": 155, "161": 155,
                "165": 171
            };
        } else if (bw == 160) {
            center_channel_map = {
                "36": 50, "40": 50, "44": 50, "48": 50,
                "52": 50, "56": 50, "60": 50, "64": 50,
                "100": 114, "104": 114, "108": 114, "112": 114,
                "116": 114, "120": 114, "124": 114, "128": 114
            };
        }
        break;
    case '6g':
        if (bw == 40) {
            center_channel_map = {
                "1": 3, "5": 3, "9": 11, "13": 11,
                "17": 19, "21": 19, "25": 27, "29": 27,
                "33": 35, "37": 35, "41": 43, "45": 43,
                "49": 51, "53": 51, "57": 59, "61": 59,
                "65": 67, "69": 67, "73": 75, "77": 75,
                "81": 83, "85": 83, "89": 91, "93": 91,
                "97": 99, "101": 99, "105": 107, "109": 107,
                "113": 115, "117": 115, "121": 123, "125": 123,
                "129": 131, "133": 131, "137": 139, "141": 139,
                "145": 147, "149": 147, "153": 155, "157": 155,
                "161": 163, "165": 163, "169": 171, "173": 171,
                "177": 179, "181": 179, "185": 187, "189": 187,
                "193": 195, "197": 195, "201": 203, "205": 203,
                "209": 211, "213": 211, "217": 219, "221": 219,
                "225": 227
            };
        } else if (bw == 80) {
            center_channel_map = {
                "1": 7, "5": 7, "9": 7, "13": 7,
                "17": 23, "21": 23, "25": 23, "29": 23,
                "33": 39, "37": 39, "41": 39, "45": 39,
                "49": 55, "53": 55, "57": 55, "61": 55,
                "65": 71, "69": 71, "73": 71, "77": 71,
                "81": 87, "85": 87, "89": 87, "93": 87,
                "97": 103, "101": 103, "105": 103, "109": 103,
                "113": 119, "117": 119, "121": 119, "125": 119,
                "129": 135, "133": 135, "137": 135, "141": 135,
                "145": 151, "149": 151, "153": 151, "157": 151,
                "161": 167, "165": 167, "169": 167, "173": 167,
                "177": 183, "181": 183, "185": 183, "189": 183,
                "193": 199, "197": 199, "201": 199, "205": 199,
                "209": 215
            };
        } else if (bw == 160) {
            center_channel_map = {
                "1": 15, "5": 15, "9": 15, "13": 15,
                "17": 15, "21": 15, "25": 15, "29": 15,
                "33": 47, "37": 47, "41": 47, "45": 47,
                "49": 47, "53": 47, "57": 47, "61": 47,
                "65": 79, "69": 79, "73": 79, "77": 79,
                "81": 79, "85": 79, "89": 79, "93": 79,
                "97": 111, "101": 111, "105": 111, "109": 111,
                "113": 111, "117": 111, "121": 111, "125": 111,
                "129": 143, "133": 143, "137": 143, "141": 143,
                "145": 143, "149": 143, "153": 143, "157": 143,
                "161": 175, "165": 175, "169": 175, "173": 175,
                "177": 175, "181": 175, "185": 175, "189": 175,
                "193": 207
            };
        } else if (bw == 320) {
            center_channel_map = {
                "1": 31, "5": 31, "9": 31, "13": 31,
                "17": 31, "21": 31, "25": 31, "29": 31,
                "33": 63, "37": 63, "41": 63, "45": 63,
                "49": 63, "53": 63, "57": 63, "61": 63,
                "65": 63, "69": 63, "73": 63, "77": 63,
                "81": 63, "85": 63, "89": 63, "93": 63,
                "97": 127, "101": 127, "105": 127, "109": 127,
                "113": 127, "117": 127, "121": 127, "125": 127,
                "129": 127, "133": 127, "137": 127, "141": 127,
                "145": 127, "149": 127, "153": 127, "157": 127,
                "161": 191
            };
        }
        break;
    }

    if (center_channel_map[channel])
        center_channel = center_channel_map[channel];

    return center_channel;
}

function interface_status_check(iface) {
    ulog_info(`[%s] Checking interface status ... \n`, iface);
    let radio_status = 'DISABLED';
    let radio_down_f = 1;

    if (board_info.board_name == 'edgecore,eap112' && phy_count == 3) {
        //  hostapd_cli_s1g status | grep 'Selected interface'| awk -F "\'" '{print $2}'
        let check_HaLow_iface_cmd = sprintf('hostapd_cli_s1g status | grep \'Selected interface\'| awk -F "\'" \'{print $2}\'');
        let check_HaLow_iface = fs.popen(check_HaLow_iface_cmd);
        let _check_HaLow_iface = trim(check_HaLow_iface.read('all'));
        check_HaLow_iface.close();

        if (_check_HaLow_iface && _check_HaLow_iface == iface) {
            ulog_info(`[%s] This is a HaLow interface \n`, _check_HaLow_iface);

            // this iface is HaLow interface and we can neither check channel utilization nor switch channel, we can check if it is UP
            radio_down_f = 2;
            ulog_info(`[%s] status: ENABLED \n`, iface, radio_status);
            return radio_down_f;
        }
    }

    let curr_stat = global.ubus.conn.call(`hostapd.${iface}`, 'get_status');
    if (curr_stat) {
        radio_status = curr_stat.status;
        radio_down_f = 0;
    }
    ulog_info(`[%s] status: %s \n`, iface, radio_status);

    return radio_down_f;
}

function check_current_channel(iface) {
    let current_channel;
    // get wireless interface's live status & channel using "ubus call hostapd.<iface> get_status"
    let curr_stat = global.ubus.conn.call(`hostapd.${iface}`, 'get_status');
    if (curr_stat)
        current_channel = curr_stat.channel;

    if (curr_stat && current_channel) {
        ulog_info(`[%s] Current channel (from hostapd) = %d \n`, iface, current_channel);
    }

    return current_channel;
}

function hostapd_switch_channel(msg) {
    ulog_info(`[%s] Start switch channel to %d \n`, msg.iface, msg.channel);

    // Channel switch in progress, set flag = 1
    stats_info_write("/tmp/rrm_chan_switch", 1);

    let chan_switch_status = 0;
    let sec_channel_offset = null;

    let mode = lc(replace(msg.htmode, /[^a-zA-Z]/g, ''));
    let bandwidth = replace(msg.htmode, /[^0-9]/g, '');

    let target_freq = channel_to_freq(msg.band, msg.channel);
    let center_channel = get_center_channel(msg.channel, msg.band, bandwidth);
    let center_freq = channel_to_freq(msg.band, center_channel);
    if (bandwidth > 20)
        sec_channel_offset = 1;

    // use hostapd_cli command
    if (target_freq != null) {
        ulog_info(`Sending to hostapd (Chan %d):: freq=%d, center_freq=%d, sec_channel_offset=%d, bandwidth=%d, mode=%s \n`, msg.channel, target_freq, center_freq, sec_channel_offset, bandwidth, mode);

        let command_hostapd = `/usr/sbin/hostapd_cli -i ${msg.iface} chan_switch 5 ${target_freq} center_freq1=${center_freq} sec_channel_offset=${sec_channel_offset} bandwidth=${bandwidth} ${mode}`;
        let res = fs.popen(command_hostapd);
        let res_check = trim(res.read('all'));
        res.close();

        if (res_check == "OK") {
            ulog_info(`hostapd_cli chan_switch status: OK \n`);
            chan_switch_status = 1;
            update_channel_switch_time(msg.iface);

            // reset breach count back to 0 as we are calling the channel selection algo
            update_breach_count(msg.iface, 0);
        } else {
            ulog_info(`hostapd_cli chan_switch status: FAIL \n`);
        }
    }
    else {
        ulog_info(`Channel to frequency conversion fail \n`);
    }

    return chan_switch_status;
}

function switch_status_check(iface, dfs_enabled_5g_flag) {
    // need to wait for radio 5GHz interface to be UP, when DFS is enabled
    if (dfs_enabled_5g_flag == 1) {
        ulog_info(`[%s] 5G radio might need some time to be UP (DFS enabled) \n`, iface);

        let p = 0;
        // Default max 70 seconds wait for the DFS enabled interface to be UP
        let timer = 70;

        // get real timer from hostapd_cli command
        let check_cac_time = sprintf('hostapd_cli -i %s status | grep \"cac_time_left_seconds\" | awk -F "=" \'{print $2}\'', iface);
        let _cac_time = fs.popen(check_cac_time);
        let cac_time = trim(_cac_time.read('all'));
        _cac_time.close();

        // if cac_time is a valid number, set timer to cac_time + 10 seconds
        if (cac_time > 0 && match(cac_time, /^[0-9]+$/)) {
            timer = int(cac_time) + 10;
        }

        while (p < timer) {
            ulog_info(`[%s] Check#%d \n `, iface, p);

            if (interface_status_check(iface) == 1) {
                ulog_info(`[%s] Interface not UP yet ... wait for 1 second \n`, iface);
                sleep(1000);
                p++;

                if (p >= timer) {
                    ulog_info(`[%s] Interface not UP yet ... cac_time is long \n`, iface);
                    return;
                }
            } else {
                ulog_info(`[%s] Interface is UP! \n`, iface);
                break;
            }
        }
    }

    // Channel switch done, set flag = 0
    stats_info_write("/tmp/rrm_chan_switch", 0);

    let current_chan = check_current_channel(iface);
    return current_chan;
}

function dfs_chan_check(iface, rcs_channel) {
    let iface_num = replace(iface, /[^0-9]/g, '');
    let phy_id = 'phy' + iface_num;
    if (board_name == 'edgecore,eap105') {
        phy_id = 'phy00';
    }
    let dfs_enabled_5g_f = 0;
    let dfs_chan_list = global.phy.phys[phy_id].dfs_channels;

    // check if rcs_channel is in dfs_channel list
    for (let dfs_chan in dfs_chan_list) {
        if (dfs_chan == rcs_channel) {
            // flag up if dfs channel detected
            dfs_enabled_5g_f = 1;
            break;
        }
    }

    return dfs_enabled_5g_f;
}

function fixed_channel_config(iface, iface_num, fixed_channel_f, auto_channel_f, fixed_chan_bkp, channel_config) {
    // if fixed channel config is stored in the /tmp/fixed_channel_<radio_iface> file
    if (fixed_channel_f == 1) {
        if (auto_channel_f == 1) {
            // if current channel is auto => change to fixed
            ulog_info(`[%s] Current channel is "auto"; Configured fixed channel was %d, reassigning fixed channel ...\n`, iface, fixed_chan_bkp);

            // Set channel to the fixed channel
            global.uci.set('wireless', 'radio' + iface_num, 'channel', fixed_chan_bkp);
            global.uci.commit('wireless');
            global.uci.apply;
        } else if (auto_channel_f == 0) {
            // if current channel is not auto => change to auto
            ulog_info(`[%s] Current channel is fixed (=%d), changing it to "auto" (=0) to trigger ACS \n`, iface, channel_config);

            // Set channel to "auto"
            global.uci.set('wireless', 'radio' + iface_num, 'channel', '0');
            global.uci.commit('wireless');
            global.uci.apply;
        }
    }
}

function get_chan_util(radio_band, sleep_time) {
	let pdev_stats = {};
	let chan_util = 0;
    let total_usage = 0;

    let prev_values = {
		txFrameCount: null,
		rxFrameCount: null,
		rxClearCount: null,
        chanBusyTime: null,
		cycleCount: null,
        chanActiveTime: null,
	};

    for (let c = 0; c < 2; c++) {
        // Check tx and tx stats for wlanX interface
        let pdev_stats_file = '/tmp/pdev_stats_phy' + radio_band;

        let curr_values = {
            txFrameCount: null,
            rxFrameCount: null,
            rxClearCount: null,
            chanBusyTime: null,
            cycleCount: null,
            chanActiveTime: null,
        };

        if (board_info.board_name == 'edgecore,eap111' || board_info.board_name == 'edgecore,eap112') {
            // for EAP111 and EAP112 (only 2.4G and 5G radio bands)
            system(`cat /tmp/sr_scene_cond_phy${radio_band}`);
            // logread -e "Congestion Ratio" | tail -n 1  | awk -F'= ' '{print $2}' | tr -d '%'
            let cmd = sprintf('logread -e \"Congestion Ratio\" | tail -n 1  | awk -F\'= \' \'{print $2}\' | tr -d \'\%\'');
            let chan_util_cmd = fs.popen(cmd);
            let _chan_util = chan_util_cmd.read('all');
            chan_util_cmd.close();

            chan_util = int(_chan_util);
            break;
        } else {
            pdev_stats = split(stats_info_read(pdev_stats_file), "\n");

            if (pdev_stats != null) {
                for (let curr_value in pdev_stats) {
                    let txFrameCount = match(trim(curr_value), /^TX frame count(\s+\d+)/);
                    if (txFrameCount)
                        curr_values.txFrameCount = int(trim(txFrameCount[1]));

                    let rxFrameCount = match(trim(curr_value), /^RX frame count(\s+\d+)/);
                    if (rxFrameCount)
                        curr_values.rxFrameCount = int(trim(rxFrameCount[1]));

                    let rxClearCount = match(trim(curr_value), /^RX clear count(\s+\d+)/);
                    if (rxClearCount)
                        curr_values.rxClearCount = int(trim(rxClearCount[1]));

                    let cycleCount = match(trim(curr_value), /^Cycle count(\s+\d+)/);
                    if (cycleCount)
                        curr_values.cycleCount = int(trim(cycleCount[1]));

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
                    let rx_clear_delta = curr_values.rxClearCount - prev_values.rxClearCount;
                    if (cycle_count_delta && cycle_count_delta > 0)
                        total_usage = (rx_clear_delta * 100) / cycle_count_delta;
                    chan_util = total_usage;
                }

                prev_values.txFrameCount=curr_values.txFrameCount;
                prev_values.rxFrameCount=curr_values.rxFrameCount;
                prev_values.rxClearCount=curr_values.rxClearCount;
                prev_values.cycleCount=curr_values.cycleCount;
            }
        }
        sleep(sleep_time);
    }

    // record channel utilization
    stats_info_write("/tmp/chanutil_phy" + radio_band, chan_util);

    return chan_util;
}

function random_channel_selection(iface, band, htmode, chan_list_valid, exclude_dfs) {
    let math = require('math');
    let bw = replace(htmode, /[^0-9]/g, '');
    let iface_num = replace(iface, /[^0-9]/g, '');
    let phy_id = 'phy' + iface_num;
    if (board_name == 'edgecore,eap105') {
        phy_id = 'phy00';
    }

    // channel list from the driver based on the country code
    let chan_list_cc = uniq(sort(global.phy.phys[phy_id].channels, (a, b) => a - b));
    // DFS channel list from the driver
    let dfs_chan_list = global.phy.phys[phy_id].dfs_channels || [];
    // complete channel list
    let chan_list_default = {};
    // allowed channel list to select random channel from
    let chan_list_allowed = [];

    let chan_list_init = [];
    let chan_list_legal = [];
    let _chan_list_legal = [];

    ulog_info(`[%s] Channel list from the driver = %s \n`, iface, chan_list_cc);
    ulog_info(`[%s] Selected channel list from config (default channel list shall be used in case channels haven't been selected) = %s \n`, iface, (chan_list_valid || '[]'));

    if (band == '2g' && bw >= 40) {
        ulog_info(`[%s] It is highly recommended to NOT use %dMHz bandwidth for 2.4G radio (RRM will not work properly) \n`, iface, bw);
    } else if (band == '5g' && bw > 160) {
        ulog_info(`[%s] %dMHz bandwidth not supported for 5G radio. Please use a bandwidth of 160MHz or lower\n`, iface, bw);
    }

    // default channel list
    if (band == '6g') {
        chan_list_default = {
            "320": [ 33, 97, 161 ],
            "160": [ 1, 33, 65, 97, 129, 161, 193 ],
            "80": [
                1, 17,
                33, 49,
                65, 81,
                97, 113,
                129, 145,
                161, 177,
                193, 209
            ],
            "40": [
                1, 9, 17, 25,
                33, 41, 49, 57,
                65, 73, 81, 89,
                97, 105, 113, 121,
                129, 137,
                145, 153,
                161, 169,
                177, 185,
                193, 201,
                209, 217,
                225
            ],
            "20": [
                2, 1, 5, 9, 13,
                17, 21, 25, 29,
                33, 37, 41, 45,
                49, 53, 57, 61,
                65, 69, 73, 77,
                81, 85, 89, 93,
                97, 101, 105, 109,
                113, 117, 121, 125,
                129, 133, 137, 141,
                145, 149, 153, 157,
                161, 165, 169, 173,
                177, 181, 185, 189,
                193, 197, 201, 205,
                209, 213, 217, 221,
                225, 229, 233
            ]
        };
    } else if (band == '5g') {
        chan_list_default = {
            "160": [ 36, 100 ],
            "80": [
                36,
                52,
                100,
                132,
                149,
                165
            ],
            "40": [
                36, 44,
                52, 60,
                100, 108,
                132, 140,
                149, 157,
                165
            ],
            "20": [
                36, 40, 44, 48,
                52, 56, 60, 64,
                100, 104, 108, 112,
                116,
                132, 136, 140, 144,
                149, 153, 157, 161,
                165
            ]
        };
    } else if (band == '2g') {
        chan_list_default = {
            "40": [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ],
            "20": [
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
            ]
        };
    }

    // initial channel list after comparing the default chan list based on current bw, and country code
    for (let cbw, default_chan in chan_list_default) {
        if (cbw == bw) {
            for (let q = 0; q < length(default_chan); q++) {
                for (let cc_chan in chan_list_cc) {
                    if (default_chan[q] == cc_chan) {
                        push(chan_list_init, default_chan[q]);
                    }
                }
            }
        }
    }

    if (band == '5g' && (bw == "80" || bw == "40")) {
        // exclude last channels from the channel list when bw is 80MHz or 40MHz to avoid selecting a channel with a secondary channel that cannot be supported
        _chan_list_legal = slice(chan_list_init, 0, length(chan_list_init)-1) ;
    } else {
        _chan_list_legal = chan_list_init;
    }

    // check if dfs is enabled or disabled for 5G radio; if dfs is disabled, remove dfs channels from chan_list_legal
    if (band == '5g' && exclude_dfs == true) {
        ulog_info(`[%s] DFS Channel list from the driver = %s \n`, iface, dfs_chan_list);

        for (let _legal_chan in _chan_list_legal) {
            let is_dfs_chan = false;
            for (let dfs_chan in dfs_chan_list) {
                if (dfs_chan == _legal_chan) {
                    is_dfs_chan = true;
                    break;
                }
            }
            if (is_dfs_chan == false) {
                push(chan_list_legal, _legal_chan);
            }
        }
    } else {
        chan_list_legal = _chan_list_legal;
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

    ulog_info(`[%s] Allowed channel list = %s \n`, iface, chan_list_allowed);

    // select random channel from chan_list_allowed
    let random_channel_idx = sprintf('%d', math.rand() % length(chan_list_allowed));
    let random_channel = chan_list_allowed[random_channel_idx];
    ulog_info(`[%s] Selected random channel = %d \n`, iface, random_channel);

    return random_channel;
}

function check_center_channel(chosen_random_channel, current_channel, band, htmode) {
    let ret = false;
    let bw = replace(htmode, /[^0-9]/g, '');

    if (band != '2g' || bw != 20) {
        // for 2G band or 20MHz bandwidth, center channel is the same as the channel
        let chosen_random_channel_center = get_center_channel(chosen_random_channel, band, bw);
        let current_channel_center = get_center_channel(current_channel, band, bw);

        ulog_info(`Center channel of the chosen random channel (%d) = %d; Center channel of the current channel (%d) = %d \n`, chosen_random_channel, chosen_random_channel_center, current_channel, current_channel_center);

        if (chosen_random_channel_center == current_channel_center)
            ret = true;
    }

    return ret;
}

function algo_rcs(iface, current_channel, band, htmode, selected_channels, exclude_dfs) {
    let chosen_random_channel = 0;
    let res = 0;
    let same_center_channel = false;

    // random_channel_selection script will help to select random channel
    chosen_random_channel = random_channel_selection(iface, band, htmode, selected_channels, exclude_dfs);
    stats_info_write("/tmp/rrm_random_channel_" + iface, chosen_random_channel);

    if (chosen_random_channel == current_channel) {
        ulog_info(`[%s] RCS assigned the same channel = %d; Skip channel switch \n`, iface, chosen_random_channel);
        res = 0;
    } else if (chosen_random_channel > 0) {
        // check if the random channel has the same center channel as the current channel
        same_center_channel = check_center_channel(chosen_random_channel, current_channel, band, htmode);
        if (same_center_channel) {
            ulog_info(`[%s] RCS found channel %d with the same center channel as current channel %d; Skip channel switch \n`, iface, chosen_random_channel, current_channel);
            res = 0;
        } else {
            ulog_info(`[%s] RCS done ... random channel found = %d\n`, iface, chosen_random_channel);
            res = 1;
        }
    } else {
        ulog_info(`[%s] RCS scan FAIL. Retry Channel optimization at next cycle \n`, iface);
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

    record_rrm_timestamp();

    let current_rf_down = {};
	let cool_down_f = {};
    let check_all_cool_down = 0;
    let cool_down_period = config.interval * 10;

    let radio_iface = {};
    let htmode = {};
    let radio_band = {};
    let radio_disabled = {};
    let acs_exclude_dfs = {};
    let channel_config = {};
    let selected_channels = {};
    let radio_5G_index = null;
    let dfs_enabled_5g_f = {};

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

        // get wireless interface uci config from "ubus call network.wireless status"
        let wireless_status = global.ubus.conn.call('network.wireless', 'status');
        radio_disabled[j] = wireless_status[radio_id].disabled;
        radio_band[j] = wireless_status[radio_id].config.band;

        radio_iface[j] = 'radio ' + radio_band[j];

        if (radio_disabled[j] == false) {
            let interface_created = wireless_status[radio_id].interfaces[0];
            if (interface_created)
                radio_iface[j] = wireless_status[radio_id].interfaces[0].ifname;
        }

        // check wlan interface status
        current_rf_down[j] = interface_status_check(radio_iface[j]);
        if (current_rf_down[j] == 0) {
            cool_down_f[j] = cool_down_check(radio_iface[j], cool_down_period);

            if (cool_down_f[j] != 1) {
                // default HT mode = HT20
                htmode[j] = 'HT20';

                // get radio's uci config
                htmode[j] = wireless_status[radio_id].config.htmode;
                acs_exclude_dfs[j] = wireless_status[radio_id].config.acs_exclude_dfs || false;
                channel_config[j] = wireless_status[radio_id].config.channel;
                selected_channels[j] = wireless_status[radio_id].config.channels;

                if (radio_band[j] == '5g') {
                    radio_5G_index = j;

                    // check if DFS is enabled for 5G radio
                    if (acs_exclude_dfs[j] == false) {
                        dfs_enabled_5g_f[j] = 1;
                    }
                }

                if (selected_algo == "RCS") {
                    if (channel_config[j] == '0') {
                        ulog_info(`[%s] Configured channel is "auto" \n`, radio_iface[j]);
                    } else {
                        ulog_info(`[%s] Configured channel is fixed at %d \n`, radio_iface[j], channel_config[j]);
                    }

                    if (selected_channels[j]) {
                        ulog_info(`[%s] Selected channel list (please update the radio config, if not correct) = %s \n`, radio_iface[j], selected_channels[j]);
                    }
                } else if (selected_algo == "ACS") {
                    if (channel_config[j] != '0') {
                        ulog_info(`[%s] Configured channel is fixed at %d \n`, radio_iface[j], channel_config[j]);
                        auto_channel_f[j] = 0;

                        stats_info_write("/tmp/fixed_channel_" + radio_iface[j], channel_config[j]);
                        fixed_channel_f[j] = 1;
                    } else {
                        ulog_info(`[%s] Configured channel is "auto" \n`, radio_iface[j]);
                        auto_channel_f[j] = 1;

                        // check if fixed channel exists
                        fixed_chan_bkp[j] = stats_info_read("/tmp/fixed_channel_" + radio_iface[j]);
                        if (fixed_chan_bkp[j] > 0) {
                            ulog_info(`[%s] Configured fixed channel was %d \n`, radio_iface[j], fixed_chan_bkp[j]);
                            fixed_channel_f[j] = 1;
                        }
                    }
                }

                // check current channel
                current_channel[j] = check_current_channel(radio_iface[j]);

                // check current breach_count
                let current_threshold_breach_count = 0;
                current_threshold_breach_count = stats_info_read("/tmp/threshold_breach_count_" + radio_iface[j]);
                ulog_info(`[%s] Allowed consecutive Channel Utilization threshold breach count = %d \n`, radio_iface[j], config.consecutive_threshold_breach);

                if (!current_threshold_breach_count || current_threshold_breach_count == null || current_threshold_breach_count == 'NaN') {
                    // /tmp/threshold_breach_count_<radio_iface> file doesn't exist yet or has invalid value
                    current_threshold_breach_count = 0;
                }
                ulog_info(`[%s] Previous consecutive Channel Utilization threshold breach count = %d \n`, radio_iface[j], current_threshold_breach_count);

                // channel util at this channel (auto/fixed)
                chan_util_value[j] = get_chan_util(radio_band[j], sleep_time);
                ulog_info(`[%s] Allowed Channel Utilization threshold = %d \n`, radio_iface[j], config.threshold);
                ulog_info(`[%s] Current Channel Utilization (Channel %d at %d) = %d \n`, radio_iface[j], current_channel[j], time(), chan_util_value[j]);

                if (chan_util_value[j] >= config.threshold) {
                    check_all_chan_util++;

                    // Channel Utilization threshold exceeded, increase breach count
                    current_threshold_breach_count++;
                    threshold_breach_count[j] = current_threshold_breach_count;
                    ulog_info(`[%s] New consecutive Channel Utilization threshold breach count = %d \n`, radio_iface[j], threshold_breach_count[j]);

                    if (threshold_breach_count[j] >= config.consecutive_threshold_breach) {
                        // threshold breach flag up!
                        threshold_breach_f[j] = 1;
                        // get index of iface which exceeded the threshold breach count
                        check_threshold_breach_idx[j] = j;
                        check_all_threshold_breach++;
                    }
                } else {
                    // reset the threshold breach count
                    ulog_info(`[%s] Current Channel Utilization (%d) < Allowed Channel Utilization threshold (%d) \n`, radio_iface[j], chan_util_value[j], config.threshold);
                    ulog_info(`[%s] Reset consecutive Channel Utilization threshold breach count \n`, radio_iface[j]);

                    threshold_breach_count[j] = 0;
                    // threshold breach flag down!
                    threshold_breach_f[j] = 0;
                }

                update_breach_count(radio_iface[j], threshold_breach_count[j]);
            } else {
                check_all_cool_down++;
            }
        } else if (current_rf_down[j] == 2) {
            // this iface is HaLow interface and we can neither check channel utilization not switch channel
            ulog_info(`[%s] HaLow interface is UP, but RRM cannot be done on this interface \n`, radio_iface[j]);
        } else {
            ulog_info(`[%s] Interface not UP, will be checked in the next interval \n`, radio_iface[j]);
        }
    }

    for (let l = 0; l < num_radios; l++) {
        if (current_rf_down[l] == 0) {
            cool_down_f[l] = cool_down_check(radio_iface[l], cool_down_period);

            if (cool_down_f[l] != 1) {
                // start algo only if threshold breach count, and chan util threshold exceeded from configured values
                if (threshold_breach_f[l] == 1 && chan_util_value[l] >= config.threshold) {
                    ulog_info(`[%s] Consecutive Channel Utilization threshold breached = %d; %s Algorithm STARTS \n`, radio_iface[l], threshold_breach_count[l], selected_algo);

                    if (selected_algo == "RCS") {
                        // no. of channel utils to be compared
                        let max_chan = 2;
                        let curr_chan_list = {};
                        let chan_util_list = {};
                        let init_payload = {};
                        let final_payload = {};
                        let long_cac_time = 0;

                        sleep_time = 3000;

                        ulog_info(`[%s] Total of %d channel utils will be compared \n `, radio_iface[l], max_chan);

                        /* Collect the chan util info of #max_chan channels*/
                        for (let num_chan = 0; num_chan < max_chan; num_chan++) {
                            ulog_info(`[%s] Channel utilization check ROUND#%d \n`, radio_iface[l], num_chan);

                            if (num_chan == 0) {
                                curr_chan_list[num_chan] = current_channel[l];
                                chan_util_list[num_chan] = chan_util_value[l];

                                ulog_info(`[%s] Current channel %d has Channel utilization = %d \n`, radio_iface[l], curr_chan_list[num_chan], chan_util_list[num_chan]);
                            } else {
                                // flag to assign max chan util value
                                let assign_max_chan_util = 0;

                                // call RCS for multiple random chan
                                let chan_scan = algo_rcs(radio_iface[l], curr_chan_list[num_chan-1], radio_band[l], htmode[l], selected_channels[l], acs_exclude_dfs[l]);
                                curr_chan_list[num_chan] = stats_info_read("/tmp/rrm_random_channel_" + radio_iface[l]);

                                if (chan_scan == 1) {
                                    // assign channel from RCS to interface
                                    init_payload = {
                                        channel: curr_chan_list[num_chan],
                                        iface: radio_iface[l],
                                        band: radio_band[l],
                                        htmode: htmode[l],
                                    };

                                    if (l == radio_5G_index) {
                                        dfs_enabled_5g_f[l] = dfs_chan_check(radio_iface[l], init_payload.channel);
                                    }

                                    ulog_info(`[%s] Initiated channel switch to random channel %d for comparing Channel utilization \n`, radio_iface[l], init_payload.channel);
                                    let init_chan_switch_status = hostapd_switch_channel(init_payload);

                                    if (init_chan_switch_status != 0) {
                                        let actual_channel = switch_status_check(radio_iface[l], dfs_enabled_5g_f[l]);

                                        if (actual_channel == init_payload.channel) {
                                            ulog_info(`[%s] Channel Switch success; Checking Channel utilization ... \n`, radio_iface[l]);
                                            // get chan util for current assigned random channel
                                            sleep(5000);
                                            chan_util_list[num_chan] = get_chan_util(radio_band[l], sleep_time);
                                        } else {
                                            if (dfs_enabled_5g_f[l] == 1 && interface_status_check(radio_iface[l]) == 1) {
                                                // dfs channel not up yet
                                                ulog_info(`[%s] DFS channel %d taking too long to be UP. Interface status/Channel utilization will be checked in the next interval\n`, radio_iface[l], init_payload.channel);
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
                                    ulog_info(`[%s] Channel switch fail; assign Channel utilization = 100 \n`, radio_iface[l]);
                                    // assign highest util value for invalid channel
                                    chan_util_list[num_chan] = 100;
                                }

                                ulog_info(`[%s] Channel utilization of random channel#%d (%s) = %d \n`, radio_iface[l], num_chan, curr_chan_list[num_chan], chan_util_list[num_chan] );
                            }
                        }

                        /* Switch to the channel with the lowest chan util; if long_cac_time flag is up then switch back to the previous channel */
                        if (long_cac_time != 1) {
                            ulog_info(`[%s] Channel utilization of all %d channels checked \n`, radio_iface[l], max_chan);

                            // find the minimum chan util and select that as the next channel
                            let min_util = chan_util_list[0];
                            let index_min_util = 0;
                            for (let x = 0; x < max_chan; x++) {
                                ulog_info(`[%s] Channel#%d = %s; Channel utilization#%d = %d \n`, radio_iface[l], x, curr_chan_list[x], x, chan_util_list[x]);

                                if (chan_util_list[x] < min_util) {
                                    min_util = chan_util_list[x];
                                    index_min_util = x;
                                }
                            }

                            ulog_info(`[%s] Channel %d has the least Channel utilization of %d; switching to this channel \n`, radio_iface[l], curr_chan_list[index_min_util], min_util );

                            let _current_channel = check_current_channel(radio_iface[l]);

                            if (_current_channel != curr_chan_list[index_min_util]) {
                                // switch channel to min_util
                                final_payload = {
                                    channel: curr_chan_list[index_min_util],
                                    iface: radio_iface[l],
                                    band: radio_band[l],
                                    htmode: htmode[l],
                                };

                                if (l == radio_5G_index) {
                                    dfs_enabled_5g_f[l] = dfs_chan_check(radio_iface[l], final_payload.channel);
                                }

                                if (final_payload.channel != curr_chan_list[max_chan-1] || min_util != chan_util_list[max_chan-1]) {
                                    ulog_info(`[%s] Initiated final channel switch to Channel %d \n`, radio_iface[l], final_payload.channel);
                                    let final_switch_status = hostapd_switch_channel(final_payload);

                                    if (final_switch_status != 0) {
                                        let final_channel = switch_status_check(radio_iface[l], dfs_enabled_5g_f[l]);

                                        if (final_channel == final_payload.channel) {
                                            ulog_info(`[%s] Final channel switch success \n`, radio_iface[l]);
                                        } else {
                                            ulog_info(`[%s] RCS algo fail (final channel switch failure), wait until next interval to retry\n`, radio_iface[l]);
                                        }
                                    } else {
                                        ulog_info(`[%s] RCS algo fail (final channel switch failure at hostapd_cli), wait until next interval to retry\n`, radio_iface[l]);
                                    }
                                } else {
                                    ulog_info(`[%s] Channel switch not necessary, current channel %d has the least channel utilization value \n`, radio_iface[l], final_payload.channel);
                                    // reset breach count
                                    update_breach_count(radio_iface[l], 0);
                                }
                            } else {
                                ulog_info(`[%s] Channel switch not necessary, current channel %d is already assigned to the interface \n`, radio_iface[l], _current_channel);
                                // reset breach count
                                update_breach_count(radio_iface[l], 0);
                            }
                        } else {
                            // revert back to the original channel
                            ulog_info(`[%s] Channel %d may have a cac_time longer than 60 seconds, RRM failed for this interval (you might want to avoid selecting this channel) \n`, radio_iface[l], init_payload.channel);
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

                        // Channel switch in progress, set flag = 1
                        stats_info_write("/tmp/rrm_chan_switch", 1);

                        // flag to check if 5G radio was restarted
                        let radio_5g_restarted = 0;

                        if (check_all_chan_util == num_radios && check_all_cool_down == 0 && check_all_threshold_breach == num_radios) {
                            // Channel util high for all interfaces && cool down period over && threshold breach count exceeded for all interfaces: restart all interfaces

                            for (let m = 0; m < num_radios; m++) {
                                fixed_channel_config(radio_iface[m], m, fixed_channel_f[m], auto_channel_f[m], fixed_chan_bkp[m], channel_config[m]);
                            }

                            ulog_info(`[all wlan interfaces] Initiating channel switch in %d seconds ... \n`, random_wait_time);
                            sleep(random_wait_time*1000);
                            ulog_info(`[all wlan interfaces] %s Algorithm will start; Turning DOWN/UP \n`, selected_algo);

                            // 5G radio was restarted
                            radio_5g_restarted = 1;

                            for (let x = 0; x < num_radios; x++) {
                                ulog_info(`[%s] Channel will be switched \n`, radio_iface[x]);
                                // timestamp for all interfaces must be saved
                                update_channel_switch_time(radio_iface[x]);

                                // reset breach count back to 0 as we are calling the channel selection algo
                                update_breach_count(radio_iface[x], 0);
                            }

                            // restart all wlan interfaces
                            system(`wifi down`);
                            system(`wifi up`);
                        } else if (check_all_chan_util >= 1 && check_all_cool_down < num_radios && check_all_threshold_breach >= 1) {
                            // Channel util high for one or more interfaces && cool down period over for one or more interfaces && threshold breach count exceeded for one or more interface: restart that interface
                            for (let high_util_iface in check_threshold_breach_idx) {
                                fixed_channel_config(radio_iface[high_util_iface], high_util_iface, fixed_channel_f[high_util_iface], auto_channel_f[high_util_iface], fixed_chan_bkp[high_util_iface], channel_config[high_util_iface]);

                                ulog_info(`[%s] Initiating channel switch in %d seconds ... \n`, radio_iface[high_util_iface], random_wait_time);
                                sleep(random_wait_time*1000);
                                ulog_info(`[%s] %s Algorithm will start; Turning DOWN/UP \n`, radio_iface[high_util_iface], selected_algo);

                                ulog_info(`[%s] Channel will be switched \n`, radio_iface[high_util_iface]);
                                // timestamp for wlanX interfaces must be saved
                                update_channel_switch_time(radio_iface[high_util_iface]);

                                // reset breach count back to 0 as we are calling the channel selection algo
                                update_breach_count(radio_iface[high_util_iface], 0);

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
                        if (radio_5g_restarted == 1 && dfs_enabled_5g_f[radio_5G_index] == 1) {
                            ulog_info(`[%s] 5G radio might need some time to be UP (DFS enabled) ... wait for 30 seconds \n`, radio_iface[radio_5G_index]);
                            // 30 sec delay for DFS scan to come finish
                            sleep(30000);
                        }
                    }

                    sleep(5000);
                    // Channel switch done, set flag = 0
                    stats_info_write("/tmp/rrm_chan_switch", 0);
                } else {
                    if (threshold_breach_f[l] != 1) {
                        ulog_info(`[%s] Threshold breach count (=%d) < Allowed consecutive Channel Utilization threshold breach count (=%d), will be checked again in the next interval \n`, radio_iface[l], threshold_breach_count[l], config.consecutive_threshold_breach);
                    } else {
                        ulog_info(`[%s] Channel utilization (=%d) within threshold (=%d), will be checked again in the next interval \n`, radio_iface[l], chan_util_value[l], config.threshold);
                    }
                }
            } else {
                ulog_info(`[%s] Need to cool down (%d seconds hasn't passed); will be checked again in the next interval \n`, radio_iface[l], cool_down_period/1000);
            }
        }
    }
    ulog_info(`RRM with channel optimization finished; next RRM round starts in %d seconds \n`, config.interval/1000);
    record_rrm_timestamp();

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
