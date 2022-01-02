#
# Copyright (C) 2006-2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

ARCH:=mipsel
BOARD:=rtkmipsel
BOARDNAME:=Realtek mipsel SOC
SUBTARGETS:=rtl8197f
#FEATURES:=squashfs usb
FEATURES:=squashfs usb
#LINUX_VERSION:=3.10.49
KERNEL_PATCHVER:=3.18
KERNEL_NAME_SUFFIX=-rtkmipsel

include $(INCLUDE_DIR)/target.mk

#DEFAULT_PACKAGES += kmod-gpio-button-hotplug

define Target/Description
	Build firmware images for Realtek based boards
endef

$(eval $(call BuildTarget))
