include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=ipq807x
BOARDNAME:=Qualcomm Atheros AX
SUBTARGETS:=ipq807x ipq60xx ipq50xx
FEATURES:=squashfs ramdisk nand pcie usb
KERNELNAME:=Image dtbs
CPU_TYPE:=cortex-a7

KERNEL_PATCHVER:=4.4
KERNEL_NAME_SUFFIX=-qsdk-11f09717303ecd83c3a64e9efe23f25921dc1016

include $(INCLUDE_DIR)/target.mk
DEFAULT_PACKAGES += kmod-qca-nss-dp kmod-qca-ssdk swconfig \
	kmod-qca-nss-drv \
	kmod-usb-phy-ipq807x kmod-usb-dwc3-of-simple \
	kmod-ath11k-ahb kmod-qrtr_mproc wpad \
	kmod-gpio-button-hotplug \
	qca-thermald-10.4 qca-ssdk-shell kmod-qca-nss-drv-bridge-mgr \
	uboot-envtools

$(eval $(call BuildTarget))
