#!/bin/sh

do_realtek() {
	. /lib/realtek.sh

	realtek_board_detect

	if [ -z "$(grep SP-W2M-AC1200-POE /dev/mtd6)" ]; then
		modprobe rtl819x_8211f
	else
		modprobe rtl819x_83xx
	fi
}

boot_hook_add preinit_main do_realtek
