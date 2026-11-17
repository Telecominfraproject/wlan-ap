#!/bin/sh
#
# Renew DHCP when a HaLow client re-associates.
#
# The upstream that carries a HaLow client is normally a bridge, because it also holds the
# ethernet port. netifd decides whether an interface is still connected from the bridge
# netdev's carrier alone, and a bridge's carrier is the OR of its members, so as long as any
# other member is up the STA dropping and coming back is invisible to netifd: no
# DEV_EVENT_LINK_DOWN, no DEV_EVENT_TOPO_CHANGE, and the dhcp proto is therefore never
# re-run. If the AP moved to a different subnet while we were away, udhcpc keeps counting
# down the dead lease (its renew and rebind packets go to a server that no longer answers),
# so the client holds an address from a subnet that no longer exists until the lease expires
# outright.
#
# This fixes the missing trigger, not the timers: when the supplicant reports CONNECTED, ask
# netifd to renew DHCP on the interfaces that sit on this vif's netdev. netifd already
# supports that (proto dhcp sets renew_handler=1), and a renew is a no-op when the subnet has
# not actually changed.
#
# Invoked by "wpa_cli_s1g -a":  $1 = ifname, $2 = event.
#

IFNAME="$1"
EVENT="$2"

[ -n "$IFNAME" ] || exit 0
[ "$EVENT" = "CONNECTED" ] || exit 0

# A flapping link must not become a DHCP storm: at most one renew per interface per
# MIN_INTERVAL seconds. The uptime clock is monotonic, so a system time jump can neither
# unblock this nor wedge it shut.
MIN_INTERVAL=10
STAMP="/tmp/morse-sta-renew.$IFNAME"

now=$(cut -d. -f1 /proc/uptime)
case "$now" in
	''|*[!0-9]*) exit 0 ;;
esac

if [ -r "$STAMP" ]; then
	read -r last < "$STAMP"
	case "$last" in
		''|*[!0-9]*) last= ;;
	esac
	if [ -n "$last" ] && [ "$now" -ge "$last" ] && [ $((now - last)) -lt "$MIN_INTERVAL" ]; then
		exit 0
	fi
fi
echo "$now" > "$STAMP"

# The networks this vif belongs to, and the netdevs those networks run on.
devices=
for net in $(ubus call network.wireless status 2>/dev/null |
		jsonfilter -q -e "@.*.interfaces[@.ifname='$IFNAME'].config.network[*]"); do
	dev=$(ubus call network.interface."$net" status 2>/dev/null | jsonfilter -q -e '@.device')
	[ -n "$dev" ] && devices="$devices $dev"
done
[ -n "$devices" ] || exit 0

# Renew every netifd interface sitting on one of those netdevs. One logical network can be
# split into several netifd interfaces on the same device (for example one with proto none
# alongside one with proto dhcp), so the vif's own network name is not necessarily the one
# holding the lease. Interfaces whose proto has no renew handler just report an error, which
# is discarded.
for net in $(ubus call network.interface dump 2>/dev/null |
		jsonfilter -q -e '@.interface[*].interface'); do
	dev=$(ubus call network.interface."$net" status 2>/dev/null | jsonfilter -q -e '@.device')
	[ -n "$dev" ] || continue
	for want in $devices; do
		[ "$dev" = "$want" ] || continue
		if ubus call network.interface."$net" renew >/dev/null 2>&1; then
			logger -t morse-sta-action "$IFNAME connected, renewed DHCP on $net ($dev)"
		fi
		break
	done
done

exit 0
