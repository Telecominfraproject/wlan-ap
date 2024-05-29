REQUIRE_IMAGE_METADATA=1
platform_do_upgrade() {
	local board=$(board_name)
	case "$board" in
	edgecore,eap111)
		nand_do_upgrade "$1"
		;;
	sonicfi,rap630w-211g)
		/tmp/nand_sonicfi_rap630w_211g.sh "$1"
		;;		
	esac
}

platform_check_image() {
	local board=$(board_name)
	local magic="$(get_magic_long "$1")"

	[ "$#" -gt 1 ] && return 1

	case "$board" in
	edgecore,eap111)
		nand_do_platform_check "$board" "$1"
		return $?
		;;
	sonicfi,rap630w-211g)
		[ "$magic" != "73797375" ] && {
			echo "Invalid image type."
			return 1
		}
		return 0
		;;
	esac

	return 0
}
