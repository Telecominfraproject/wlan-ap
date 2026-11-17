#
# Shared helpers for the morse netifd handler.
#

# Upstream uses `sleep 1` with count=3 (3 s ceiling, 1 s
# granularity). This version polls every 100 ms with count=30 - same 3 s ceiling,
# ten times the resolution, which matters because morse_interface_cleanup calls
# it on every `wifi up`. usleep is why netifd-morse depends on
# @BUSYBOX_CONFIG_USLEEP.
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

# Upstream, unchanged.
_list_phy_interfaces() {
	local phy="$1"
	if [ -d "/sys/class/ieee80211/${phy}/device/net" ]; then
		ls "/sys/class/ieee80211/${phy}/device/net" 2>/dev/null;
	else
		ls "/sys/class/ieee80211/${phy}/device" 2>/dev/null | grep net: | sed -e 's,net:,,g'
	fi
}

# Upstream, unchanged.
list_phy_interfaces() {
	local phy="$1"

	for dev in $(_list_phy_interfaces "$phy"); do
		readlink "/sys/class/net/${dev}/phy80211" | grep -q "/${phy}\$" || continue
	done
}

# Kept for callers outside this file.
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

MORSE_REGDB_CHANNELS=/usr/share/morse-regdb/channels.csv

# Column resolution for channels.csv, shared by the two lookups below.
# Columns are looked up by name, not by position: morse-regdb v2.1.1 (used
# here) has no channelizations/default_channelization columns, while newer
# releases insert them right after country_code.
_regdb_awk_prologue='
	NR == 1 {
		for (i = 1; i <= NF; i++) {
			gsub(/^[ \t\r]+|[ \t\r]+$/, "", $i)
			col[$i] = i
		}
		c_cc = col["country_code"]; c_bw = col["bw"]; c_ch = col["s1g_chan"]
		c_lop = col["s1g_op_class"]; c_gop = col["global_op_class"]
		c_freq = col["centre_freq_mhz"]
		c_chzn = col["channelizations"]; c_dchzn = col["default_channelization"]
		if (!c_cc || !c_bw || !c_ch || !c_lop || !c_gop || !c_freq) {
			# Unrecognised header: assume the v2.1.1 positional layout.
			c_cc = 1; c_bw = 2; c_ch = 3; c_lop = 4; c_gop = 5; c_freq = 6
			c_chzn = 0; c_dchzn = 0
		}
		next
	}
	function chzn_ok() {
		if (!c_chzn) return 1                 # column absent: no filtering
		if ($c_chzn == "") return 1           # row applies to every channelization
		if (want_chzn != "") {
			n = split($c_chzn, parts, ";")
			for (j = 1; j <= n; j++)
				if (parts[j] == want_chzn) return 1
			return 0
		}
		if (!c_dchzn) return 1
		return ($c_dchzn == "True")
	}
	$c_cc != want_cc { next }
	!chzn_ok() { next }
'

# Get regulatory info.
# Country is required, remaining args are optional.
# If channel is unset, it will choose a high bandwidth channel.
# If channel is auto, it will choose the maximum available bandwidth if bandwidth
# is not provided.
# Sets "$country", "$channel", "$s1g_chanbw", and "$op_class".
# "$center_freq" is also set if channel is not auto.
#
# Upstream's function and upstream's contract; only the CSV parsing is adapted
# (see the block above).
_get_regulatory() {
	country=$1
	channel=$2
	s1g_chanbw=$3
	op_class=$4
	s1g_chzn=$5

	local row

	if [ -z "$country" ]; then
		return 2
	fi

	if [ ! -r "$MORSE_REGDB_CHANNELS" ]; then
		echo "$MORSE_REGDB_CHANNELS is missing; is morse-regdb installed?" >&2
		return 2
	fi

	# Setting auto_channel allows _get_regulatory to operate independent of netifd wireless scripts.
	if [ -z "$auto_channel" ]; then
		case "$channel" in
			""|0|auto)
				auto_channel=1 ;;
			[0-9]*)
				auto_channel=0 ;;
			*)
				return 2 ;;
		esac
	fi

	# Choose the maximum possible bandwidth if no bw set and auto
	# (NB if no channel is set, auto_channel is 1).
	if [ "$auto_channel" -gt 0 -a -z "$s1g_chanbw" ]; then
		s1g_chanbw="$(awk -F, -v want_cc="$country" -v want_chzn="$s1g_chzn" \
			"$_regdb_awk_prologue"'
			{ if ($c_bw + 0 > max) max = $c_bw + 0 }
			END { if (max > 0) print max }
		' "$MORSE_REGDB_CHANNELS")"
	fi

	# Emits "bw channel local_op_class global_op_class centre_freq" for the first
	# row that matches, in file order - same selection rule as upstream's shell
	# loop.
	row="$(awk -F, -v want_cc="$country" -v want_chzn="$s1g_chzn" \
		-v want_bw="$s1g_chanbw" -v want_op="$op_class" -v want_ch="$channel" \
		-v auto="$auto_channel" \
		"$_regdb_awk_prologue"'
		{
			if (want_bw != "" && $c_bw + 0 != want_bw + 0) next
			if (want_op != "" && $c_lop + 0 != want_op + 0 && $c_gop + 0 != want_op + 0) next
			if (auto + 0 == 0 && $c_ch + 0 != want_ch + 0) next
			print $c_bw, $c_ch, $c_lop, $c_gop, $c_freq
			exit
		}
	' "$MORSE_REGDB_CHANNELS")"

	[ -n "$row" ] || return 1

	set -- $row
	s1g_chanbw="$1"

	# If an auto channel is selected here, we use the op_class to restrict
	# to the right bandwidth. Oddly, hostapd_s1g (ACS) and smart_manager (DCS)
	# appear to not respect this op_class completely, but do use it to determine
	# the bandwidth.
	if [ -z "$op_class" ]; then
		op_class="$4"
	fi

	if [ "$auto_channel" -eq 0 ]; then
		channel="$2"
		center_freq="$5"
	fi

	return 0
}

# Upstream, unchanged.
#
# Tied to the wlanN interface naming kept for EAP112 - see the DEFAULT_MORSE_IFNAME
# note in lib/netifd/wireless/morse.sh. If this tree ever moves to upstream's wlhN
# prefix, this glob has to move with it.
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

# Returns the MAC address burnt into the chip, or an empty string.
# The `[[ ... =~ ... ]]` test relies on busybox ASH_BASH_COMPAT.
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
