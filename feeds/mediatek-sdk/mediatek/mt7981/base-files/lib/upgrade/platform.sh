REQUIRE_IMAGE_METADATA=1
platform_do_upgrade() {
	local board=$(board_name)

	case "$board" in
	edgecore,eap111)
		if [ -e /tmp/downgrade ]; then
			CI_UBIPART="rootfs1"
			{ echo 'active 1'; echo 'upgrade_available 0'; } > /tmp/fw_setenv.txt || exit 1
			CI_FWSETENV="-s /tmp/fw_setenv.txt"
		else
			local CI_UBIPART_B=""
			if grep -q rootfs1 /proc/cmdline; then
				CI_UBIPART="rootfs2"
				CI_UBIPART_B="rootfs1"
				CI_FWSETENV="active 2"
			elif grep -q rootfs2 /proc/cmdline; then
				CI_UBIPART="rootfs1"
				CI_UBIPART_B="rootfs2"
				CI_FWSETENV="active 1"
			else
				CI_UBIPART="rootfs1"
				CI_UBIPART_B=""
				CI_FWSETENV="active 1"
			fi
			if [ "$(fw_printenv -n upgrade_available 2>/dev/null)" = "0" ]; then
				if [ -n "$CI_UBIPART_B" ]; then
					CI_UBIPART="$CI_UBIPART_B"
					CI_FWSETENV=""
				fi
			fi
		fi
		nand_do_upgrade "$1"
		;;
	esac
}

platform_check_image() {
	local board=$(board_name)
	local magic="$(get_magic_long "$1")"

	[ "$#" -gt 1 ] && return 1

	case "$board" in
	edgecore,eap111)
		nand_do_platform_check "$board" "$1"
		return $?
		;;
	esac

	return 0
}
