include $(TOPDIR)/rules.mk

ARCH:=aarch64
BOARD:=ipq53xx
BOARDNAME:=Qualcomm Technologies, Inc. IPQ53XX
FEATURES:=squashfs fpu ramdisk nand pcie
CPU_TYPE:=cortex-a53
CPU_SUBTYPE:=neon-vfpv4
SUBTARGETS:=generic ipq53xx_32


KERNELNAME:=Image dtbs
KERNEL_PATCHVER:=6.6
#KERNEL_NAME_SUFFIX=-0c8ca76b942082e176fe07ddf18a7a5413b1612a

include $(INCLUDE_DIR)/target.mk

DEFAULT_PACKAGES += \
	e2fsprogs uboot-envtools kmod-gpio-button-hotplug kmod-button-hotplug \
	kmod-ata-core kmod-ata-ahci kmod-ata-ahci-platform kmod-usb3 \
	kmod-usb-phy-ipq807x kmod-usb-dwc3-qcom-internal \
	kmod-qca-nss-ppe kmod-qca-nss-ppe-ds kmod-qca-nss-ppe-vp \
	kmod-qca-ssdk-qca-nohnat kmod-qca-nss-dp-qca \
	kmod-qca-nss-ppe-bridge-mgr kmod-qca-nss-ppe-vlan-mgr \
	iwinfo kmod-ath12k-qca ath12k-firmware-qcn92xx kmod-bootconfig \
	kmod-leds-gpio swconfig kmod-fs-ext4 losetup wpad-openssl \
	ucode-mod-nl80211 ucode-mod-rtnl -procd-ujail

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
	#echo '# CONFIG_INITRAMFS_PRESERVE_MTIME is not set' >> $(LINUX_DIR)/.config
	#echo '# CONFIG_ATHMEMDEBUG is not set' >> $(LINUX_DIR)/.config
	#echo 'CONFIG_NFT_NPT=m' >> $(LINUX_DIR)/.config
	#echo 'CONFIG_SKB_RECYCLE_SIZE=2304' >> $(LINUX_DIR)/.config
	#echo '# CONFIG_SKB_TIMESTAMP is not set' >> $(LINUX_DIR)/.config
	#echo '# CONFIG_SKB_FAST_RECYCLABLE_DEBUG_ENABLE is not set' >> $(LINUX_DIR)/.config
	#echo '# CONFIG_DEFAULT_QDISC_DISABLE is not set' >> $(LINUX_DIR)/.config
	#echo 'CONFIG_HWMON=y' >> $(LINUX_DIR)/.config
	#echo 'CONFIG_RTK_MSSDK_PHY=y' >> $(LINUX_DIR)/.config
	#echo 'CONFIG_REALTEK_PHY=y' >> $(LINUX_DIR)/.config
	#echo '# CONFIG_QCOM_SCM_DOWNLOAD_MODE_DEFAULT is not set' >> $(LINUX_DIR)/.config
	#echo 'CONFIG_QCOM_NON_SECURE_PIL=y' >> $(LINUX_DIR)/.config
endef


$(eval $(call BuildTarget))
