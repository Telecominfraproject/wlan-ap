#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.  
#SPDX-License-Identifier: ISC

#!/bin/sh

if [ ! -d /sys/kernel/debug/tracing ]; then
    exit 0
fi

cd /sys/kernel/debug/tracing || exit 1

# cfg80211 events
CFG80211_EVENTS="
cfg80211_assoc_comeback
cfg80211_ch_switch_notify
cfg80211_del_sta
cfg80211_ibss_joined
cfg80211_new_sta
cfg80211_notify_new_peer_candidate
cfg80211_pmksa_candidate_notify
cfg80211_probe_status
cfg80211_ready_on_channel
cfg80211_ready_on_channel_expired
cfg80211_rx_control_port
cfg80211_send_assoc_failure
cfg80211_send_auth_timeout
cfg80211_send_rx_assoc
cfg80211_send_rx_auth
cfg80211_tx_mlme_mgmt
cfg80211_new_sta
cfg80211_return_bool
cfg80211_control_port_tx_status
cfg80211_rx_control_port
rdev_add_station
rdev_add_link_station
rdev_assoc
rdev_auth
rdev_change_station
rdev_connect
rdev_deauth
rdev_del_station
rdev_del_link_station
rdev_disassoc
rdev_disconnect
rdev_get_station
rdev_remain_on_channel
rdev_set_default_key
rdev_set_default_mgmt_key
rdev_set_rekey_data
rdev_set_tx_power
rdev_start_ap
rdev_stop_ap
rdev_tdls_cancel_channel_switch
rdev_tdls_channel_switch
rdev_tdls_mgmt
rdev_tdls_oper
rdev_tx_control_port
rdev_update_ap
rdev_update_connect_params
rdev_dump_station
rdev_add_key
rdev_probe_client
rdev_del_key
rdev_return_int
rdev_mgmt_tx
rdev_mgmt_tx_cancel_wait
wiphy_work_queue
rdev_return_int_cookie
wiphy_work_worker_start
wiphy_work_run
"

# mac80211 events
MAC80211_EVENTS="
api_connection_loss
api_disconnect
api_cqm_beacon_loss_notify
api_beacon_loss
drv_flush_sta
drv_sta_add
drv_sta_remove
drv_sta_state
drv_change_sta_links
drv_sta_set_decap_offload
drv_start
drv_stop
drv_add_interface
drv_remove_interface
drv_change_interface
drv_channel_switch
drv_abort_channel_switch
drv_set_key
drv_change_sta_links
drv_set_key
drv_sta_statistics
drv_get_expected_throughput
drv_sta_set_4addr
drv_ampdu_action
drv_sta_pre_rcu_remove
drv_return_int
"
for event in $CFG80211_EVENTS; do
  echo 1 > "events/cfg80211/$event/enable" 2>/dev/null
done

for event in $MAC80211_EVENTS; do
  echo 1 > "events/mac80211/$event/enable" 2>/dev/null
done

