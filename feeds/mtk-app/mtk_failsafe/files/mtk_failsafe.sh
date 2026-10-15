#!/bin/sh

mtk_common_init() {
	echo "Running mtk failsafe script..."
	echo "You can edit here : package/mtk/mtk_failsafe/file/mtk_failsafe.sh"
	echo "mtk_common_init....."
	mount_root
	mount_root done
	sync
	echo 3 > /proc/sys/vm/drop_caches
}

mtk_wifi_init() {
	echo "mtk_wifi_init....."
	# once the bin is correct, unmark below
        #insmod wifi_emi_loader
        #rmmod wifi_emi_loader
	#insmod mt_wifi
        #ifconfig ra0 up
        #ifconfig rax0 up
}

mtk_network_init() {
	echo "mtk_network_init....."
	# NOTE : LAN IP subnet should be 192.168.1.x
        ifconfig eth0 0.0.0.0
        brctl addbr br-lan
        ifconfig br-lan 192.168.1.1 netmask 255.255.255.0 up
        brctl addif br-lan eth0
        #brctl addif br-lan ra0
        #brctl addif br-lan rax0
	./etc/init.d/telnet start
	#./usr/bin/ated
}

mtk_common_init
mtk_wifi_init
mtk_network_init

