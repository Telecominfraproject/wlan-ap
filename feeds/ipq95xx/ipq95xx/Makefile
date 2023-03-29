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
KERNEL_NAME_SUFFIX=-qsdk-879aa8d1f540d2357674beed0069e4450946b831

include $(INCLUDE_DIR)/target.mk
DEFAULT_PACKAGES += \
	uboot-envtools kmod-leds-gpio kmod-gpio-button-hotplug kmod-button-hotplug \
	kmod-ata-core kmod-ata-ahci kmod-ata-ahci-platform kmod-usb3 swconfig \
	kmod-usb-phy-ipq807x kmod-usb-dwc3-qcom-internal \
	kmod-qca-nss-ppe kmod-qca-ssdk-nohnat kmod-qca-psdk \
	kmod-qca-nss-dp kmod-qca-nss-ppe-bridge-mgr kmod-qca-nss-ppe-vlan-mgr \
	kmod-gpio-button-hotplug iwinfo uboot-envtools swconfig \
	kmod-ath12k ath12k-firmware-qcn92xx wpad-openssl

$(eval $(call BuildTarget))
