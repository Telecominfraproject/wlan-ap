include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=ipq807x
BOARDNAME:=Qualcomm Atheros AX
SUBTARGETS:=ipq807x ipq60xx ipq50xx
FEATURES:=squashfs ramdisk nand pcie usb usbgadget
KERNELNAME:=Image dtbs
CPU_TYPE:=cortex-a7

KERNEL_PATCHVER:=5.4
KERNEL_NAME_SUFFIX=-qsdk-d5fcb18e5420670c8734c6a659873e73adab6dac

include $(INCLUDE_DIR)/target.mk
DEFAULT_PACKAGES += kmod-qca-nss-dp kmod-qca-ssdk swconfig \
	kmod-qca-nss-drv \
	kmod-usb-phy-ipq807x kmod-usb-dwc3-qcom-internal \
	kmod-ath11k-ahb kmod-qrtr_mproc wpad \
	kmod-gpio-button-hotplug \
	qca-thermald-10.4 qca-ssdk-shell kmod-qca-nss-drv-bridge-mgr \
	uboot-envtools

$(eval $(call BuildTarget))
