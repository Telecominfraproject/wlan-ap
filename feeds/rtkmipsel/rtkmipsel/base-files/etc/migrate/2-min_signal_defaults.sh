add_min_signal_defaults () {

    local min_signal_allowed

    config_get min_signal_allowed "$1" min_signal_allowed

    [ -z "$min_signal_allowed" ] &&  {
        uci set wireless."$1".min_signal_allowed=0
    }
}

. /lib/functions.sh

config_load wireless
config_foreach add_min_signal_defaults wifi-iface
uci commit wireless
