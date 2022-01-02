#
# Copyright (C) 2015 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/rtk_IOT
	NAME:=rtk_IOT
	PACKAGES:=\
		kmod-usb-core kmod-usb2 kmod-usb-ohci
endef

define Profile/rtk_IOT/Description
	Default package set compatible with most boards.
endef
$(eval $(call Profile,rtk_IOT))
