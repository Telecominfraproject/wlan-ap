#!/bin/sh

. /usr/share/libubox/jshn.sh

# Product name and VID:PID used by OpenWrt/OpenWiFi MCUboot fork
MCUBOOT_USB_PRODUCT="MCUboot serial recovery"
MCUBOOT_USB_VID_PID="16c005e1"

# Host support and firmware directories
MCU_HS_DIR="/etc/mcu.d"
MCU_FW_DIR="/lib/firmware/mcu"

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

mcu_fetch_fwlist() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	if [ "$flow" = "1" ]; then
		flow=" -f"
	else
		flow=""
	fi

	[ -n "$baud" ] || baud="115200"

	[ -n "$MCU_IMGLIST_OUTPUT" ] && return 0

	MCU_IMGLIST_OUTPUT="$(umcumgr -s -d "$uart" -b "$baud$flow" list)"
	[ $? -eq 0 ] || {
		mcu_loge "request 'list' failed (uart='$uart', baud='$baud', flow='$flow')"
		return 1
	}
}

mcu_fetch_sysinfo() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	if [ "$flow" = "1" ]; then
		flow=" -f"
	else
		flow=""
	fi

	[ -n "$baud" ] || baud="115200"

	[ -n "$MCU_SYSINFO_OUTPUT" ] && return 0

	MCU_SYSINFO_OUTPUT="$(umcumgr -s -d "$uart" -b "$baud$flow" sysinfo)"
	[ $? -eq 0 ] || {
		mcu_loge "request 'sysinfo' failed (uart='$uart', baud='$baud', flow='$flow')"
		return 1
	}
}

mcu_get() {
	local param="$1"
	local slot="$2"

	local value
	local metadata
	local sysinfo_field

	case "$param" in
	"board"|\
	"soft_ver"|\
	"serial_num"|\
	"active_slot")
		sysinfo_field="$param"
		;;
	"slots_num")
		sysinfo_field="single_slot"
		;;
	"fwname")
		[ -n "$slot" ] || return 1
		param="image list: slot${slot} fw_name"

		metadata="$(echo "$MCU_IMGLIST_OUTPUT" | grep "slot${slot}_metadata=" | cut -d '=' -f 2)"
		[ -n "$metadata" ] && {
			json_load "$metadata"
			json_get_var value fw_name
		}
		;;
	"fwsha")
		[ -n "$slot" ] || return 1
		param="image list: slot${slot}_hash"

		value="$(echo "$MCU_IMGLIST_OUTPUT" | grep "slot${slot}_hash=" | cut -d '=' -f 2)"
		;;
	*)
		return 1
		;;
	esac

	[ -n "$sysinfo_field" ] && {
		value="$(echo "$MCU_SYSINFO_OUTPUT" | grep "${sysinfo_field}=" | cut -d '=' -f 2)"

		[ "$sysinfo_field" = "single_slot" ] && {
			[ -n "$value" ] || value="1"
			[ "$value" != "1" ] && value="2"
		}

		param="sysinfo: $param"
	}

	[ -n "$value" ] && mcu_logd "$param: '$value'"
	echo "$value"
}

mcu_req() {
	local cmd="$1"
	local uart="$2"
	local baud="$3"
	local flow="$4"

	case "$cmd" in
	"boot"|\
	"reset")
		;;
	*)
		return 1
		;;
	esac

	if [ "$flow" = "1" ]; then
		flow=" -f"
	else
		flow=""
	fi

	[ -n "$baud" ] || baud="115200"

	umcumgr -s -d "$uart" -b "$baud$flow" "$cmd" || {
		mcu_loge "request '$cmd' failed (uart='$uart', baud='$baud', flow='$flow')"
		return 1
	}

	mcu_logi "MCU requested '$cmd'"
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
		mcu_loge "request 'select slot' failed (uart='$uart', baud='$baud', flow='$flow')"
		return 1
	}

	mcu_logi "active firmware slot changed to: '$slot'"
}

mcu_fwfile_sha() {
	local path="$1"

	local value

	[ -f "$path" ] || return 1

	value="$(umcumgr -s hash "$path" | grep "hash=" | cut -d '=' -f 2)"
	[ -n "$value" ] || return 1

	echo "$value"
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
		mcu_loge "request 'upload' failed (uart='$uart', baud='$baud', flow='$flow')"
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

	# Fetch sysinfo
	mcu_fetch_sysinfo "$uart" "$baud" "$flow" || return 1

	sn_dev="$(mcu_get "serial_num")"
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

# Returns:
# 0 if MCU was requested to boot firmware
# 1 on error
# 2 if MCU was requested to reset
mcu_fw_check_and_update() {
	local uart="$1"
	local baud="$2"
	local flow="$3"

	local active_slot
	local fw_slots
	local slot0_fw
	local slot0_sha
	local slot1_fw
	local slot1_sha
	local firmware
	local fw0_sha
	local fw1_sha
	local board
	local soft_ver

	config_get firmware "$SECT" firmware
	[ -n "$firmware" ] || mcu_logw "option 'firmware' unset"

	# Fetch sysinfo and firmware images list
	mcu_fetch_sysinfo "$uart" "$baud" "$flow" || return 1
	mcu_fetch_fwlist "$uart" "$baud" "$flow" || return 1

	# MCU board name and software version
	board="$(mcu_get "board")"
	[ $? -eq 0 ] || return 1

	soft_ver="$(mcu_get "soft_ver")"
	[ $? -eq 0 ] || return 1

	# Number of firmware slots and active slot
	fw_slots="$(mcu_get "slots_num")"
	[ $? -eq 0 ] || return 1
	[ -n "$fw_slots" ] || fw_slots="1"

	[ "$fw_slots" = "2" ] && {
		active_slot="$(mcu_get "active_slot")"
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

			fw1_sha="$(mcu_fwfile_sha "${MCU_FW_DIR}/${board}/${firmware}/slot1.bin")"
		else
			[ -f "${MCU_FW_DIR}/${board}/${firmware}/slot0.bin" ] || {
				mcu_loge "firmware '$firmware' doesn't exist"
				return 1
			}
		fi

		fw0_sha="$(mcu_fwfile_sha "${MCU_FW_DIR}/${board}/${firmware}/slot0.bin")"
	}

	slot0_fw="$(mcu_get "fwname" "0")"
	[ $? -eq 0 ] || return 1

	[ -n "$slot0_fw" ] && {
		slot0_sha="$(mcu_get "fwsha" "0")"
		[ $? -eq 0 ] || return 1
	}

	[ "$fw_slots" = "2" ] && {
		slot1_fw="$(mcu_get "fwname" "1")"
		[ $? -eq 0 ] || return 1

		[ -n "$slot1_fw" ] && {
			slot1_sha="$(mcu_get "fwsha" "1")"
			[ $? -eq 0 ] || return 1
		}
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

			mcu_req "boot" "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1

			return 0
		}

		return 1
	}

	# Do we have target firmware installed in the first slot?
	[ "$firmware" = "$slot0_fw" -a "$slot0_sha" = "$fw0_sha" ] && {
		mcu_logi "found matching firmware installed in slot '0'"

		if [ "$fw_slots" = "2" -a "$active_slot" != "0" ]; then
			mcu_sel_slot "0" "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1

			# Changing active slots requires MCU reset at the moment
			mcu_req "reset" "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1

			return 2
		else
			mcu_req "boot" "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1
		fi

		return 0
	}

	mcu_logi "no matching firmware found in slot '0'"

	# Upload firmware and reset on single-slot device
	[ "$fw_slots" = "1" ] && {
		mcu_fw_upload "$board" "0" "$firmware" "$uart" "$baud" "$flow"
		[ $? -ne 0 ] && return 1

		mcu_req "reset" "$uart" "$baud" "$flow"
		[ $? -ne 0 ] && return 1

		return 2
	}

	# Do we have target firmware installed in the second slot?
	[ "$firmware" = "$slot1_fw" -a "$slot1_sha" = "$fw1_sha" ] && {
		mcu_logi "found matching firmware installed in slot '1'"

		if [ "$active_slot" != "1" ]; then
			mcu_sel_slot "1" "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1

			# Changing active slots requires MCU reset at the moment
			mcu_req "reset" "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1

			return 2
		else
			mcu_req "boot" "$uart" "$baud" "$flow"
			[ $? -ne 0 ] && return 1
		fi

		return 0
	}

	mcu_logi "no matching firmware found in slot '1'"

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
	mcu_req "reset" "$uart" "$baud" "$flow"
	[ $? -ne 0 ] && return 1

	return 2
}

mcu_add_uci_config() {
	local name="$1"
	local interface="$2"
	local bootloader="$3"
	local firmware="$4"
	local enable_pin="$5"
	local uart_path="$6"
	local uart_baud="$7"
	local uart_flow="$8"

	uci -q set mcu.${name}="mcu"
	uci -q set mcu.${name}.interface="$interface"
	uci -q set mcu.${name}.bootloader="$bootloader"
	uci -q set mcu.${name}.firmware="$firmware"

	[ -n "$enable_pin" ] && uci -q set mcu.${name}.enable_pin="$enable_pin"
	[ -n "$uart_path" ] && uci -q set mcu.${name}.uart_path="$uart_path"
	[ -n "$uart_baud" ] && uci -q set mcu.${name}.uart_baud="$uart_baud"

	[ "$uart_flow" = "1" ] && uci -q set mcu.${name}.uart_flow="1"

	uci -q set mcu.${name}.disabled="0"

	uci -q commit mcu
}
