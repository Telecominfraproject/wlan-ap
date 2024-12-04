. /lib/functions/system.sh

RAMFS_COPY_BIN='fw_printenv fw_setenv'
RAMFS_COPY_DATA='/etc/fw_env.config /var/lock/fw_printenv.lock'

platform_check_image() {
	local magic_long="$(get_magic_long "$1")"
	board=$(board_name)
	case $board in
	qcom,rdp433|\
	prpl,freedom)
		[ "$magic_long" = "73797375" ] && return 0
		;;
	esac
	return 1
}

platform_do_upgrade() {
	board=$(board_name)
	case $board in
	qcom,rdp433|\
	prpl,freedom)
		kernelname="0:HLOS"
		rootfsname="rootfs"
		mmc_do_upgrade "$1"
		;;
	esac
}
