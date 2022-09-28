#================================================================
# HEADER
#================================================================

channel_2g=1
channel_5g=36
country="US"
ssid_2g="Openwrt-7916-2g"
ssid_5g="Openwrt-7916-5g"

#================================================================
# END_OF_HEADER
#================================================================

wifi down
rm -rf /etc/config/wireless

cat > /etc/config/wireless <<EOF
config wifi-device 'radio0'
        option type 'mac80211'
        option path '11280000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0'
        option channel '${channel_2g}'
        option band '2g'
        option htmode 'HE40'
		option noscan '1'
		option disabled '0'
		option country '${country}'

config wifi-iface 'default_radio0'
        option device 'radio0'
        option network 'lan'
        option mode 'ap'
        option ssid '${ssid_2g}'
        option encryption 'none'

config wifi-device 'radio1'
        option type 'mac80211'
        option path '11280000.pcie/pci0000:00/0000:00:00.0/0000:01:00.0+1'
        option channel '${channel_5g}'
        option band '5g'
        option htmode 'HE80'
        option disabled '0'
		option country '${country}'

config wifi-iface 'default_radio1'
        option device 'radio1'
        option network 'lan'
        option mode 'ap'
        option ssid '${ssid_5g}'
        option encryption 'none'
EOF

wifi up
wifi reload

sleep 5

iwinfo