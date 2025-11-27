. /lib/functions/system.sh


RAMFS_COPY_BIN='fw_setenv'
RAMFS_COPY_DATA='/etc/fw_env.config /var/lock/fw_printenv.lock /tmp/downgrade'

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
	dd if=/dev/zero of=${emmcblock} 2> /dev/null
	echo flash $4
	tar Oxf $tar_file ${board_dir}/$part | dd of=${emmcblock}
}

emmc_do_upgrade() {
	local tar_file="$1"

	local board_dir=$(tar tf $tar_file | grep -m 1 '^sysupgrade-.*/$')
	board_dir=${board_dir%/}
	do_flash_emmc $tar_file '0:HLOS' $board_dir kernel
	do_flash_emmc $tar_file 'rootfs' $board_dir root

	local emmcblock="$(find_mmc_part "rootfs_data")"
	if [ -e "$emmcblock" ]; then
		mkfs.ext4 -F "$emmcblock"
	fi
}

platform_check_image() {
	local magic_long="$(get_magic_long "$1")"
	board=$(board_name)
	case $board in
	cig,wf186w|\
	cig,wf186h|\
	sonicfi,rap630c-311g|\
	sonicfi,rap630w-311g|\
	sonicfi,rap630w-312g|\
	sonicfi,rap630e|\
	cybertan,eww631-a1|\
	cybertan,eww631-b1|\
	edgecore,eap104|\
	emplus,wap385c|\
	wallys,dr5018|\
	hfcl,ion4x_w|\
	hfcl,ion4xi_w|\
	indio,um-325ax-v2|\
	indio,um-335ax|\
	indio,um-525axp|\
	indio,um-525axm|\
	optimcloud,d60|\
	optimcloud,d60-5g|\
	optimcloud,d50|\
	optimcloud,d50-5g|\
	yuncore,fap655|\
	glinet,b3000|\
	udaya,a6-id2|\
	udaya,a6-od2|\
	edgecore,oap101|\
	edgecore,oap101-6e|\
	edgecore,oap101e|\
	emplus,wap581|\
	edgecore,oap101e-6e)
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
	indio,um-325ax-v2|\
	indio,um-335ax|\
	indio,um-525axp|\
	indio,um-525axm|\
	edgecore,oap101|\
	edgecore,oap101-6e|\
	edgecore,oap101e|\
	edgecore,oap101e-6e|\
	edgecore,eap104)
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
	glinet,b3000)
		CI_UBIPART="rootfs1"
		[ "$(find_mtd_chardev rootfs)" ] && CI_UBIPART="rootfs"
		nand_upgrade_tar "$1"
		;;
	hfcl,ion4x_w|\
	hfcl,ion4xi_w)
		wp_part=$(fw_printenv primary | cut  -d = -f2)
		echo "Current Primary is $wp_part"
		if [[ $wp_part == 1 ]]; then
				CI_UBIPART="rootfs"
				CI_FWSETENV="primary 0"
		else
				CI_UBIPART="rootfs_1"
				CI_FWSETENV="primary 1"
		fi
		nand_upgrade_tar "$1"
		;;
	cig,wf186w|\
	cig,wf186h|\
	emplus,wap385c|\
	udaya,a6-id2|\
	udaya,a6-od2|\
	wallys,dr5018|\
	optimcloud,d60|\
	optimcloud,d60-5g|\
	optimcloud,d50|\
	optimcloud,d50-5g|\
	emplus,wap581|\
	yuncore,fap655)
		[ -f /proc/boot_info/rootfs/upgradepartition ] && {
			CI_UBIPART="$(cat /proc/boot_info/rootfs/upgradepartition)"
			CI_BOOTCFG=1
		}
		nand_upgrade_tar "$1"
		;;
	cybertan,eww631-a1|\
	cybertan,eww631-b1|\
	sonicfi,rap630c-311g|\
	sonicfi,rap630w-311g|\
	sonicfi,rap630w-312g|\
	sonicfi,rap630e)
		boot_part=$(fw_printenv bootfrom | cut  -d = -f2)
			echo "Current bootfrom is $boot_part"
			if [[ $boot_part == 1 ]]; then
				CI_UBIPART="rootfs"
				CI_FWSETENV="bootfrom 0"
			elif [[ $boot_part == 0 ]]; then
				CI_UBIPART="rootfs_1"
				CI_FWSETENV="bootfrom 1"
			fi
		nand_upgrade_tar "$1"
		;;
	esac
}
