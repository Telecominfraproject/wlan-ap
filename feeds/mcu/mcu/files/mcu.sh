#!/bin/sh

. /usr/share/libubox/jshn.sh

# Product name and VID:PID used by OpenWrt/OpenWiFi MCUboot fork
MCUBOOT_USB_PRODUCT="MCUboot serial recovery"
MCUBOOT_USB_VID_PID="16c005e1"

MCU_FW_DIR="/lib/firmware/mcu/"
MCU_FLOCK_FILE="/tmp/lock/mcu"

MCU_SYSINFO_OUTPUT=
MCU_IMGLIST_OUTPUT=

MCU_SCRIPT_NAME=""

# logger helpers
mcu_log() {
	if [ -n "$MCU_SCRIPT_NAME" ]; then
		logger -p "$1" -t "${MCU_SCRIPT_NAME}[$$]" "$2"
	else
		logger -p "$1" "$2"
	fi
}

mcu_loge() {
	mcu_log "err" "$1"
}

mcu_logd() {
	mcu_log "debug" "$1"
}

mcu_logi() {
	mcu_log "info" "$1"
}

mcu_logn() {
	mcu_log "notice" "$1"
}

mcu_logw() {
	mcu_log "warn" "$1"
}

_mcu_get_fwlist() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	if [ "$flow" = "1" ]; then
		flow=" -f"
	else
		flow=""
	fi

	[ -n "$baud" ] || baud="115200"

	MCU_IMGLIST_OUTPUT="$(umcumgr -s -d "$uart" -b "$baud$flow" list)"
	[ $? -eq 0 ] || return 1
}

_mcu_get_fwmetadata() {
	local slot="$1"
	local uart="$2"
	local baud="$3"
	local flow="$4"

	local value

	[ -n "$MCU_IMGLIST_OUTPUT" ] || {
		_mcu_get_fwlist "$uart" "$baud" "$flow" || {
			mcu_loge "request 'list' failed (uart='$uart', baud='$baud', flow='$flow')"
			return 1
		}
	}

	if [ "$slot" = "0" ]; then
		slot="slot0_metadata"
	else
		slot="slot1_metadata"
	fi

	value="$(echo "$MCU_IMGLIST_OUTPUT" | grep "$slot=" | cut -d '=' -f 2)"
	echo "$value"
}

_mcu_get_sysinfo() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	if [ "$flow" = "1" ]; then
		flow=" -f"
	else
		flow=""
	fi

	[ -n "$baud" ] || baud="115200"

	MCU_SYSINFO_OUTPUT="$(umcumgr -s -d "$uart" -b "$baud$flow" sysinfo)"
	[ $? -eq 0 ] || return 1
}

_mcu_get() {
	local field="$1"
	local uart="$2"
	local baud="$3"
	local flow="$4"

	local value

	[ -n "$MCU_SYSINFO_OUTPUT" ] || {
		_mcu_get_sysinfo "$uart" "$baud" "$flow" || {
			mcu_loge "request 'sysinfo' failed (uart='$uart', baud='$baud', flow='$flow')"
			return 1
		}
	}

	value="$(echo "$MCU_SYSINFO_OUTPUT" | grep "$field=" | cut -d '=' -f 2)"
	echo "$value"
}

mcu_get_sn() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	local value

	value="$(_mcu_get "serial_num" "$uart" "$baud" "$flow")"
	[ $? -eq 0 ] || return 1

	[ -n "$value" ] && mcu_logd "MCU S/N: '$value'"
	echo "$value"
}

mcu_get_board() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	local value

	value="$(_mcu_get "board" "$uart" "$baud" "$flow")"
	[ $? -eq 0 ] || return 1

	[ -n "$value" ] && mcu_logd "MCU board: '$value'"
	echo "$value"
}

mcu_get_slotsnum() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	local value

	value="$(_mcu_get "single_slot" "$uart" "$baud" "$flow")"
	[ $? -eq 0 ] || return 1

	[ -n "$value" ] || value="1"
	[ "$value" != "1" ] && value="2"

	[ -n "$value" ] && mcu_logd "number of firmware slots: '$value'"
	echo "$value"
}

mcu_get_softver() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	local value

	value="$(_mcu_get "soft_ver" "$uart" "$baud" "$flow")"
	[ $? -eq 0 ] || return 1

	[ -n "$value" ] && mcu_logd "MCUboot version: '$value'"
	echo "$value"
}

mcu_get_activeslot() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	local value

	value="$(_mcu_get "active_slot" "$uart" "$baud" "$flow")"
	[ $? -eq 0 ] || return 1

	[ -n "$value" ] && mcu_logd "active firmware slot: '$value'"
	echo "$value"
}

mcu_get_fwname() {
	local slot="$1"
	local uart="$2"
	local baud="$3"
	local flow="$4"

	local value
	local metadata

	metadata="$(_mcu_get_fwmetadata "$slot" "$uart" "$baud" "$flow")"
	[ $? -eq 0 ] || return 1

	[ -n "$metadata" ] && {
		json_load "$metadata"
		json_get_var value fw_name
	}

	if [ -n "$value" ]; then
		mcu_logi "firmware installed in slot '$slot': '$value'"
	else
		mcu_logw "no firmware installed in slot '$slot'"
	fi

	echo "$value"
}

mcu_req_boot() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	if [ "$flow" = "1" ]; then
		flow=" -f"
	else
		flow=""
	fi

	[ -n "$baud" ] || baud="115200"

	umcumgr -s -d "$uart" -b "$baud$flow" boot || {
		mcu_loge "request 'boot' failed"
		return 1
	}

	mcu_logi "MCU requested to boot the firmware"
}

mcu_req_reset() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	if [ "$flow" = "1" ]; then
		flow=" -f"
	else
		flow=""
	fi

	[ -n "$baud" ] || baud="115200"

	# Request warm reset of the MCU
	umcumgr -s -d "$uart" -b "$baud$flow" reset || {
		mcu_loge "request 'reset' failed"
		return 1
	}

	mcu_logi "MCU requested to reset"
}

mcu_sel_slot() {
	local slot="$1"
	local uart="$2"
	local baud="$3"
	local flow="$4"

	if [ "$flow" = "1" ]; then
		flow=" -f"
	else
		flow=""
	fi

	[ -n "$baud" ] || baud="115200"

	# Request firmware active slot change
	umcumgr -s -d "$uart" -b "$baud$flow" select "$slot" || {
		mcu_loge "request 'select slot' failed"
		return 1
	}

	mcu_logi "active firmware slot changed to: '$slot'"
}

mcu_fw_upload() {
	local board="$1"
	local slot="$2"
	local fw_name="$3"
	local uart="$4"
	local baud="$5"
	local flow="$6"

	local fw_path

	if [ "$flow" = "1" ]; then
		flow=" -f"
	else
		flow=""
	fi

	[ -n "$baud" ] || baud="115200"

	fw_path="${MCU_FW_DIR}/${board}/${fw_name}/slot${slot}.bin"
	umcumgr -q info "$fw_path" > /dev/null 2>&1 || {
		mcu_loge "invalid or missing firmware file: '$fw_path'"
		return 1
	}

	mcu_logi "uploading '$fw_name' to slot: '$slot'..."

	# Upload fw to selected slot (TODO: slots numbering Zephyr vs. MCUboot)
	[ "$slot" = "1" ] && slot="2"
	umcumgr -q -n "$slot" -d "$uart" -b "$baud$flow" upload "$fw_path" || {
		mcu_loge "request 'upload' failed"
		return 1
	}

	mcu_logi "firmware uploaded!"
}

mcu_enable_pin_set() {
	local gpio="$1"
	local gpio_value="$2"

	mcu_logd "setting MCU enable_pin '$(basename "$gpio")' to '$gpio_value'"
	echo "$gpio_value" > "${gpio}/value" 2>/dev/null
}

mcu_sn_check_and_update() {
	local sn="$1"
	local uart="$2"
	local baud="$3"
	local flow="$4"

	local sn_dev

	if [ "$flow" = "1" ]; then
		flow=" -f"
	else
		flow=""
	fi

	[ -n "$baud" ] || baud="115200"

	sn_dev="$(mcu_get_sn "$uart" "$baud" "$flow")"
	[ -n "$sn_dev" ] || return 1

	[ -n "$sn_dev" ] && {
		if [ -z "$sn" ]; then
			[ -d /etc/config-shadow ] && {
				uci -c /etc/config-shadow -q set mcu.${SECT}.sn="$sn_dev"
				uci -c /etc/config-shadow -q commit mcu
			}

			uci -q set mcu.${SECT}.sn="$sn_dev"
			uci -q commit mcu
		else
			[ "$sn" != "$sn_dev" ] && {
				mcu_loge "MCU S/N mismatch ('$sn_dev' != '$sn')!"
				return 1
			}
		fi
	}

	return 0
}

mcu_fw_check_and_update() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	local active_slot
	local fw_slots
	local slot0_fw
	local slot1_fw
	local firmware
	local board

	config_get firmware "$SECT" firmware
	[ -n "$firmware" ] || mcu_logw "option 'firmware' unset"

	# MCU board name
	board="$(mcu_get_board "$uart" "$baud" "$flow")"
	[ $? -eq 0 ] || return 1

	# Number of firmware slots and active slot
	fw_slots="$(mcu_get_slotsnum "$uart" "$baud" "$flow")"
	[ $? -eq 0 ] || return 1
	[ -n "$fw_slots" ] || fw_slots="1"

	[ "$fw_slots" = "2" ] && {
		active_slot="$(mcu_get_activeslot "$uart" "$baud" "$flow")"
		[ $? -eq 0 ] || return 1
	}
	[ -n "$active_slot" ] || active_slot="0"

	# Firmware available?
	[ -n "$firmware" ] && {
		if [ "$fw_slots" = "2" ]; then
			[ -f "${MCU_FW_DIR}/${board}/${firmware}/slot0.bin" -a \
			  -f "${MCU_FW_DIR}/${board}/${firmware}/slot1.bin" ] || {
				mcu_loge "firmware '$firmware' doesn't exist"
				return 1
			}
		else
			[ -f "${MCU_FW_DIR}/${board}/${firmware}/slot0.bin" ] || {
				mcu_loge "firmware '$firmware' doesn't exist"
				return 1
			}
		fi
	}

	slot0_fw="$(mcu_get_fwname "0" "$uart" "$baud" "$flow")"
	[ $? -eq 0 ] || return 1

	[ "$fw_slots" = "2" ] && {
		slot1_fw="$(mcu_get_fwname "1" "$uart" "$baud" "$flow")"
		[ $? -eq 0 ] || return 1
	}

	# No target firmware provided, check what's on device and update config
	[ -n "$firmware" ] || {
		firmware="$slot0_fw"
		[ "$active_slot" = "1" ] && firmware="$slot1_fw"

		[ -n "$firmware" ] && {
			[ -d /etc/config-shadow ] && {
				uci -c /etc/config-shadow -q set mcu.${SECT}.firmware="$firmware"
				uci -c /etc/config-shadow -q commit mcu
			}

			uci -q set mcu.${SECT}.firmware="$firmware"
			uci -q commit mcu

			mcu_req_boot "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1
		}

		return 0
	}

	# Do we have target firmware installed in the first slot?
	[ "$firmware" = "$slot0_fw" ] && {
		mcu_logd "found matching firmware installed in slot '0'"

		if [ "$fw_slots" = "2" -a "$active_slot" != "0" ]; then
			mcu_sel_slot "0" "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1

			# Changing active slots requires MCU reset at the moment
			mcu_req_reset "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1
		else
			mcu_req_boot "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1
		fi

		return 0
	}

	# Upload and boot firmware on single-slot device
	[ "$fw_slots" = "1" ] && {
		mcu_fw_upload "$board" "0" "$firmware" "$uart" "$baud" "$flow"
		[ $? -ne 0 ] && return 1

		mcu_req_boot "$uart" "$baud" "$flow"
		[ $? -ne 0 ] && return 1

		return 0
	}

	# Do we have target firmware installed in the second slot?
	[ "$firmware" = "$slot1_fw" ] && {
		mcu_logd "found matching firmware installed in slot '1'"

		if [ "$active_slot" != "1" ]; then
			mcu_sel_slot "1" "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1

			# Changing active slots requires MCU reset at the moment
			mcu_req_reset "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1
		else
			mcu_req_boot "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1
		fi

		return 0
	}

	# Upload and boot firmware on multi-slot device
	# Always use inactive slot
	if [ "$active_slot" = "0" ]; then
		active_slot="1"
	else
		active_slot="0"
	fi

	mcu_fw_upload "$board" "$active_slot" "$firmware" "$uart" "$baud" "$flow"
	[ $? -ne 0 ] && return 1

	mcu_sel_slot "$active_slot" "$uart" "$baud" "$flow"
	[ $? -ne 0 ] && return 1

	# Changing active slots requires MCU reset at the moment
	mcu_req_reset "$uart" "$baud" "$flow"
	[ $? -ne 0 ] && return 1

	return 0
}
