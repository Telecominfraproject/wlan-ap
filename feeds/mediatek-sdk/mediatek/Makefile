# Copyright (c) 2015 OpenWrt.org
#
include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=mediatek
BOARDNAME:=MediaTek Ralink ARM
SUBTARGETS:=mt7622 mt7623 mt7629 mt7986 mt7981 mt7988
FEATURES:=squashfs nand ramdisk fpu

KERNEL_PATCHVER:=5.4
KERNEL_TESTING_PATCHVER:=5.4

DEVICE_TYPE:=fw3

GENERIC_BACKPORT_DIR := ${CURDIR}/backport-$(KERNEL_PATCHVER)
GENERIC_PATCH_DIR := ${CURDIR}/pending-$(KERNEL_PATCHVER)
GENERIC_HACK_DIR := ${CURDIR}/hack-$(KERNEL_PATCHVER)
GENERIC_FILES_DIR := 
GENERIC_LINUX_CONFIG:=${CURDIR}/config-$(KERNEL_PATCHVER)-mediatek

include $(INCLUDE_DIR)/target.mk

DEFAULT_PACKAGES += \
	kmod-leds-gpio kmod-gpio-button-hotplug uboot-envtools

$(eval $(call BuildTarget))
