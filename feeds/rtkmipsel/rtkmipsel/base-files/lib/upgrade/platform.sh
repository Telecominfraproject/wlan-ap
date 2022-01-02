RAMDISK_SWITCH=1
#NO_LDD_SUPPORT=1

get_fwhw_string() {
        local seek=$(($(wc -c < "$1")-64))
        2>/dev/null hexdump -n 64 -s $seek -e "64 \"%_c\""  $1 | cut -d '\' -f 1
        return 0;
}

sysupgrade_l() {
        logger -t sysupgrade -p info $1
        return 0
}

image_fw_product_matches() {
        # get DISTRIB_PRODUCT value as current_prod
        local current_prod=$(2>/dev/null grep DISTRIB_PRODUCT /etc/release | cut -d=  -f2 | sed -e 's/^"//' -e 's/"$//')
        local candidate=$(get_fwhw_string $1)
        # allow all firmwares if product in /etc/release is not set
        [ -z "$current_prod" ] && {\
                sysupgrade_l "Empty product value. Allowing all firmwares."
                return 0;
        }
        # candidate must have non empty custom fw string
        [ -z "$candidate" ] &&  {\
                sysupgrade_l "Firmware without fw string set. Aborting..."
                return 1;
        }

        local candidate_prod=$(echo -n $candidate | cut -d. -f1)

        [ "$candidate_prod" = "$current_prod" ] && return 0;

        sysupgrade_l "Not matching fw products found $candidate_prod and $current_prod"
        # not an exact product match
        return 1;
}

platform_check_image() {
        [ "$ARGC" -gt 1 ] && return 1

        case "$(get_magic_word "$1")" in
                # .cvimg files
                6373)
                        image_fw_product_matches $1 || return 1
                        return 0;;
                *)
                        echo "Invalid image type. Please use only .trx files"
                        return 1
                ;;
        esac
}

platform_do_upgrade() {
	
	default_do_upgrade "$ARGV"
}
# use default for platform_do_upgrade()

# without toolchain ldd support(NO_LDD_SUPPORT=1) , you have to below before switch root to ramdisk
#realtek pre upgrade
install_ram_libs() {
	echo "- install_ram_libs -"
	ramlib="$RAM_ROOT/lib"
	mkdir -p "$ramlib"
	cp /lib/*.so.* $ramlib 
	cp /lib/*.so $ramlib 
}

disable_watchdog() {
        killall watchdog
        ( ps | grep -v 'grep' | grep '/dev/watchdog' ) && {
                echo 'Could not disable watchdog'
                return 1
        }
}
rtk_dualimage_check() {
        local bootmode_cmd=`cat /tmp/bootmode`
        local bootbank_cmd=`cat /tmp/bootbank`
	local bootmode=0
        local cur_bootbank=0
        local next_bootbank=$cur_bootbank
        PART_NAME=linux

        [ -f /proc/flash/bootoffset ] && {
        bootmode=$bootmode_cmd
        cur_bootbank=$bootbank_cmd
        if [ $bootmode = '1' ];then
                echo "It's dualimage toggle mode,always burn image to backup and next boot from it"
                PART_NAME=linux_backup
                if [ $cur_bootbank = '0' ];then
                next_bootbank=1
                else
                next_bootbank=0
                fi
        fi
	rtk_bootinfo setbootbank $next_bootbank
	sleep 1
        }
}


rtk_pre_upgrade() {
	echo "- rtk_pre_upgrade -"
	rtk_dualimage_check
	if [ -n "$RAMDISK_SWITCH" ]; then
		if [ -n "$NO_LDD_SUPPORT" ]; then
        		install_ram_libs
		fi
	fi
	disable_watchdog
}

append sysupgrade_pre_upgrade rtk_pre_upgrade
