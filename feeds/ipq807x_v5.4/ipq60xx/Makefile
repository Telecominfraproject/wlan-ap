include $(TOPDIR)/rules.mk

ARCH:=aarch64
BOARD:=ipq60xx
BOARDNAME:=Qualcomm Technologies, Inc IPQ60xx
FEATURES:=ubifs squashfs fpu ramdisk nand pcie usbgadget
CPU_TYPE:=cortex-a53
CPU_SUBTYPE:=neon-vfpv4
MAINTAINER:=John Crispin <blogic@openwrt.org>
SUBTARGETS:=generic ipq60xx_32

KERNELNAME:=zImage Image dtbs

KERNEL_PATCHVER:=5.4
KERNEL_NAME_SUFFIX=-qsdk-26349818b464f8c7b52d59ce73579d9f3dd6bd5d

include $(INCLUDE_DIR)/target.mk
DEFAULT_PACKAGES += kmod-qca-nss-dp kmod-qca-ssdk swconfig \
	kmod-qca-nss-drv qca-nss-fw-ipq60xx \
	kmod-usb-dwc3-qcom \
	kmod-ath11k-ahb  ath11k-firmware-ipq60xx \
	kmod-gpio-button-hotplug iwinfo \
	qca-ssdk-shell kmod-qca-nss-drv-bridge-mgr \
	uboot-envtools

$(eval $(call BuildTarget))
