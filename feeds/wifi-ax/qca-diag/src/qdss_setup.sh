#!/bin/bash
#
#  Copyright (c) 2019 Qualcomm Technologies, Inc.
#
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.
#
if [ $# -ne 1 ]
then
        echo "usage $0 {install | uninstall}"
        exit 1
fi
if [ $1 == "uninstall" ]
then
	echo "uninstalling"

	fw_setenv usb_mode
	sed -i '/sh \/diag.sh/d' /etc/rc.local
	rm -f /diag.sh

	exit 0
elif [ $1 == "install" ]
then
	echo "installing"

	fw_printenv | grep "usb_mode=peripheral"
	if [ $? -ne 0 ]
	then
		fw_setenv usb_mode "peripheral"
	fi

	sed -i '/sh \/diag.sh/d' /etc/rc.local
	sed -i '$ish /diag.sh' /etc/rc.local

	if [ -f /diag.sh ]
	then
		echo "Already installed returning"
		exit 1
	fi
	echo "mkdir -p /config" >> /diag.sh
	echo "mount -t configfs none /config" >> /diag.sh
	echo "mkdir -p /config/usb_gadget/g1" >> /diag.sh
	echo "echo 0x05c6 > /config/usb_gadget/g1/idVendor" >> /diag.sh
	echo "echo 0x9060 > /config/usb_gadget/g1/idProduct" >> /diag.sh
	echo "mkdir -p /config/usb_gadget/g1/strings/0x409" >> /diag.sh
	echo "echo 12345678 > /config/usb_gadget/g1/strings/0x409/serialnumber" >> /diag.sh
	echo "echo Dummy > /config/usb_gadget/g1/strings/0x409/manufacturer" >> /diag.sh
	echo "echo Demo > /config/usb_gadget/g1/strings/0x409/product" >> /diag.sh
	echo "mkdir -p /config/usb_gadget/g1/configs/c.1" >> /diag.sh
	echo "mkdir -p /config/usb_gadget/g1/configs/c.1/strings/0x409" >> /diag.sh
	echo "echo "Conf 1" > /config/usb_gadget/g1/configs/c.1/strings/0x409/configuration" >> /diag.sh
	echo "echo 120 > /config/usb_gadget/g1/configs/c.1/MaxPower" >> /diag.sh
	echo "mkdir -p /config/usb_gadget/g1/functions/diag.diag" >> /diag.sh
	echo "ln -s /config/usb_gadget/g1/functions/diag.diag /config/usb_gadget/g1/configs/c.1/diag.diag" >> /diag.sh
	echo "mkdir -p /config/usb_gadget/g1/functions/qdss.qdss" >> /diag.sh
	echo "ln -s /config/usb_gadget/g1/functions/qdss.qdss /config/usb_gadget/g1/configs/c.1/qdss.qdss" >> /diag.sh
	echo "mkdir -p /config/usb_gadget/g1/functions/qdss.qdss2" >> /diag.sh
	echo "ln -s /config/usb_gadget/g1/functions/qdss.qdss2 /config/usb_gadget/g1/configs/c.1/qdss.qdss2" >> /diag.sh
	echo "echo "8a00000.dwc3" > /config/usb_gadget/g1/UDC" >> /diag.sh
	echo "/usr/sbin/registerReboot &" >> /diag.sh

	exit 0
else
	echo "usage $0 {install | uninstall}"
	exit 1
fi
