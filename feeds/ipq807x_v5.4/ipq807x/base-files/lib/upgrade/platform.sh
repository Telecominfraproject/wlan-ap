. /lib/functions/system.sh

RAMFS_COPY_BIN='fw_printenv fw_setenv'
RAMFS_COPY_DATA='/etc/fw_env.config /var/lock/fw_printenv.lock'

qca_do_upgrade() {
	local tar_file="$1"

	local board_dir=$(tar tf $tar_file | grep -m 1 '^sysupgrade-.*/$')
	board_dir=${board_dir%/}
	local dev=$(find_mtd_chardev "0:HLOS")

	tar Oxf $tar_file ${board_dir}/kernel | mtd write - ${dev}

	if [ -n "$UPGRADE_BACKUP" ]; then
		tar Oxf $tar_file ${board_dir}/root | mtd -j "$UPGRADE_BACKUP" write - rootfs
	else
		tar Oxf $tar_file ${board_dir}/root | mtd write - rootfs
	fi
}

platform_check_image() {
	local magic_long="$(get_magic_long "$1")"
	board=$(board_name)
	case $board in
	cig,wf194c4|\
	cig,wf196|\
	edgecore,eap102|\
	edgecore,oap102|\
	edgecore,oap103|\
	edgecore,eap106|\
	sonicfi,rap650c|\
	tplink,ex227|\
	tplink,ex447)
		[ "$magic_long" = "73797375" ] && return 0
		;;
	esac
	return 1
}

platform_do_upgrade() {
	CI_UBIPART="rootfs"
	CI_ROOTPART="ubi_rootfs"
	CI_IPQ807X=1

	board=$(board_name)
	case $board in
	tplink,ex227)	
		qca_do_upgrade "$1"
		;;
        cig,wf196)
                [ -f /proc/boot_info/rootfs/upgradepartition ] && {
                        CI_UBIPART="$(cat /proc/boot_info/rootfs/upgradepartition)"
                        CI_BOOTCFG=1
                }
                nand_upgrade_tar "$1"
                ;;
	cig,wf194c4|\
	tplink,ex447)
		nand_upgrade_tar "$1"
		;;
	edgecore,eap106)
		CI_UBIPART="rootfs1"
		[ "$(find_mtd_chardev rootfs)" ] && CI_UBIPART="rootfs"
		nand_upgrade_tar "$1"
		;;
	edgecore,eap102|\
	edgecore,oap102|\
	edgecore,oap103)
		if [ "$(find_mtd_chardev rootfs)" ]; then
			CI_UBIPART="rootfs"
		else
			if [ -e /tmp/downgrade ]; then
				CI_UBIPART="rootfs1"
				{ echo 'active 1'; echo 'upgrade_available 0'; } > /tmp/fw_setenv.txt || exit 1
				CI_FWSETENV="-s /tmp/fw_setenv.txt"
			elif grep -q rootfs1 /proc/cmdline; then
				CI_UBIPART="rootfs2"
				CI_FWSETENV="active 2"
			else
				CI_UBIPART="rootfs1"
				CI_FWSETENV="active 1"
			fi
		fi
		nand_upgrade_tar "$1"
		;;
	sonicfi,rap650c)
		boot_part=$(fw_printenv -n bootfrom)
		[ ${#boot_part} -eq 0 ] && boot_part=0
		echo "Current bootfrom is $boot_part"
		if [[ $boot_part == 1 ]]; then
			CI_UBIPART="rootfs"
			CI_FWSETENV="bootfrom 0"
		elif [[ $boot_part == 0 ]]; then
			CI_UBIPART="rootfs_1"
			CI_FWSETENV="bootfrom 1"
		fi
		nand_upgrade_tar "$1"
	esac
}
