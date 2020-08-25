#Add/Delete Rate limiting for wifi client based on MAC address
. /lib/functions.sh

rate=0
iface=$2
handle_interface() {
	local ifa rlimit
	ifa=$1
	if [ "$ifa" == $iface ]; then
		config_get rlimit $iface rlimit
		if [ -z "$rlimit" -o "$rlimit" == "0" ]; then
			rate=0
			return
		fi
		if [ $2 == "download" ]; then
			config_get rate $iface cdrate
		elif [ $2 == "upload" ]; then
			config_get rate $iface curate
		fi
	fi
}

if [ -z "$1" -o -z "$2" -o -z "$3" ]; then
	exit 1
fi

logger -t "$1 $2 $3"

if [ "$1" == "add" ]; then
	config_load wireless

	config_foreach handle_interface wifi-iface download

	exists=`nft list chain bridge nft-qos-ssid-lan-bridge download  -a | grep -ic $3`
	logger -t "mac-rate" "exists = $exists"
	if [ "$exists" -ne 0 ]; then
		old_drate=`nft list chain bridge nft-qos-ssid-lan-bridge download -a | grep -i $3 |  awk -F'kbytes' '{print $1}' | awk '{print $NF}'`
		logger -t "mac-rate" "old_drate=$old_drate"
		if [ "$old_drate" -ne "$rate" ]; then
			changed=1
			logger -t "mac-rate" "changed DL $old_drate to $rate"
		else
			changed=0
			logger -t "mac-rate" "Not changed DL $old_drate to $rate"
		fi
	fi

        if [ "$exists" == 0 -o "$changed" == 1 ]; then
	        if [ "$rate" -ne 0 ]; then
			dok=`nft add rule bridge nft-qos-ssid-lan-bridge download ether daddr $3 limit rate over $rate kbytes/second drop`
	        fi
        fi

	config_foreach handle_interface wifi-iface upload
	exists=`nft list chain bridge nft-qos-ssid-lan-bridge upload  -a | grep -ic $3`
	if [ "$exists" -ne 0 ]; then
		old_urate=`nft list chain bridge nft-qos-ssid-lan-bridge upload -a | grep -i $3 |  awk -F'kbytes' '{print $1}' | awk '{print $NF}'`
		if [ "$old_urate" -ne "$rate" ]; then
			changed=1
			logger -t "mac-rate" "changed UL $old_urate to $rate"
		else
			changed=0
			logger -t "mac-rate" "Not changed UL $old_urate to $rate"
		fi
	fi

        if [ "$exists" == 0 -o "$changed" == 1 ]; then
        	if [ "$rate" -ne 0 ]; then
			uok=`nft add rule bridge nft-qos-ssid-lan-bridge upload ether saddr $3 limit rate over $rate kbytes/second drop`
		fi
	fi

elif [ "$1" == "del" ]; then
	id=`nft list chain bridge nft-qos-ssid-lan-bridge download  -a | grep -i $3  | awk -F "handle " '{print $2;exit}'`
	logger -t "mac-rate" "$id $3"
	if [ -n "$id" ]; then
		nft delete rule bridge nft-qos-ssid-lan-bridge download handle $id
	fi

	id=`nft list chain bridge nft-qos-ssid-lan-bridge upload  -a | grep -i $3  | awk -F "handle " '{print $2;exit}'`
	if [ -n "$id" ]; then
		nft delete rule bridge nft-qos-ssid-lan-bridge upload handle $id
	fi

fi
