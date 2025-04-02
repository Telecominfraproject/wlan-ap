kill_wait()
{
	local names=$*
	local count=30

	for pid in $(pidof $names)
	do
		kill $pid &> /dev/null
	done

	while pidof $names &> /dev/null;
	do
		usleep 100000
		let "count--"
		if [ $count -eq 0 ]
		then
			echo "$names failed to terminate normally, force quitting" >&2
			kill -9 $(pidof $names)
			return 1
		fi
	done
	return 0
}

_list_phy_interfaces() {
	local phy="$1"
	if [ -d "/sys/class/ieee80211/${phy}/device/net" ]; then
		ls "/sys/class/ieee80211/${phy}/device/net" 2>/dev/null;
	else
		ls "/sys/class/ieee80211/${phy}/device" 2>/dev/null | grep net: | sed -e 's,net:,,g'
	fi
}

list_phy_interfaces() {
	local phy="$1"

	for dev in $(_list_phy_interfaces "$phy"); do
		readlink "/sys/class/net/${dev}/phy80211" | grep -q "/${phy}\$" || continue
	done
}

num_test()
{
	case $1 in
		''|*[!0-9]*)
			return 1
			;;
		*)
			;;
	esac
	return 0
}

_get_regulatory() {
	local _mode=$1
	local _country=$2
	local _channel=$3
	local _op_class=$4

	local dc_min="0.01"
	local dc_max="100.00"
	local cc; local bw; local l_op; local g_op;
	local freq; local dc_ap; local dc_sta;

	oIFS=$IFS
	HEADER=1
	while IFS=, read -r cc bw ch l_op g_op freq remainder; do
		if [ $HEADER = 1 ]; then
			HEADER=0
			continue
		fi
		num_test $bw
		[ $? -eq 1 ] 		&& continue
		num_test $ch
		[ $? -eq 1 ] 		&& continue
		num_test $l_op
		[ $? -eq 1 ] 		&& continue
		num_test $g_op
		[ $? -eq 1 ] 		&& continue

		if [ "$cc" == "$_country" ] && [ "$ch" -eq "$_channel" ]; then
			if [ -z "$_op_class" ]; then
				halow_bw=$bw
				center_freq=$freq
				# If you didn't pass op_class, set it from this data.
				op_class="$g_op"
				IFS=$oIFS
				return 0;
			elif [ "$l_op" -eq "$_op_class" ] || [ "$g_op" -eq "$_op_class" ]; then
				halow_bw=$bw
				center_freq=$freq
				IFS=$oIFS
				return 0;
			fi
		fi
	done < /usr/share/morse-regdb/channels.csv

	IFS=$oIFS
	return 1;
}


morse_find_ifname()
{
	for file in "/sys/class/net/wlan"*;
	do
		if [ -d "$file"/device/morse ]
		then
			ifname=$(basename $file)
			break
		fi
	done
}

# this function checks if an macaddr is burnt into the device.
# if so, returns it, otherwise returns an empty string
morse_get_chip_macaddr()
{
	ifname=
	morse_find_ifname
	local state=$(cat /sys/class/net/${ifname}/operstate 2>/dev/null)
	[ $? -ne 0 ] && return

	if [ "$state" == "down" ]; then
		ip link set ${ifname} up
		[ $? -ne 0 ] && return
	fi

	local chip_macaddr="$(morse_cli -i ${ifname} macaddr 2>/dev/null)"
	[ $? -ne 0 ] && printf ""
	chip_macaddr=${chip_macaddr##"Chip MAC address: "}

	#make sure (using regex) that the we got a macaddr
	if [[ "$chip_macaddr" =~ ^\([0-9A-Fa-f]{2}[:]\){5}\([0-9A-Fa-f]{2}\)$ ]]; then
		[ "$chip_macaddr" = "00:00:00:00:00:00" ] && printf "" || printf "$chip_macaddr"
	else
		printf ""
	fi

	if [ "$state" == "down" ]; then
		ip link set ${ifname} down
	fi
}

update_dpp_qrcode()
{
	local uci_changes_path="$(mktemp -d)"
	local private_key_path=$1
	#remove ':' from the macaddress
	local mac_address=$(echo "$2" | sed -r 's/://g')
	#generate the public key string from the private key.
	#unfortunately, there is no way to quiet the 'read EC key' messages without redirecting stderr.
	local pubkey=$(openssl ec -in $private_key_path -pubout -conv_form compressed -outform DER 2> /dev/null | hexdump -e '16/1 "%02x " "\n"' | xxd -r -p | base64 -w0)
	#save qrcode string into /www
	qrencode --inline --8bit --type=SVG --output=/tmp/dpp_qrcode.svg "DPP:V:2;M:$mac_address;K:$pubkey;;"
	#only write if necessary
	if ! cmp -s /tmp/dpp_qrcode.svg /www/dpp_qrcode.svg; then
		cp /tmp/dpp_qrcode.svg /www/dpp_qrcode.svg
	fi
	rm /tmp/dpp_qrcode.svg
}