RAMFS_COPY_BIN='mkfs.f2fs blkid blockdev fw_printenv fw_setenv dmsetup'
RAMFS_COPY_DATA="/etc/fw_env.config /var/lock/fw_printenv.lock"

senao_swap_active_fw() {
	echo "Doing swap active_fw" > /dev/console
	tmp_active_fw=$(fw_printenv -n active_fw)
	if [ $tmp_active_fw == "0" ]; then
		fw_setenv active_fw 1
		fw_setenv mtdparts nmbm0:1024k\(bl2\),512k\(u-boot-env\),2048k\(factory\),2048k\(fip\),112640k\(ubi_1\),112640k\(ubi\),384k\(cert\),640k\(userconfig\),384k\(crashdump\)
	else
		fw_setenv active_fw 0
		fw_setenv mtdparts nmbm0:1024k\(bl2\),512k\(u-boot-env\),2048k\(factory\),2048k\(fip\),112640k\(ubi\),112640k\(ubi_1\),384k\(cert\),640k\(userconfig\),384k\(crashdump\)
	fi
}

platform_do_upgrade() {
	local board=$(board_name)

	case "$board" in
	*snand*)
		ubi_do_upgrade "$1"
		;;
	*emmc*)
		mtk_mmc_do_upgrade "$1"
		;;
	senao,iap4300m)
		CI_UBIPART="ubi_1"
		nand_do_upgrade "$1"
		;;
	*)
		default_do_upgrade "$1"
		;;
	esac
}

PART_NAME=firmware

platform_check_image() {
	local board=$(board_name)
	local magic="$(get_magic_long "$1")"

	[ "$#" -gt 1 ] && return 1

	case "$board" in
	senao,iap4300m |\
	*snand* |\
	*emmc*)
		# tar magic `ustar`
		magic="$(dd if="$1" bs=1 skip=257 count=5 2>/dev/null)"

		[ "$magic" != "ustar" ] && {
			echo "Invalid image type."
			return 1
		}

		return 0
		;;
	*)
		[ "$magic" != "d00dfeed" ] && {
			echo "Invalid image type."
			return 1
		}
		return 0
		;;
	esac

	return 0
}

platform_post_upgrade_success() {
	local board=$(board_name)

	case "$board" in
		senao,iap4300m)
			senao_swap_active_fw
		;;
	esac
}
