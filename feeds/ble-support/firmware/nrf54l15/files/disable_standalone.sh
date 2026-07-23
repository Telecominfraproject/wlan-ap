#!/bin/sh

# Called by wifi_provision when the app sends the "disable_standalone" command.
# Restores ucentral and cloud_discovery to normal operation.
# The reboot is the commit point - flag removal is last before reboot
# to avoid a broken state on power loss.

BLOCK_FLAG="/etc/ucentral/.ble_provisioned"

# Not blocked - nothing to do
[ -f "$BLOCK_FLAG" ] || exit 0

logger -t disable-standalone "BLE app triggered: re-enabling ucentral"

# Restore the init script
[ -f /etc/init.d/ucentral.disabled ] && mv /etc/init.d/ucentral.disabled /etc/init.d/ucentral

# Re-enable boot startup
/etc/init.d/ucentral enable

# Re-enable cloud_discovery
[ -f /etc/init.d/cloud_discovery ] && /etc/init.d/cloud_discovery enable

# Remove the flag and reboot atomically - if power is lost before reboot,
# the flag is already gone but services are re-enabled, so next boot is clean.
rm -f "$BLOCK_FLAG"
sync
reboot
