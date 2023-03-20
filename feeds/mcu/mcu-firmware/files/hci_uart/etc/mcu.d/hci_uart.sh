#!/bin/sh

. /lib/functions.sh
. /lib/functions/mcu.sh

attach_hci_controller() {
	local section="$1"

	local sn
	local pid
	local interface
	local uart_path
	local uart_baud
	local uart_flow

	[ -n "$section" ] || return 1
	command -v btattach > /dev/null 2>&1 || return 1

	config_load mcu
	config_get sn "$section" sn
	config_get interface "$section" interface
	config_get uart_path "$section" uart_path
	config_get uart_baud "$section" uart_baud "115200"
	config_get uart_flow "$section" uart_flow "0"

	[ -n "$sn" ] || return 1

	if [ "$interface" = "usb" ]; then
		uart_baud="1000000"
		uart_flow="1"

		dev_found=""
		usb_path="/sys/bus/usb/devices/*"
		for dev_path in $usb_path; do
			dev="$(basename "$dev_path")"
			[[ $dev == *":"* ]] && continue

			[ "$sn" = "$(cat "${dev_path}/serial" 2>/dev/null)" ] && {
				dev_found="$dev"
				break
			}
		done

		[ -n "$dev_found" ] || return 1

		usb_path="/sys/bus/usb/devices/${dev_found}*/tty/*"
		for tty_path in $usb_path; do
			tty="$(basename "$tty_path")"
			[ -c "/dev/${tty}" ] && {
				uart_path="/dev/${tty}"
				break
			}
		done
	fi

	[ -c "$uart_path" ] || return 1

	# Give MCU some time for BLE controller setup
	sleep 1

	if [ "$uart_flow" = "1" ]; then
		btattach -B "$uart_path" -S "$uart_baud" > /dev/null 2>&1 &
	else
		btattach -B "$uart_path" -S "$uart_baud" -N > /dev/null 2>&1 &
	fi

	pid="$!"

	kill -0 "$pid" > /dev/null 2>&1 && {
		echo "$pid" > "/var/run/mcu.${sn}.pid"
		return 0
	}

	return 1
}

attach_hci_controller "$1" || exit 1

exit 0
