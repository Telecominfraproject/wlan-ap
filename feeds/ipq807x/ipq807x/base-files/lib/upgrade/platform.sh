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

do_flash_bootconfig_emmc() {
        local bin=$1
        local emmcblock=$2
	echo "In do_flash_bootconfig_emmc bin is $bin emmc $emmcblock"
        dd if=/dev/zero of=${emmcblock}
        dd if=/tmp/${bin}.bin of=${emmcblock}
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

do_populate_emmc() {
        local bin=$1
        local emmcblock=$2

	echo erase $emmcblock
        dd if=/dev/zero of=${emmcblock} 2> /dev/null
	echo flash $emmcblock
        dd if=/tmp/${bin}.bin of=${emmcblock}
}


do_flash_failsafe_partition() {
        local tar_file=$1
        local mtdname=$2
	local section=$3
        local emmcblock
        local primaryboot
	local board_dir="sysupgrade-slink_r680"

        local mtd_part=$(cat /proc/mtd | grep $mtdname)
        [ -z "$mtd_part" ] && {
                mtd_part=$(echo $(find_mmc_part "$mtdname"))
                [ -z "$mtd_part" ] && return 1
        }

        # Fail safe upgrade
        [ -f /proc/boot_info/$mtdname/upgradepartition ] && {
                default_mtd=$mtdname
                mtdname=$(cat /proc/boot_info/$mtdname/upgradepartition)
		echo "SK: Upgrade partion is $mtdname"
                primaryboot=$(cat /proc/boot_info/$default_mtd/primaryboot)
                if [ $primaryboot -eq 0 ]; then
                        echo 1 > /proc/boot_info/$default_mtd/primaryboot
                else
                        echo 0 > /proc/boot_info/$default_mtd/primaryboot
                fi
        }

        emmcblock="$(find_mmc_part "$mtdname")"
	echo "Flashing $emmcblock"

        if [ -e "$emmcblock" ]; then
		echo erase $emmcblock
		dd if=/dev/zero of=${emmcblock} 2> /dev/null
		echo flash $emmcblock
		tar Oxf $tar_file ${board_dir}/$section | dd of=${emmcblock}

        else
		echo "We came in the worng location"
        fi

}

do_flash_bootconfig() {
        local bin=$1
        local mtdname=$2
	local emmcblock=$(find_mmc_part $2)

        # Indicate the bootloader that formware upgrade is complete.
        if [ -f /proc/boot_info/getbinary_${bin} ]; then
                cat /proc/boot_info/getbinary_${bin} > /tmp/${bin}.bin
                do_populate_emmc $bin $emmcblock
        fi
}

platform_version_upgrade() {
        local version_files="appsbl_version sbl_version tz_version hlos_version rpm_version"
        local sys="/sys/devices/system/qfprom/qfprom0/"
        local tmp="/tmp/"

        for file in $version_files; do
                [ -f "${tmp}${file}" ] && {
                        echo "Updating "${sys}${file}" with `cat "${tmp}${file}"`"
                        echo `cat "${tmp}${file}"` > "${sys}${file}"
                        rm -f "${tmp}${file}"
                }
        done
}

emmc_do_upgrade_failsafe() {
	local tar_file="$1"

	do_flash_failsafe_partition $1 '0:HLOS' kernel
	do_flash_failsafe_partition $1 'rootfs' root
	local emmcblock="$(find_mmc_part "rootfs_data")"
        if [ -e "$emmcblock" ]; then
                mkfs.ext4 -F "$emmcblock"
        fi

	do_flash_bootconfig bootconfig "0:BOOTCONFIG"
	do_flash_bootconfig bootconfig1 "0:BOOTCONFIG1"
	platform_version_upgrade
}



platform_check_image() {
	local magic_long="$(get_magic_long "$1")"
	board=$(board_name)
	case $board in
	cig,wf188|\
	cig,wf188n|\
	cig,wf194c|\
	cig,wf194c4|\
	cig,wf196|\
	cybertan,eww622-a1|\
	glinet,ax1800|\
	glinet,axt1800|\
	wallys,dr6018|\
	wallys,dr6018-v4|\
	edgecore,eap101|\
	edgecore,eap102|\
	edgecore,eap104|\
	edgecore,eap106|\
	hfcl,ion4xi|\
	hfcl,ion4xe|\
	tplink,ex227|\
	tplink,ex447|\
	yuncore,ax840|\
	motorola,q14|\
	qcom,ipq6018-cp01|\
	qcom,ipq807x-hk01|\
	qcom,ipq807x-hk14|\
	qcom,ipq5018-mp03.3|\
	slink,r680)
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
	cig,wf188)
		qca_do_upgrade $1
		;;
	motorola,q14)
		emmc_do_upgrade $1
		;;
	slink,r680)
		emmc_do_upgrade_failsafe $1
		;;
	cig,wf188n|\
	cig,wf194c|\
	cig,wf194c4|\
	cig,wf196|\
	cybertan,eww622-a1|\
	edgecore,eap104|\
	glinet,ax1800|\
	glinet,axt1800|\
	qcom,ipq6018-cp01|\
	qcom,ipq807x-hk01|\
	qcom,ipq807x-hk14|\
	qcom,ipq5018-mp03.3|\
	wallys,dr6018|\
	wallys,dr6018-v4|\
	yuncore,ax840|\
	tplink,ex447|\
	tplink,ex227)
		nand_upgrade_tar "$1"
		;;
	hfcl,ion4xi|\
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
	edgecore,eap106)
		CI_UBIPART="rootfs1"
		[ "$(find_mtd_chardev rootfs)" ] && CI_UBIPART="rootfs"
		nand_upgrade_tar "$1"
		;;
	edgecore,eap101|\
	edgecore,eap102)
		if [ "$(find_mtd_chardev rootfs)" ]; then
			CI_UBIPART="rootfs"
		else
			if grep -q rootfs1 /proc/cmdline; then
				CI_UBIPART="rootfs2"
				fw_setenv active 2 || exit 1
			else
				CI_UBIPART="rootfs1"
				fw_setenv active 1 || exit 1
			fi
		fi
		nand_upgrade_tar "$1"
		;;
	esac
}
