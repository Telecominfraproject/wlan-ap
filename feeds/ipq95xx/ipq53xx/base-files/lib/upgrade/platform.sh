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

emmc_do_upgrade() {
	local tar_file="$1"
	local block_kernel="0:HLOS"
	local block_rootfs="rootfs"
	local board_dir=$(tar tf $tar_file | grep -m 1 '^sysupgrade-.*/$')
	board_dir=${board_dir%/}

	board=$(board_name)
	case $board in
	sonicfi,rap7110c-341x)
		local boot_part=""
		boot_part=$(fw_printenv | grep bootfrom | awk -F'=' '{printf $2}')
		[ -n "$boot_part" ] || boot_part="0"
		echo "**** boot_part=$boot_part" > /dev/console

		if [ "$boot_part" = "0" ]; then
			block_kernel="0:HLOS_1"
			block_rootfs="rootfs_1"
			fw_setenv bootfrom 1
		elif [ "$boot_part" = "1" ]; then
			block_kernel="0:HLOS"
			block_rootfs="rootfs"
			fw_setenv bootfrom 0
		else
			echo "Invalid boot partition $boot_part! Skip upgrade....."
			return
		fi
		;;
	esac
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

	board=$(board_name)
	case $board in
	cig,wf189|\
	edgecore,eap105|\
	sercomm,ap72tip|\
	qcom,ipq9574-ap-al02-c4|\
	qcom,ipq9574-ap-al02-c15)
		nand_upgrade_tar "$1"
		;;
	sonicfi,rap7110c-341x)
		emmc_do_upgrade $1 $1
	;;
	esac
}
