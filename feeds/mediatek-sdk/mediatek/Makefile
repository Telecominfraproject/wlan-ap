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

PLATFORM_DIR:=${CURDIR}
GENERIC_BACKPORT_DIR := $(PLATFORM_DIR)/backport-$(KERNEL_PATCHVER)
GENERIC_PATCH_DIR := $(PLATFORM_DIR)/pending-$(KERNEL_PATCHVER)
GENERIC_HACK_DIR := $(PLATFORM_DIR)/hack-$(KERNEL_PATCHVER)
GENERIC_FILES_DIR := 
GENERIC_LINUX_CONFIG:=$(PLATFORM_DIR)/config-$(KERNEL_PATCHVER)-mediatek

include $(INCLUDE_DIR)/target.mk

DEFAULT_PACKAGES += \
	kmod-leds-gpio kmod-gpio-button-hotplug uboot-envtools

$(eval $(call BuildTarget))
