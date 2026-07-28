#!/bin/sh

# Called by wifi_provision when the app sends the "enable_standalone" command.
# Stops ucentral and ensures it stays dead across reboots.
# Only a factory reset (firstboot -y -r) or "disable_standalone" restores ucentral.

BLOCK_FLAG="/etc/ucentral/.ble_provisioned"

# Already done - nothing to do
[ -f "$BLOCK_FLAG" ] && exit 0

logger -t enable-standalone "BLE app triggered: stopping and disabling ucentral"

# Write flag first - if power is lost after this but before mv,
# the flag exists harmlessly (ucentral still starts). On retry,
# the script will see the flag and exit. disable_standalone.sh can
# always restore from this state.
touch "$BLOCK_FLAG"
sync

# Stop the running service
/etc/init.d/ucentral stop

# Disable boot startup (removes /etc/rc.d/S99ucentral)
/etc/init.d/ucentral disable

# Move the init script so nothing (cloud_discovery, etc.) can restart it
[ -f /etc/init.d/ucentral ] && mv /etc/init.d/ucentral /etc/init.d/ucentral.disabled

# Stop cloud_discovery - it would otherwise restart ucentral
[ -f /etc/init.d/cloud_discovery ] && {
	/etc/init.d/cloud_discovery stop
	/etc/init.d/cloud_discovery disable
}
