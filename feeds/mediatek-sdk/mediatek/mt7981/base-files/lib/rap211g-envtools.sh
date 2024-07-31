#
# Copyright (C) 2021 OpenWrt.org
#

. /lib/uboot-envtools.sh
. /lib/functions.sh
. /lib/functions/uci-defaults.sh
. /lib/functions/system.sh

CI_UBIPART=ubi

ubi_mknod() {
        local dir="$1"
        local dev="/dev/$(basename $dir)"

        [ -e "$dev" ] && return 0

        local devid="$(cat $dir/dev)"
        local major="${devid%%:*}"
        local minor="${devid##*:}"
        mknod "$dev" c $major $minor
}

get_boot_param()
{
	local cmdline_param=$(cat /proc/cmdline)
	local name
	for var in $cmdline_param
	do
		#echo "aaa---$var"
		if [ $var == $1 ];
		then
			echo "Y"
			return
		else
			name=$(echo $var | awk -F '=' '{print $1}')
			#echo "$name"
			if [ $name == $1 ];
			then
					echo $(echo $var | awk -F '=' '{print $2}')
					return
			fi
		fi
	done
	echo "N"
}

block_dev_path() {
	local dev_path

	case "$1" in
	/dev/mmcblk*)
		dev_path="$1"
		;;
	PARTLABEL=* | PARTUUID=*)
		dev_path=$(blkid -t "$1" -o device)
		[ -z "${dev_path}" -o $? -ne 0 ] && return 1
		;;
	*)
		return 1;
		;;
	esac

	echo "${dev_path}"
	return 0
}

nand_find_volume() {
	local ubidevdir ubivoldir
	ubidevdir="/sys/devices/virtual/ubi/$1"
	[ ! -d "$ubidevdir" ] && return 1
	for ubivoldir in $ubidevdir/${1}_*; do
		[ ! -d "$ubivoldir" ] && continue
		if [ "$( cat $ubivoldir/name )" = "$2" ]; then
			basename $ubivoldir
			ubi_mknod "$ubivoldir"
			return 0
		fi
	done
}

nand_find_ubi() {
	local ubidevdir ubidev mtdnum
	mtdnum="$( find_mtd_index $1 )"
	[ ! "$mtdnum" ] && return 1
	for ubidevdir in /sys/devices/virtual/ubi/ubi*; do
		[ ! -d "$ubidevdir" ] && continue
		cmtdnum="$( cat $ubidevdir/mtd_num )"
		[ ! "$mtdnum" ] && continue
		if [ "$mtdnum" = "$cmtdnum" ]; then
			ubidev=$( basename $ubidevdir )
			ubi_mknod "$ubidevdir"
			echo $ubidev
			return 0
		fi
	done
}


