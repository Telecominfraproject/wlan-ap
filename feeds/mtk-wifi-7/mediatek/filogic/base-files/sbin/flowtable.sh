#!/bin/sh

# Configuration flags
NFT_ENABLE=1  # 1: use nftables, 0: use iptables
HW_OFFLOAD=1   # 1: hardware offload path, 0: software fast path
FLOWTABLE_CONFIG="/etc/flowtable.conf"  # nft flowtable configuration file

show_help() {
	echo "Usage: $(basename $0) [-h] [-f] [-m hw|sw] [-t nftables|iptables]"
	echo ""
	echo "Options:"
	echo "  -h, --help           Display this help message"
	echo "  -f, --flush          Disable all flow offload rules"
	echo "  -m, --mode [hw|sw]   Set offload mode: hw (hardware, default) or sw (software)"
	echo "  -t, --tool [nftables|iptables]  Set tool: nftables (default) or iptables"
	echo ""
	echo "Examples:"
	echo "  $(basename $0)                # Enable hardware offload with nftables"
	echo "  $(basename $0) -t iptables    # Enable hardware offload with iptables"
	echo "  $(basename $0) -m sw          # Enable software fast path with nftables"
	echo "  $(basename $0) -t iptables -m sw # Enable software fast path with iptables"
}

nftables_flowoffload_disable()
{
	# delete existing nft flowtable configuration
	nft delete table inet filter 2>/dev/null
}

nftables_flowoffload_enable()
{
	local hw_path=${1:-$HW_OFFLOAD}  # Use parameter if provided, else use global HW_OFFLOAD

	# Validate parameter
	if [ "$hw_path" -ne 0 ] && [ "$hw_path" -ne 1 ]; then
		echo "Error: Invalid parameter. Must be 1 (hardware) or 0 (software)" >&2
		return 1
	fi

	# check all of the ETH/WiFi virtual interfaces
	eth_ifs=$(ls /sys/class/net | grep -E '^(eth|lan|wan|mxl|ppp)')
	wifi_ifs=$(iw dev | grep 'Interface' | awk '{print $2}')

	interfaces=$(echo -e "$eth_ifs\n$wifi_ifs")

	if [ -z "$interfaces" ]; then
		echo "Error: No valid network interfaces found" >&2
		return 1
	fi

	# generate ETH/WiFi virtual interfaces list
	interfaces_list=$(echo $interfaces | tr ' ' ', ')

	# determine flags line based on hw_path parameter
	if [ "$hw_path" -eq 1 ]; then
		flags_line="flags offload;"
	else
		flags_line=""
	fi

	# generate nft flowtable configuration rules
	cat <<EOL > $FLOWTABLE_CONFIG
table inet filter {
	flowtable f {
		hook ingress priority filter + 1;
		devices = { $interfaces_list };
		$flags_line
		counter;
	}

	chain forward {
		type filter hook forward priority filter; policy accept;
		meta pkttype multicast accept;
		meta l4proto { tcp, udp } flow add @f;
	}
}
EOL

	# clear existing flowoffload rules
	iptables_flowoffload_disable
	nftables_flowoffload_disable

	# apply nft flowtable configuration file
	if ! nft -f $FLOWTABLE_CONFIG; then
		echo "Error: Failed to apply nftables configuration" >&2
		return 1
	fi
}

iptables_flowoffload_disable()
{
	iptables -D FORWARD -m conntrack --ctproto tcp -j FLOWOFFLOAD --hw 2>/dev/null
	iptables -D FORWARD -m conntrack --ctproto udp -j FLOWOFFLOAD --hw 2>/dev/null
	iptables -D FORWARD -m conntrack --ctproto tcp -j FLOWOFFLOAD 2>/dev/null
	iptables -D FORWARD -m conntrack --ctproto udp -j FLOWOFFLOAD 2>/dev/null
	iptables -D FORWARD -m pkttype --pkt-type multicast -j ACCEPT 2>/dev/null

	ip6tables -D FORWARD -m conntrack --ctproto tcp -j FLOWOFFLOAD --hw 2>/dev/null
	ip6tables -D FORWARD -m conntrack --ctproto udp -j FLOWOFFLOAD --hw 2>/dev/null
	ip6tables -D FORWARD -m conntrack --ctproto tcp -j FLOWOFFLOAD 2>/dev/null
	ip6tables -D FORWARD -m conntrack --ctproto udp -j FLOWOFFLOAD 2>/dev/null
	ip6tables -D FORWARD -m pkttype --pkt-type multicast -j ACCEPT 2>/dev/null
}

iptables_flowoffload_enable()
{
	local hw_path=${1:-$HW_OFFLOAD}  # Use parameter if provided, else use global HW_OFFLOAD

	# Validate parameter
	if [ "$hw_path" -ne 0 ] && [ "$hw_path" -ne 1 ]; then
		echo "Error: Invalid parameter. Must be 1 (hardware) or 0 (software)" >&2
		return 1
	fi

	# clear existing flowoffload rules
	nftables_flowoffload_disable
	iptables_flowoffload_disable

	if [ "$hw_path" -eq 1 ]; then
		#TCP/UDP hardware Binding
		iptables -I FORWARD -m conntrack --ctproto tcp -j FLOWOFFLOAD --hw
		iptables -I FORWARD -m conntrack --ctproto udp -j FLOWOFFLOAD --hw
		ip6tables -I FORWARD -m conntrack --ctproto tcp -j FLOWOFFLOAD --hw
		ip6tables -I FORWARD -m conntrack --ctproto udp -j FLOWOFFLOAD --hw
	else
		#TCP/UDP software fast path
		iptables -I FORWARD -m conntrack --ctproto tcp -j FLOWOFFLOAD
		iptables -I FORWARD -m conntrack --ctproto udp -j FLOWOFFLOAD
		ip6tables -I FORWARD -m conntrack --ctproto tcp -j FLOWOFFLOAD
		ip6tables -I FORWARD -m conntrack --ctproto udp -j FLOWOFFLOAD
	fi

	#Multicast skip Binding
	iptables -I FORWARD -m pkttype --pkt-type multicast -j ACCEPT
	ip6tables -I FORWARD -m pkttype --pkt-type multicast -j ACCEPT
}

main() {
	while [ $# -gt 0 ]; do
		key="$1"
		case $key in
			-h|--help)
				show_help
				return 0
				;;
			-f|--flush)
				nftables_flowoffload_disable
				iptables_flowoffload_disable
				echo "Successfully disabled all flow offload rules"
				return 0
				;;
			-m|--mode)
				if [ -n "$2" ]; then
					case "$2" in
						hw)
							HW_OFFLOAD=1
							;;
						sw)
							HW_OFFLOAD=0
							;;
						*)
							echo "Error: Invalid mode '$2'."
							echo "Use 'hw' or 'sw'."
							show_help
							return 1
							;;
					esac
					shift 2
				else
					echo "Error: --mode requires argument (hw|sw)."
					show_help
					return 1
				fi
				;;
			-t|--tool)
				if [ -n "$2" ]; then
					case "$2" in
						nftables|nft)
							NFT_ENABLE=1
							;;
						iptables|ipt)
							NFT_ENABLE=0
							;;
						*)
							echo "Error: Invalid tool '$2'."
							echo "Use 'nftables' or 'iptables'."
							show_help
							return 1
							;;
					esac
					shift 2
				else
					echo "Error: --tool requires argument (nftables|iptables)."
					show_help
					return 1
				fi
				;;
			*)
				echo "Error: Unknown option '$key'"
				show_help
				return 1
				;;
		esac
	done

	# Display current configuration
	echo "Current configuration:"
	if [ "$NFT_ENABLE" -eq 1 ]; then
		echo "Tool: nftables"
	else
		echo "Tool: iptables"
	fi
	if [ "$HW_OFFLOAD" -eq 1 ]; then
		echo "Path: hardware offload"
	else
		echo "Path: software fast path"
	fi
	echo ""

	# Apply configuration
	if [ "$NFT_ENABLE" -eq 1 ]; then
		nftables_flowoffload_enable "$HW_OFFLOAD"
	else
		iptables_flowoffload_enable "$HW_OFFLOAD"
	fi
}

# Only run main function if script is executed directly (not sourced)
case "$0" in
    *flowtable.sh)
        main "$@"
        ;;
esac
