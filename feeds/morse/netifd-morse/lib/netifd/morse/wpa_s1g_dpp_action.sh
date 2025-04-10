#!/bin/sh


. /lib/functions.sh
. /usr/share/libubox/jshn.sh
. /lib/netifd/netifd-wireless.sh
. /etc/diag.sh

#returns 0 if config is complete
is_config_complete()
{
    [ -z "$encryption" ] && return 1

    #if encryption is sae, check if we have psk
    if [ "$encryption" = "sae" ]; then
        [ -z "$psk" ] && return 1
    fi

    #check if we have ssid
    [ -z "$ssid" ] && return 1

    return 0
}

get_uci_section()
{
    local target_iface=$1
    local dpp_wifi_iface=

    check_interface() {
        json_get_vars section ifname
        [ "$ifname" = "$target_iface" ] && [ -n "$section" ] && [ -z "$dpp_wifi_iface" ] && dpp_wifi_iface=$section
    }

    wireless_status=$(ubus call network.wireless status)

    json_init
    json_load "$wireless_status"
    json_get_keys radios

    for radio in $radios; do
        json_select $radio
        for_each_interface "sta" check_interface
        json_select ..
    done
    echo "$dpp_wifi_iface"
}

save_and_reload()
{
    logger "Saving Received AP Credentials."
    local uci_section=$(get_uci_section $iface_name)
    if [ -z "$uci_section" ]; then
        logger "FATAL: unable to find the correct wifi interface in configs."
        exit 0
    fi

    uci set wireless.$uci_section.encryption="$encryption"

    #if encryption is sae, check if we have psk
    if [ "$encryption" = "sae" ]; then
        uci set wireless.$uci_section.key="$psk"
    fi

    uci set wireless.$uci_section.ssid="$ssid"

    uci set wireless.$uci_section.dpp='0'

    uci commit
    ubus call service event "{ \"type\": \"config.change\", \"data\": { \"package\": \"wireless\" }}"
}

# Do not name 'config'; will clash with uci functions.
do_config() {
    # convert password from hex string to string.
    psk="$(echo "$psk" | xxd -r -p)"

    # if the file contains all the necessary stuff, then save and reload.
    is_config_complete && save_and_reload
}

failed() {
    set_state dpp_failed
}

finished() {
    set_state done
}

started() {
    set_state dpp_started
}

case "$1" in
    config)
        do_config
    ;;
    failed)
        failed
    ;;
    finished)
        finished
    ;;
    started)
        started
    ;;
    *)
        echo "Usage: $0 {config|failed|finished|started}"
        exit 1
    ;;
esac
