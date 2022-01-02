
# Keep these values be up-to-date with definition in libfstools/rootdisk.c of fstools package
ROOTDEV_OVERLAY_ALIGN=$((64*1024))
F2FS_MINSIZE=$((100*1024*1024))

mtk_get_root() {
	local rootfsdev

	if read cmdline < /proc/cmdline; then
		case "$cmdline" in
			*root=*)
				rootfsdev="${cmdline##*root=}"
				rootfsdev="${rootfsdev%% *}"
			;;
		esac

		echo "${rootfsdev}"
	fi
}

mmc_upgrade_tar() {
	local tar_file="$1"
	local kernel_dev="$2"
	local rootfs_dev="$3"

	local board_dir=$(tar tf ${tar_file} | grep -m 1 '^sysupgrade-.*/$')
	board_dir=${board_dir%/}

	local kernel_length=$( (tar xf $tar_file ${board_dir}/kernel -O | wc -c) 2> /dev/null)
	local rootfs_length=$( (tar xf $tar_file ${board_dir}/root -O | wc -c) 2> /dev/null)

	[ "${kernel_length}" != 0 ] && {
		tar xf ${tar_file} ${board_dir}/kernel -O >${kernel_dev}
	}

	[ "${rootfs_length}" != 0 ] && {
		tar xf ${tar_file} ${board_dir}/root -O >${rootfs_dev}
	}

	local rootfs_dev_size=$(blockdev --getsize64 ${rootfs_dev})
	[ $? -ne 0 ] && return 1

	local rootfs_data_offset=$(((rootfs_length+ROOTDEV_OVERLAY_ALIGN-1)&~(ROOTDEV_OVERLAY_ALIGN-1)))
	local rootfs_data_size=$((rootfs_dev_size-rootfs_data_offset))

	local loopdev="$(losetup -f)"
	losetup -o $rootfs_data_offset $loopdev $rootfs_dev || {
		v "Failed to mount looped rootfs_data."
		return 1
	}

	local fstype=ext4
	local mkfs_arg="-q -L rootfs_data"
	[ "${rootfs_data_size}" -gt "${F2FS_MINSIZE}" ] && {
		fstype=f2fs
		mkfs_arg="-q -l rootfs_data"
	}

	v "Format new rootfs_data at position ${rootfs_data_offset}."
	mkfs.${fstype} ${mkfs_arg} ${loopdev}
	[ $? -ne 0 ] && return 1

	[ -n "$UPGRADE_BACKUP" ] && {
		mkdir -p /tmp/new_root
		mount -t ${fstype} ${loopdev} /tmp/new_root && {
			v "Saving config to rootfs_data at position ${rootfs_data_offset}."
			mv "$UPGRADE_BACKUP" "/tmp/new_root/$BACKUP_FILE"
			umount /tmp/new_root
		}
	}

	# Cleanup
	losetup -d ${loopdev} >/dev/null 2>&1
	sync

	return 0
}

mtk_mmc_do_upgrade() {
	local tar_file="$1"
	local board=$(board_name)
	local kernel_dev=
	local rootfs_dev=
	local cmdline_root="$(mtk_get_root)"

	case "$cmdline_root" in
	/dev/mmcblk*)
		rootfs_dev=${cmdline_root}
		;;
	PARTLABEL=* | PARTUUID=*)
		rootfs_dev=$(blkid -t "${cmdline_root}" -o device)
		[ -z "${rootfs_dev}" -o $? -ne 0 ] && return 1
		;;
	*)
		return 1;
		;;
	esac

	case "$board" in
	*)
		kernel_dev=$(blkid -t "PARTLABEL=kernel" -o device)
		[ -z "${kernel_dev}" -o $? -ne 0 ] && return 1
		;;
	esac

	# keep sure its unbound
	losetup --detach-all || {
		v "Failed to detach all loop devices."
		sleep 10
		reboot -f
	}

	mmc_upgrade_tar "${tar_file}" "${kernel_dev}" "${rootfs_dev}"

	[ $? -ne 0 ] && {
		v "Upgrade failed."
		return 1
	}

	return 0
}
