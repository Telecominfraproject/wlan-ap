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

spi_nor_emmc_do_upgrade_bootconfig() {
	local tar_file="$1"

	local board_dir=$(tar tf $tar_file | grep -m 1 '^sysupgrade-.*/$')
	board_dir=${board_dir%/}
	[ -f /proc/boot_info/bootconfig0/getbinary_bootconfig ] || {
		echo "bootconfig does not exist"
		exit
	}
	CI_ROOTPART="$(cat /proc/boot_info/bootconfig0/rootfs/upgradepartition)"
	CI_KERNPART="$(cat /proc/boot_info/bootconfig0/0:HLOS/upgradepartition)"

	[ -n "$CI_KERNPART" -a -n "$CI_ROOTPART" ] || {
		echo "kernel or rootfs partition is unknown"
		exit
	}

	local primary="0"
	[ "$(cat /proc/boot_info/bootconfig0/rootfs/primaryboot)" = "0" ] && primary="1"
	echo "$primary" > /proc/boot_info/bootconfig0/rootfs/primaryboot 2>/dev/null
	echo "$primary" > /proc/boot_info/bootconfig0/0:HLOS/primaryboot 2>/dev/null
	cp /proc/boot_info/bootconfig0/getbinary_bootconfig /tmp/bootconfig

	do_flash_emmc $tar_file $CI_KERNPART $board_dir kernel
	do_flash_emmc $tar_file $CI_ROOTPART $board_dir root

	local emmcblock="$(find_mmc_part "rootfs_data")"
	if [ -e "$emmcblock" ]; then
		mkfs.ext4 -F "$emmcblock"
	fi

	for part in "0:BOOTCONFIG" "0:BOOTCONFIG1"; do
				local mtdchar=$(echo $(find_mtd_chardev $part) | sed 's/^.\{5\}//')
				if [ -n "$mtdchar" ]; then
						echo start to update $mtdchar
						mtd -qq write /proc/boot_info/bootconfig0/getbinary_bootconfig "/dev/${mtdchar}" 2>/dev/null && echo update mtd $mtdchar
			   else
						emmcblock=$(find_mmc_part $part)
						echo erase ${emmcblock}
						dd if=/dev/zero of=${emmcblock} 2> /dev/null
						echo update $emmcblock
						dd if=/tmp/bootconfig of=${emmcblock} 2> /dev/null
				fi
	done
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
	cig,wf197)
		# TODO Add image upgrade and bootconfig function
		;;
	esac
}
