include $(TOPDIR)/rules.mk

ARCH:=aarch64
BOARD:=ipq95xx
BOARDNAME:=Qualcomm Technologies, Inc IPQ95xx
FEATURES:=ubifs squashfs fpu ramdisk nand pcie usbgadget
CPU_TYPE:=cortex-a73
CPU_SUBTYPE:=neon-vfpv4
MAINTAINER:=John Crispin <blogic@openwrt.org>
SUBTARGETS:=generic ipq95xx_32

KERNELNAME:=zImage Image dtbs

KERNEL_PATCHVER:=5.4
KERNEL_NAME_SUFFIX=-qsdk-b2d40c94fad765a48c03f492d669aeecbbb9b617

include $(INCLUDE_DIR)/target.mk
DEFAULT_PACKAGES += \
	uboot-envtools kmod-leds-gpio kmod-gpio-button-hotplug kmod-button-hotplug \
	kmod-usb3 swconfig kmod-qca-nss-dp \
	kmod-usb-phy-ipq807x kmod-usb-dwc3-qcom-internal \
	kmod-qca-nss-ppe kmod-qca-ssdk-nohnat kmod-qca-psdk \
	kmod-gpio-button-hotplug iwinfo uboot-envtools swconfig \
	kmod-ath11k-ahb ath11k-firmware-ipq95xx \
	kmod-ath12k ath12k-firmware-qcn92xx wpad-openssl \
	-procd-ujail

$(eval $(call BuildTarget))
