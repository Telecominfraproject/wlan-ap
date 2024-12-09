include $(TOPDIR)/rules.mk

ARCH:=aarch64
BOARD:=ipq53xx
BOARDNAME:=Qualcomm Technologies, Inc. IPQ53XX
FEATURES:=squashfs fpu ramdisk nand pcie
CPU_TYPE:=cortex-a53
CPU_SUBTYPE:=neon-vfpv4
SUBTARGETS:=generic ipq53xx_32


KERNELNAME:=Image dtbs
KERNEL_PATCHVER:=6.1

include $(INCLUDE_DIR)/target.mk

DEFAULT_PACKAGES += \
	e2fsprogs uboot-envtools kmod-gpio-button-hotplug kmod-button-hotplug \
	kmod-ata-core kmod-ata-ahci kmod-ata-ahci-platform kmod-usb3 \
	kmod-usb-phy-ipq807x kmod-usb-dwc3-qcom-internal \
	kmod-qca-nss-ppe kmod-qca-nss-ppe-ds kmod-qca-nss-ppe-vp \
	kmod-qca-ssdk-qca-nohnat kmod-qca-nss-dp-qca \
	kmod-qca-nss-ppe-bridge-mgr kmod-qca-nss-ppe-vlan-mgr \
	iwinfo kmod-ath12k-qca ath12k-firmware-qcn92xx \
	kmod-leds-gpio swconfig kmod-fs-ext4 losetup wpad-openssl \
	ucode-mod-nl80211 ucode-mod-rtnl

GENERIC_LINUX_CONFIG = $(PLATFORM_DIR)/generic/config-$(KERNEL_PATCHVER)-platform

define Kernel/Prepare
	$(LINUX_CAT) $(DL_DIR)/$(LINUX_SOURCE) | $(TAR) -C $(KERNEL_BUILD_DIR) $(TAR_OPTIONS)

	$(if $(QUILT),rm -rf $(LINUX_DIR)/patches; mkdir -p $(LINUX_DIR)/patches)
	$(if $(FILES_DIR),$(CP) $(FILES_DIR)/* $(LINUX_DIR)/)
	find $(LINUX_DIR)/ -name \*.rej -or -name \*.orig | $(XARGS) rm -f
	$(call PatchDir,$(LINUX_DIR),$(PATCH_DIR),platform/)

	$(if $(QUILT),touch $(LINUX_DIR)/.quilt_used)
endef

define Kernel/Configure
	$(call Kernel/Configure/Default)
	echo '# CONFIG_INITRAMFS_PRESERVE_MTIME is not set' >> $(LINUX_DIR)/.config
endef


$(eval $(call BuildTarget))
