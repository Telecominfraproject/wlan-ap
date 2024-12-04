#
# Copyright (c) 2020, The Linux Foundation. All rights reserved.
#
# Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

. /lib/functions.sh
. /lib/upgrade/common.sh

RAMFS_COPY_DATA="/etc/fw_env.config /var/lock/fw_printenv.lock"
RAMFS_COPY_BIN="/usr/bin/dumpimage /usr/sbin/ubiattach /usr/sbin/ubidetach
	/usr/sbin/ubiformat /usr/sbin/ubiupdatevol /bin/rm /usr/bin/find
	/usr/sbin/mkfs.ext4 /usr/sbin/fw_printenv /sbin/lsmod"

get_full_section_name() {
	local img=$1
	local sec=$2

	dumpimage -l ${img} | grep "^ Image.*(${sec})" | \
		sed 's,^ Image.*(\(.*\)),\1,'
}

image_contains() {
	local img=$1
	local sec=$2
	dumpimage -l ${img} | grep -q "^ Image.*(${sec}.*)" || return 1
}

print_sections() {
	local img=$1

	dumpimage -l ${img} | awk '/^ Image.*(.*)/ { print gensub(/Image .* \((.*)\)/,"\\1", $0) }'
}

image_has_mandatory_section() {
	local img=$1
	local mandatory_sections=$2

	for sec in ${mandatory_sections}; do
		image_contains $img ${sec} || {\
			return 1
		}
	done
}

image_demux() {
	local img=$1

	for sec in $(print_sections ${img}); do
		local fullname=$(get_full_section_name ${img} ${sec})

		local position=$(dumpimage -l ${img} | grep "(${fullname})" | awk '{print $2}')
		version=$(dumpimage -V 2>&1 | awk '{split($3, a, "."); print a[1]}')

		if [ "$version" == "2016" ]; then
			dumpimage -i ${img} -o /tmp/${fullname}.bin -T "flat_dt" -p "${position}" ${fullname} > /dev/null || { \
				echo "Error while extracting \"${sec}\" from ${img}"
				return 1
			}
		else
			dumpimage -o /tmp/${fullname}.bin -T "flat_dt" -p "${position}" ${img} > /dev/null || { \
				echo "Error while extracting \"${sec}\" from ${img}"
				return 1
			}
		fi
	done
	return 0
}

image_is_FIT() {
	if ! dumpimage -l $1 > /dev/null 2>&1; then
		echo "$1 is not a valid FIT image"
		return 1
	fi
	return 0
}

do_flash_mtd() {
	local bin=$1
	local mtdname=$2
	local append=""
	local mtdname_rootfs="rootfs"
	local boot_layout=`find / -name boot_layout`
	local flash_type=`fw_printenv | grep flash_type=11`

	local mtdpart=$(grep "\"${mtdname}\"" /proc/mtd | awk -F: '{print $1}')
	if [ ! -n "$mtdpart" ]; then
		echo "$mtdname is not available" && return
	fi

	local pgsz=$(cat /sys/class/mtd/${mtdpart}/writesize)

	local mtdpart_rootfs=$(grep "\"${mtdname_rootfs}\"" /proc/mtd | awk -F: '{print $1}')

	[ -f "$UPGRADE_BACKUP" -a "$2" == "rootfs" ] && append="-j $UPGRADE_BACKUP"
	dd if=/tmp/${bin}.bin bs=${pgsz} conv=sync | mtd $append -e "/dev/${mtdpart}" write - "/dev/${mtdpart}"
}

do_flash_emmc() {
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
		do_flash_emmc $bin $emmcblock
	else
		do_flash_mtd $bin $mtdname
	fi
}

get_alternate_bootconfig() {
	local age0=$(cat /proc/boot_info/bootconfig0/age)
        local age1=$(cat /proc/boot_info/bootconfig1/age)

	if [ -e /proc/upgrade_info/trybit ]; then
		if [ $age0 -le $age1 ]; then
			echo "bootconfig0"
		else
			echo "bootconfig1"
		fi
	else
		echo "bootconfig0 bootconfig1"
	fi
}

get_current_bootconfig() {
	local bcname=$1
	local age0=$(cat /proc/boot_info/bootconfig0/age)
	local age1=$(cat /proc/boot_info/bootconfig1/age)

	if [ -e /proc/upgrade_info/trybit ]; then
		if [ $age0 -le $age1 ]; then
			echo "bootconfig1"
		else
			echo "bootconfig0"
		fi
	else
		echo $bcname
	fi
}

do_flash_bootconfig() {
	local bin=$1
	local mtdname=$2

	# Fail safe upgrade
	if [ -f /proc/boot_info/$bin/getbinary_bootconfig ]; then
		cat /proc/boot_info/$bin/getbinary_bootconfig > /tmp/${bin}.bin
		do_flash_partition $bin $mtdname
	fi
}

do_flash_failsafe_partition() {
	local bin=$1
	local mtdname=$2
	local emmcblock
	local primaryboot
	local default_mtd
	local primary_bcname

	#Failsafe upgrade
	default_mtd=$mtdname
	for bcname in $(get_alternate_bootconfig)
	do
		[ -f /proc/boot_info/$bcname/$default_mtd/upgradepartition ] && {
			primary_bcname=$(get_current_bootconfig $bcname)
			primaryboot=$(cat /proc/boot_info/$primary_bcname/$default_mtd/primaryboot)
			mtdname=$(cat /proc/boot_info/$bcname/$default_mtd/upgradepartition)
			echo $((primaryboot ^= 1)) > /proc/boot_info/$bcname/$default_mtd/primaryboot
		}
	done

	emmcblock="$(find_mmc_part "$mtdname")"

	if [ -e "$emmcblock" ]; then
		do_flash_emmc $bin $emmcblock
	else
		do_flash_mtd $bin $mtdname
	fi

}

do_flash_ubi() {
	local bin=$1
	local mtdname=$2
	local mtdpart
	local primaryboot
	local default_mtd
	local primary_bcname

	mtdpart=$(grep "\"${mtdname}\"" /proc/mtd | awk -F: '{print $1}')
	ubidetach -f -p /dev/${mtdpart}

	# Fail safe upgrade
	default_mtd=$mtdname
	for bcname in $(get_alternate_bootconfig)
	do
		[ -f /proc/boot_info/$bcname/$default_mtd/upgradepartition ] && {
			primary_bcname=$(get_current_bootconfig $bcname)
			primaryboot=$(cat /proc/boot_info/$primary_bcname/$default_mtd/primaryboot)
			mtdname=$(cat /proc/boot_info/$bcname/$default_mtd/upgradepartition)
			echo $((primaryboot ^= 1)) > /proc/boot_info/$bcname/$default_mtd/primaryboot
		}
	done

	mtdpart=$(grep "\"${mtdname}\"" /proc/mtd | awk -F: '{print $1}')
	ubiformat /dev/${mtdpart} -y -f /tmp/${bin}.bin
}

do_flash_failsafe_ubi_volume() {
	local bin=$1
	local mtdname=$2
	local vol_name=$3
	local tmpfile="${bin}.bin"
	local mtdpart
	local default_mtd
	
	default_mtd=$mtdname
	for bcname in $(get_alternate_bootconfig)
	do
		[ -f /proc/boot_info/$bcname/$default_mtd/upgradepartition ] && {
			mtdname=$(cat /proc/boot_info/$bcname/$default_mtd/upgradepartition)
		}
	done

	mtdpart=$(grep "\"${mtdname}\"" /proc/mtd | awk -F: '{print $1}')

	if [ ! -n "$mtdpart" ]; then
		echo "$mtdname is not available" && return
	fi

	ubiattach -p /dev/${mtdpart}

	volumes=$(ls /sys/class/ubi/*/ | grep ubi._.*)

	for vol in ${volumes}
	do
		[ -f /sys/class/ubi/${vol}/name ] && name=$(cat /sys/class/ubi/${vol}/name)
			[ ${name} == ${vol_name} ] && m_vol=${vol}
	done

	ubiupdatevol /dev/${m_vol} /tmp/${tmpfile}
}

do_flash_tz() {
	local sec=$1
	local mtdpart=$(grep "\"0:QSEE\"" /proc/mtd | awk -F: '{print $1}')
	local emmcblock="$(find_mmc_part "0:QSEE")"

	if [ -n "$mtdpart" -o -e "$emmcblock" ]; then
		do_flash_failsafe_partition ${sec} "0:QSEE"
		do_flash_failsafe_partition ${sec} "0:QSEE_1"
	fi
}

do_flash_ddr() {
	local sec=$1
	local mtdpart=$(grep "\"0:CDT\"" /proc/mtd | awk -F: '{print $1}')
	local emmcblock="$(find_mmc_part "0:CDT")"

	if [ -n "$mtdpart" -o -e "$emmcblock" ]; then
		do_flash_failsafe_partition ${sec} "0:CDT"
	fi
}

to_lower ()
{
	echo $1 | awk '{print tolower($0)}'
}

to_upper ()
{
	echo $1 | awk '{print toupper($0)}'
}

image_is_nand()
{
	local nand_part="$(find_mtd_part "ubi_rootfs")"
	[ -e "$nand_part" ] || return 1

}

get_fw_name() {
	wifi_ipq="ignored"
	image_suffix1="qcn9224_v2_single_dualmac"
	image_suffix2="qcn6432cs"
	image_suffix3="qcn6432"
	image_suffix4="qcn9224_v2_qcn6432"
	image_suffix5="qcn9224_v2_qcn9160"
	machineid=$(fw_printenv -l /tmp/. machid | cut -d '=' -f 2)

	case "${machineid}" in
		"F060000"|\
		"8060000"|\
		"8060001"|\
		"8060003"|\
		"8060006"|\
		"1060001"|\
		"1060002"|\
		"8060201")
			wifi_ipq="ipq5332_"$image_suffix1
			;;
		"8060002"|\
		"8060004")
			wifi_ipq="ipq5332_"$image_suffix2
			;;
		"1060003"|\
		"8060102"|\
		"8060007")
			wifi_ipq="ipq5332_"$image_suffix3
			;;
		"8060202"|\
		"8060302")
			wifi_ipq="ipq5332_"$image_suffix4
			;;
		"8060101")
			wifi_ipq="ipq5332_"$image_suffix5
			;;
		*)
			wifi_ipq="ipq5332_qcn9224_v2_single_dualmac_qcn9160"
			;;

	esac

	echo $wifi_ipq
}

flash_section() {
	local sec=$1
	local board=$(board_name)
	local board_model=$(to_lower $(grep -o "RDP.*" /proc/device-tree/model | awk -F/ '{print $2}'))

	case "${sec}" in
		hlos*) image_is_nand && return || do_flash_failsafe_partition ${sec} "0:HLOS";;
		rootfs*) image_is_nand && return || do_flash_failsafe_partition ${sec} "rootfs";;
		wifi_fw_$(get_fw_name)-*) do_flash_failsafe_partition ${sec} "0:WIFIFW"; do_flash_failsafe_ubi_volume ${sec} "rootfs" "wifi_fw" ;;
		ubi*) image_is_nand || return && do_flash_ubi ${sec} "rootfs";;
		sbl1*) do_flash_partition ${sec} "0:SBL1"; \
			do_flash_partition ${sec} "0:SBL1_1";;
		u-boot*) do_flash_failsafe_partition ${sec} "0:APPSBL";;
		ddr-$(to_upper $board_model)_*) do_flash_ddr ${sec};;
		ddr-${board_model}-*) do_flash_failsafe_partition ${sec} "0:DDRCONFIG";;
		tz*) do_flash_tz ${sec};;
		tme*) do_flash_partition ${sec} "0:TME"; \
			do_flash_partition ${sec} "0:TME_1";;
		devcfg*) do_flash_failsafe_partition ${sec} "0:DEVCFG";;
		*) echo "Section ${sec} ignored"; return 1;;
	esac

	echo "Flashed ${sec}"
}

erase_emmc_config() {
	local mtdpart=$(cat /proc/mtd | grep rootfs)
	local emmcblock="$(find_mmc_part "rootfs_data")"
	if [ -z "$mtdpart" -a -e "$emmcblock" ]; then
		yes | mkfs.ext4 "$emmcblock"
	fi
}

platform_check_image() {
	local board=$(board_name)
	local board_model=$(to_lower $(grep -o "RDP.*" /proc/device-tree/model | awk -F/ '{print $2}'))
	local mandatory_nand="ubi"
	local mandatory_nor_emmc="hlos fs"
	local mandatory_nor="hlos"
	local mandatory_section_found=0
	local ddr_section="ddr"
	local optional="sb11 sbl2 u-boot lkboot ddr-${board_model} tz rpm"
	local ignored="mibib bootconfig"

	image_is_FIT $1 || return 1

	image_has_mandatory_section $1 ${mandatory_nand} && {\
		mandatory_section_found=1
	}

	image_has_mandatory_section $1 ${mandatory_nor_emmc} && {\
		mandatory_section_found=1
	}

	image_has_mandatory_section $1 ${mandatory_nor} && {\
		mandatory_section_found=1
	}

	if [ $mandatory_section_found -eq 0 ]; then
		echo "Error: mandatory section(s) missing from \"$1\". Abort..."
		return 1
	fi

	image_has_mandatory_section $1 $ddr_section && {\
		image_contains $1 ddr-$board_model || {\
			image_contains $1 ddr-$(to_upper $board_model) || {\
			return 1
			}
		}
	}
	for sec in ${optional}; do
		image_contains $1 ${sec} || {\
			echo "Warning: optional section \"${sec}\" missing from \"$1\". Continue..."
		}
	done

	for sec in ${ignored}; do
		image_contains $1 ${sec} && {\
			echo "Warning: section \"${sec}\" will be ignored from \"$1\". Continue..."
		}
	done

	echo 1711 > /proc/sys/vm/min_free_kbytes
	echo 3 > /proc/sys/vm/drop_caches

	image_demux $1 || {\
		echo "Error: \"$1\" couldn't be extracted. Abort..."
		return 1
	}

	[ -f /tmp/hlos_version ] && rm -f /tmp/*_version
	dumpimage -c $1
	if [[ "$?" == 0 ]];then
		return $?
	else
		echo "Rebooting the system"
		reboot
		return 1
	fi
}

platform_do_upgrade() {
	local board=$(board_name)

	# verify some things exist before erasing
	if [ ! -e $1 ]; then
		echo "Error: Can't find $1 after switching to ramfs, aborting upgrade!"
		reboot
	fi

	for sec in $(print_sections $1); do
		if [ ! -e /tmp/${sec}.bin ]; then
			echo "Error: Cant' find ${sec} after switching to ramfs, aborting upgrade!"
			reboot
		fi
	done

	case "$board" in
	qcom,devsoc-ap-emulation |\
	qcom,ipq5332-ap-mi01.2 |\
	qcom,ipq5332-ap-mi01.2-c2 |\
	qcom,ipq5332-ap-mi01.2-qcn9160-c1 |\
	qcom,ipq5332-ap-mi01.3 |\
	qcom,ipq5332-ap-mi01.3-c2 |\
	qcom,ipq5332-ap-mi01.4 |\
	qcom,ipq5332-ap-mi01.6 |\
	qcom,ipq5332-ap-mi01.7 |\
	qcom,ipq5332-ap-mi01.9 |\
	qcom,ipq5332-ap-mi01.12 |\
	qcom,ipq5332-ap-mi01.14 |\
	qcom,ipq5332-ap-mi04.1 |\
	qcom,ipq5332-ap-mi04.1-c2 |\
	qcom,ipq5332-db-mi01.1 |\
	qcom,ipq5332-db-mi02.1)
		for sec in $(print_sections $1); do
			flash_section ${sec}
		done

		for bcname in $(get_alternate_bootconfig)
		do
			if [ $bcname = "bootconfig0" ]; then
				do_flash_bootconfig $bcname "0:BOOTCONFIG"
			else
				do_flash_bootconfig $bcname "0:BOOTCONFIG1"
			fi
		done

		erase_emmc_config
		return 0;
		;;
	esac

	echo "Upgrade failed!"
	return 1;
}

age_do_upgrade(){
	age0=$(cat /proc/boot_info/bootconfig0/age)
	age1=$(cat /proc/boot_info/bootconfig1/age)

	if [ -e /proc/upgrade_info/trybit ]; then
		if [ $age0 -eq $age1 ]; then
			ageinc=$((age0+1))
			echo $ageinc > /proc/boot_info/bootconfig0/age
			do_flash_bootconfig bootconfig0 "0:BOOTCONFIG"
		elif [ $age0 -lt $age1 ]; then
			ageinc=$((age0+2))
			echo $ageinc > /proc/boot_info/bootconfig0/age
			do_flash_bootconfig bootconfig0 "0:BOOTCONFIG"
		else
			ageinc=$((age1+2))
			echo $ageinc > /proc/boot_info/bootconfig1/age
			do_flash_bootconfig bootconfig1 "0:BOOTCONFIG1"
		fi
	else
		echo "Not in Try mode"
	fi
}


get_magic_long_at() {
        dd if="$1" skip=$(( 65536 / 4 * $2 )) bs=4 count=1 2>/dev/null | hexdump -v -n 4 -e '1/1 "%02x"'
}

# find rootfs_data start magic
platform_get_offset() {
        offsetcount=0
        magiclong="x"

        while magiclong=$( get_magic_long_at "$1" "$offsetcount" ) && [ -n "$magiclong" ]; do
                case "$magiclong" in
                        "deadc0de"|"19852003")
                                echo $(( $offsetcount * 65536 ))
                                return
                        ;;
                esac
                offsetcount=$(( $offsetcount + 1 ))
        done
	echo $(( $offsetcount * 65536 ))
}


platform_copy_config() {
	local nand_part="$(find_mtd_part "ubi_rootfs")"
	local emmcblock="$(find_mmc_part "rootfs")"
	local upgradepart
	mkdir -p /tmp/overlay

	#setting Try bit
	if [ -e /proc/upgrade_info/trybit ]; then
		echo 1 > /proc/upgrade_info/trybit
	fi

	for bcname in $(get_alternate_bootconfig)
	do
		[ -f /proc/boot_info/$bcname/rootfs/upgradepartition ] && {
			upgradepart=$(cat /proc/boot_info/$bcname/rootfs/upgradepartition)
		}
	done

	if [ -e "${nand_part%% *}" ]; then
		local mtdpart
		mtdpart=$(grep "\"${upgradepart}\"" /proc/mtd | awk -F: '{print $1}')
		ubiattach -p /dev/${mtdpart}
		mount -t ubifs ubi0:rootfs_data /tmp/overlay
	elif [ -e "$emmcblock" ]; then
		losetup --detach-all
		local data_blockoffset="$(platform_get_offset $(ls /tmp/rootfs-*))"
		local loopdev="$(losetup -f)"
		[ "$upgradepart" == "rootfs" ] && upgradepart="rootfs_1" || upgradepart="rootfs"
		emmcblock="$(find_mmc_part ${upgradepart})"
		losetup -o $data_blockoffset $loopdev $emmcblock || {
			echo "Failed to mount looped rootfs_data."
			reboot
		}
		echo y | mkfs.ext4 -F -L rootfs_data $loopdev
		mount -t ext4 "$loopdev" /tmp/overlay
		cp /tmp/sysupgrade.tgz /tmp/overlay/
		sync
		umount /tmp/overlay
	fi
	cp /tmp/sysupgrade.tgz /tmp/overlay/
	sync
	umount /tmp/overlay
}

