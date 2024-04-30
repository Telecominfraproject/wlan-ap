include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=ipq40xx
BOARDNAME:=Qualcomm Atheros IPQ40XX
FEATURES:=squashfs fpu ramdisk nand
CPU_TYPE:=cortex-a7
CPU_SUBTYPE:=neon-vfpv4
SUBTARGETS:=generic mikrotik

KERNEL_PATCHVER:=5.4
KERNEL_TESTING_PATCHVER:=5.4
DEVICE_TYPE:=qsdk

KERNELNAME:=zImage Image dtbs

GENERIC_BACKPORT_DIR := ${CURDIR}/backport-$(KERNEL_PATCHVER)
GENERIC_PATCH_DIR := ${CURDIR}/pending-$(KERNEL_PATCHVER)
GENERIC_HACK_DIR := ${CURDIR}/hack-$(KERNEL_PATCHVER)
GENERIC_FILES_DIR := 
GENERIC_LINUX_CONFIG:=${CURDIR}/config-$(KERNEL_PATCHVER)-ipq40xx

include $(INCLUDE_DIR)/target.mk
DEFAULT_PACKAGES += \
	kmod-usb-dwc3-qcom \
	kmod-leds-gpio kmod-gpio-button-hotplug swconfig \
	kmod-ath10k-ct wpad-basic-wolfssl \
	kmod-usb3 kmod-usb-dwc3 ath10k-firmware-qca4019-ct \
	uboot-envtools

$(eval $(call BuildTarget))
