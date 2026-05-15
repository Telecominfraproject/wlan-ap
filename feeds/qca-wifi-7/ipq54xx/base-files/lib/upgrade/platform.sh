. /lib/functions/system.sh

RAMFS_COPY_BIN='fw_printenv fw_setenv dumpimage'
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

do_flash_bootconfig_emmc() {
	local bin=$1
	local emmcblock=$2

	dd if=/dev/zero of=${emmcblock} &> /dev/null
	dd if=/tmp/${bin}.bin of=${emmcblock}
}

do_flash_partition() {
	local bin=$1
	local mtdname=$2
	local emmcblock="$(find_mmc_part "$mtdname")"

	if [ -e "$emmcblock" ]; then
		do_flash_bootconfig_emmc $bin $emmcblock
	fi
}

do_flash_bootconfig() {
	local mtdname=$1
	local bin=bootconfig

	#flash bootconfig with updated boot-info
	if [ -f /tmp/bootconfig.bin ]; then
		do_flash_partition $bin $mtdname
	else
		echo " Bootconfig binary is missing.... "
		return 1
	fi
}

get_upgrade_bank() {
	local mtdname=$1
	local boot_set=$(grep "Boot-set" /tmp/bootconfig_members.txt | awk -F: '{print $2}')
	local image_status=$(grep "Image-set-status" /tmp/bootconfig_members.txt | awk -F: '{print $2}')
	local current_bank=0

	if [ "$boot_set" -eq 0 ] && [ "$image_status" -ne 1 ]; then
		mtdname="${mtdname}_1"
		current_bank=1
	elif [ "$boot_set" -eq 1 ] && [ "$image_status" -eq 2 ]; then
		mtdname="${mtdname}_1"
		current_bank=1
	fi

	if [[ -z "$mtdname" || "$mtdname" == "_1" ]]; then
		echo $current_bank
	else
		echo $mtdname
	fi
}

spi_nor_emmc_do_upgrade_bootconfig() {
	local tar_file="$1"

	local board_dir=$(tar tf $tar_file | grep -m 1 '^sysupgrade-.*/$')
	board_dir=${board_dir%/}

	CI_ROOTPART="$(get_upgrade_bank "rootfs")"
	CI_KERNPART="$(get_upgrade_bank "0:HLOS")"

	[ -n "$CI_KERNPART" -a -n "$CI_ROOTPART" ] || {
		echo "kernel or rootfs partition is unknown"
		exit
	}

	do_flash_emmc $tar_file $CI_KERNPART $board_dir kernel
	do_flash_emmc $tar_file $CI_ROOTPART $board_dir root

	local emmcblock="$(find_mmc_part "rootfs_data")"
	if [ -e "$emmcblock" ]; then
		mkfs.ext4 -F "$emmcblock"
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

extract_bootconfig() {
	local mtdname=$1
	local mtdpart=$(grep "\"${mtdname}\"" /proc/mtd | awk -F: '{print $1}')
	local emmcblock="$(find_mmc_part "$mtdname")"

	if [ -e "$emmcblock" ]; then
		dd if=${emmcblock} of=/tmp/bootconfig.bin
	else
		dd if=/dev/${mtdpart} of=/tmp/bootconfig.bin
	fi
}

platform_do_upgrade() {
	CI_UBIPART="rootfs"
	CI_ROOTPART="ubi_rootfs"
	CI_IPQ807X=1
	block_kernel="0:HLOS"
	block_rootfs="rootfs"

	extract_bootconfig "0:BOOTCONFIG"
	# Parse bootconfig members and update /tmp/bootconfig_members.txt
	dumpimage -b 5

	if [[ "$?" == 1 ]];then
		echo "bootconfig functionality failed, rebooting.."
		return 1
	fi

	board=$(board_name)
	case $board in
	cig,wf197)
		spi_nor_emmc_do_upgrade_bootconfig $1
		;;
	esac
	# Finalize bootconfig and prepare for flashing
	dumpimage -b 0
	do_flash_bootconfig "0:BOOTCONFIG"
}
