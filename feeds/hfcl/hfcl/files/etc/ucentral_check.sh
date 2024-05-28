#!/bin/sh
echo "Start Websocket check/recovery script"

ucentral_conn=$(netstat -atulpn | grep -i ucentral | awk '{print $6}')
hostname_AP=$(uci get system.@system[0].hostname)
uc_file_check=$(du /etc/config/ucentral | awk '{print $1}' )
sleep 20

curr_date=$(date)

if [[ "$uc_file_check" = 0 ]]
then
	echo "[[$curr_date]]  empty ucentral file found, need to factory reset"
	ubi_mount=$(mount | grep ubifs | grep noatime | awk '{print $1}')
	if [[ "$ubi_mount" != "/dev/ubi0_3" ]]
	then
		echo "[[$curr_date]]  ubifs not mounted, need to reboot before factory reset, mount was $ubi_mount"
		/sbin/reboot
	else
		/sbin/jffs2reset -y -r
	fi
elif [[ "$hostname_AP" = "OpenWrt" ]]
then
	echo "[[$curr_date]] hostname set to openwrt, doing ucentral and capabilities load"
	/usr/share/ucentral/capabilities.uc
	rlink=$(readlink -f /etc/ucentral/ucentral.active)
	/usr/share/ucentral/ucentral.uc /etc/ucentral/ucentral.active
	rm -rf /etc/ucentral/ucentral.active
	ln -s $rlink /etc/ucentral/ucentral.active
	sleep 60
	ucentral_check=$(netstat -atulpn | grep -i ucentral | awk '{print $6}')
	if [[ "$ucentral_check" != "ESTABLIHED" ]]
	then
		echo "[[$curr_date]] loading didn't work, need to factory reset"
		/sbin/jffs2reset -y -r
	fi
elif [[ "$ucentral_conn" != "ESTABLISHED" ]]
then
	echo "[[$curr_date]]  Ucentral either crashed or stopped, restarting the same"
        /etc/init.d/ucentral restart
else
        echo "[[$curr_date]]  Ucentral working all fine, nothing to do"
fi
