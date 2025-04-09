. /lib/functions/system.sh

RAMFS_COPY_BIN='fw_printenv fw_setenv'
RAMFS_COPY_DATA='/etc/fw_env.config /var/lock/fw_printenv.lock'

find_mmc_part() {
	local DEVNAME PARTNAME

	if grep -q "$1" /proc/mtd; then
		echo "" && return 0
	fi

	for DEVNAME in /sys/block/mmcblk*/mmcblk*p*; do
		PARTNAME=$(grep PARTNAME ${DEVNAME}/uevent | cut -f2 -d'=')
		[ "$PARTNAME" = "$1" ] && echo "/dev/$(basename $DEVNAME)" && return 0
	done
}

do_flash_emmc() {
	local tar_file=$1
	local emmcblock=$(find_mmc_part $2)
	local board_dir=$3
	local part=$4

	[ -b "$emmcblock" ] || emmcblock=$(find_mmc_part $2)

	[ -z "$emmcblock" ] && {
		echo failed to find $2
		return
	}

	echo erase $4 / $emmcblock
	dd if=/dev/zero of=${emmcblock} 2> /dev/null
	echo flash $4
	tar Oxf $tar_file ${board_dir}/$part | dd of=${emmcblock}
}

sonicfi_dualimage_check() {
	local boot_part=""
	boot_part=$(fw_printenv | grep bootfrom | awk -F'=' '{printf $2}')
	[ -n "$boot_part" ] || boot_part="0"
	echo "boot_part=$boot_part" > /dev/console

	if [ "$boot_part" = "0" ]; then
		block_kernel="0:HLOS_1"
		block_rootfs="rootfs_1"
		CI_UBIPART="rootfs_1"
		fw_setenv bootfrom 1
	elif [ "$boot_part" = "1" ]; then
		block_kernel="0:HLOS"
		block_rootfs="rootfs"
		CI_UBIPART="rootfs"
		fw_setenv bootfrom 0
	else
		echo "Invalid boot partition $boot_part! Skip upgrade....."
		return
	fi
}

emmc_do_upgrade() {
	local tar_file="$1"
	local board_dir=$(tar tf $tar_file | grep -m 1 '^sysupgrade-.*/$')
	board_dir=${board_dir%/}
	echo "block_kernel=$block_kernel, block_rootfs=$block_rootfs" > /dev/console
	do_flash_emmc $tar_file $block_kernel $board_dir kernel
	do_flash_emmc $tar_file $block_rootfs $board_dir root

	local emmcblock="$(find_mmc_part "rootfs_data")"
	if [ -e "$emmcblock" ]; then
		mkfs.ext4 -F "$emmcblock"
	fi
}

platform_check_image() {
	local magic_long="$(get_magic_long "$1")"
	[ "$magic_long" = "73797375" ] && return 0
	return 1
}

platform_do_upgrade() {
	CI_UBIPART="rootfs"
	CI_ROOTPART="ubi_rootfs"
	CI_IPQ807X=1
	block_kernel="0:HLOS"
	block_rootfs="rootfs"

	board=$(board_name)
	case $board in
	cig,wf189w|\
	cig,wf189)
		if [ -f /proc/boot_info/bootconfig0/rootfs/upgradepartition ]; then
			CI_UBIPART="$(cat /proc/boot_info/bootconfig0/rootfs/upgradepartition)"
			CI_BOOTCFG=1
		elif [ -f /proc/boot_info/bootconfig1/rootfs/upgradepartition ]; then
			CI_UBIPART="$(cat /proc/boot_info/bootconfig1/rootfs/upgradepartition)"
			CI_BOOTCFG=1
		fi
		nand_upgrade_tar "$1"
		;;
	edgecore,eap105)
		if [ "$(find_mtd_chardev rootfs)" ]; then
			CI_UBIPART="rootfs"
		else
			if grep -q rootfs1 /proc/cmdline; then
				CI_UBIPART="rootfs2"
				CI_FWSETENV="active 2"
			else
				CI_UBIPART="rootfs1"
				CI_FWSETENV="active 1"
			fi
		fi
		nand_upgrade_tar "$1"
		;;
	sonicfi,rap7110c-341x)
		sonicfi_dualimage_check
		emmc_do_upgrade $1 $1
		;;
	sonicfi,rap750w-311a)
		sonicfi_dualimage_check
		nand_upgrade_tar "$1"
		;;
	esac
}
