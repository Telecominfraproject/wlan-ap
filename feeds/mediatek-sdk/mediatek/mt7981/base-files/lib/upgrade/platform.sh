REQUIRE_IMAGE_METADATA=1
platform_do_upgrade() {
	local board=$(board_name)

	case "$board" in
	edgecore,eap111)
		nand_do_upgrade "$1"
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
	esac

	return 0
}
