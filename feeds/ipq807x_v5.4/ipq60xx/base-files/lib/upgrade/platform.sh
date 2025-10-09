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

	[ -z "$emmcblock" ] && {
		echo failed to find $2
		return
	}

	echo erase $4
	dd if=/dev/zero of=${emmcblock}
	echo flash $4
	tar Oxf $tar_file ${board_dir}/$part | dd of=${emmcblock}
}

spi_nor_emmc_do_upgrade_bootconfig() {
	local tar_file="$1"

	local board_dir=$(tar tf $tar_file | grep -m 1 '^sysupgrade-.*/$')
	board_dir=${board_dir%/}
	[ -f /proc/boot_info/getbinary_bootconfig ] || {
		echo "bootconfig does not exist"
		exit
	}
	CI_ROOTPART="$(cat /proc/boot_info/rootfs/upgradepartition)"
	CI_KERNPART="$(cat /proc/boot_info/0:HLOS/upgradepartition)"

	[ -n "$CI_KERNPART" -a -n "$CI_ROOTPART" ] || {
		echo "kernel or rootfs partition is unknown"
		exit
	}

	local primary="0"
	[ "$(cat /proc/boot_info/rootfs/primaryboot)" = "0" ] && primary="1"
	echo "$primary" > /proc/boot_info/rootfs/primaryboot 2>/dev/null
	echo "$primary" > /proc/boot_info/0:HLOS/primaryboot 2>/dev/null
	cp /proc/boot_info/getbinary_bootconfig /tmp/bootconfig

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
                       mtd -qq write /proc/boot_info/getbinary_bootconfig "/dev/${mtdchar}" 2>/dev/null && echo update mtd $mtdchar
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
	do_flash_emmc $tar_file '0:HLOS' $board_dir kernel
	do_flash_emmc $tar_file 'rootfs' $board_dir root

	local emmcblock="$(find_mmc_part "rootfs_data")"
        if [ -e "$emmcblock" ]; then
                mkfs.ext4 "$emmcblock"
        fi
}

platform_check_image() {
	local magic_long="$(get_magic_long "$1")"
	board=$(board_name)
	case $board in
	cig,wf660a|\
	cig,wf188n|\
	cig,wf194c4|\
	cig,wf196|\
	glinet,ax1800|\
	glinet,axt1800|\
	indio,um-310ax-v1|\
	wallys,dr6018|\
	wallys,dr6018-v4|\
	edgecore,eap101|\
	emplus,wap386v2|\
	hfcl,ion4xi|\
	hfcl,ion4x|\
	hfcl,ion4x_2|\
	hfcl,ion4x_3|\
	hfcl,ion4xe|\
	yuncore,ax840|\
	yuncore,fap650)
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
	cig,wf660a)
		spi_nor_emmc_do_upgrade_bootconfig $1
		;;
	cig,wf188n|\
	emplus,wap386v2)
		[ -f /proc/boot_info/rootfs/upgradepartition ] && {
			CI_UBIPART="$(cat /proc/boot_info/rootfs/upgradepartition)"
			CI_BOOTCFG=1
		}
		nand_upgrade_tar "$1"
		;;
	glinet,ax1800|\
	glinet,axt1800|\
	indio,um-310ax-v1|\
	wallys,dr6018|\
	wallys,dr6018-v4|\
	yuncore,ax840|\
	yuncore,fap650)
		nand_upgrade_tar "$1"
		;;
	hfcl,ion4xi|\
	hfcl,ion4x|\
	hfcl,ion4x_2|\
	hfcl,ion4x_3|\
	hfcl,ion4xe)
		if grep -q rootfs_1 /proc/cmdline; then
			CI_UBIPART="rootfs"
			fw_setenv primary 0 || exit 1
		else
			CI_UBIPART="rootfs_1"
			fw_setenv primary 1 || exit 1
		fi
		nand_upgrade_tar "$1"
		;;
	edgecore,eap101)
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
	esac
}
