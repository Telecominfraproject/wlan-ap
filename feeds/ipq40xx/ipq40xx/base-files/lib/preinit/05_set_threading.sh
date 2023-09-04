. /lib/functions.sh

preinit_set_threading() {
	echo 1 > /sys/class/net/eth0/threaded
}

boot_hook_add preinit_main preinit_set_threading
