#!/bin/sh

RUN_DIR="/var/run"

TIMEOUT=120

if [ -f $RUN_DIR/ibeacon_payload ]; then
	ibeacon_data="$(cat $RUN_DIR/ibeacon_payload)"

	while true
	do
		(
			echo menu advertise
			echo manufacturer $ibeacon_data
			echo duration $TIMEOUT
			echo back
			echo advertise on
			sleep $TIMEOUT
		) | bluetoothctl
	done
fi
