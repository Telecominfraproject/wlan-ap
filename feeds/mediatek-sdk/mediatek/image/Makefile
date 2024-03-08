# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2012-2015 OpenWrt.org
# Copyright (C) 2016-2017 LEDE project

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

define Build/sysupgrade-emmc
	rm -f $@.recovery
	mkfs.fat -C $@.recovery 3070

	./gen_$(SUBTARGET)_emmc_img.sh $@ \
		$(IMAGE_KERNEL) \
		$@.recovery \
		$(IMAGE_ROOTFS)
endef

define Build/fdt-patch-dm-crypt
	BIN=$(STAGING_DIR_HOST)/bin \
	LIBFDT_PATH=$(STAGING_DIR_HOST)/lib \
	$(TOPDIR)/scripts/fdt-patch-dm-crypt.sh \
		$(KDIR)/image-sb-$(firstword $(DEVICE_DTS)).dtb \
		$(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME))-hashed-summary-$(firstword $(DEVICE_DTS))
endef

define Build/squashfs-encrypt
	BIN=$(STAGING_DIR_HOST)/bin \
	$(TOPDIR)/scripts/make-squashfs-encrypted.sh \
	$(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME)) \
	$(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME))-encrypted-$(firstword $(DEVICE_DTS)) \
	$(ROE_KEY_DIR) \
	$@
endef

# build squashfs-hashed
define Build/squashfs-hashed
	$(CP) $(if $(ROOTFS_ENCRYPT), \
		$(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME))-encrypted-$(firstword $(DEVICE_DTS)), \
		$(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME))) \
		$(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME))-hashed-$(firstword $(DEVICE_DTS))
	$(TOPDIR)/scripts/make-squashfs-hashed.sh \
		$(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME))-hashed-$(firstword $(DEVICE_DTS)) \
		$(STAGING_DIR_HOST) \
		$(TOPDIR) \
		$(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME))-hashed-summary-$(firstword $(DEVICE_DTS))
	fdt-patch-dm-verify $(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME))-hashed-summary-$(firstword $(DEVICE_DTS)) \
		$(KDIR)/image-$(firstword $(DEVICE_DTS)).dtb $(KDIR)/image-sb-$(firstword $(DEVICE_DTS)).dtb \
		$(HASHED_BOOT_DEVICE)
	$(if $(ROOTFS_ENCRYPT),$(call Build/fdt-patch-dm-crypt))
endef

# build fw-ar-ver
get_fw_ar_ver = \
  $(if $(wildcard $(2)),$(shell rm -rf $(2))) \
  $(if $(wildcard $(1)),$(info $(shell $(STAGING_DIR_HOST)/bin/ar-tool fw_ar_table create_ar_conf $(1) $(2)))) \
  $(if $(wildcard $(2)),$(eval include $(2))) \
  $(if $(FW_AR_VER),$(info FW_AR_VER = $(FW_AR_VER)))

define Build/fw-ar-ver
	$(call get_fw_ar_ver,$(ANTI_ROLLBACK_TABLE),$(AUTO_AR_CONF))
endef

define Build/rfsk-encrypt
	BIN=$(STAGING_DIR_HOST)/bin \
	$(TOPDIR)/scripts/enc-rfsk.sh \
		-d $(ROE_KEY_DIR) \
		-f $@ \
		-k $(ROE_KEY_DIR)/$(ROE_KEY_NAME) \
		-s $(dir $@)
endef

define Build/fit-secret
	BIN=$(STAGING_DIR_HOST)/bin \
	LIBFDT_PATH=$(STAGING_DIR_HOST)/lib \
	$(TOPDIR)/scripts/enc-rfsk.sh \
		-c "config-1" \
		-d $(ROE_KEY_DIR) \
		-f $@ \
		-k $(ROE_KEY_DIR)/$(ROE_KEY_NAME) \
		-s $(dir $@)
endef

define Build/rootfs-encrypt
	$(if $(ROOTFS_ENCRYPT),$(call Build/rfsk-encrypt))
	$(if $(ROOTFS_ENCRYPT),$(call Build/squashfs-encrypt))
endef

define append-opteenode
	LD_LIBRARY_PATH=$(STAGING_DIR_HOST)/lib \
	fdtput $(1) "/reserved-memory/secmon@43000000" -tx reg 0 0x43000000 0 0x11f0000
	LD_LIBRARY_PATH=$(STAGING_DIR_HOST)/lib \
	fdtput $(1) /firmware -cp
	LD_LIBRARY_PATH=$(STAGING_DIR_HOST)/lib \
	fdtput $(1) /firmware/optee -cp
	LD_LIBRARY_PATH=$(STAGING_DIR_HOST)/lib \
	fdtput $(1) /firmware/optee -ts compatible "linaro,optee-tz"
	LD_LIBRARY_PATH=$(STAGING_DIR_HOST)/lib \
	fdtput $(1) /firmware/optee -ts method "smc"
	LD_LIBRARY_PATH=$(STAGING_DIR_HOST)/lib \
	fdtput $(1) /firmware/optee -ts status "okay"
endef

define Build/append-opteenode
	$(if $(CONFIG_PACKAGE_optee-mediatek),$(call append-opteenode, $(1)))
endef
# build signed fit
define Build/fit-sign
	$(TOPDIR)/scripts/mkits.sh \
		-D $(DEVICE_NAME) \
		-o $@.its \
		-k $@ \
		$(if $(word 2,$(1)),-d $(word 2,$(1))) -C $(word 1,$(1)) \
		-a $(KERNEL_LOADADDR) \
		-e $(if $(KERNEL_ENTRY),$(KERNEL_ENTRY),$(KERNEL_LOADADDR)) \
		-c $(if $(DEVICE_DTS_CONFIG),$(DEVICE_DTS_CONFIG),"config-1") \
		-A $(LINUX_KARCH) \
		-v $(LINUX_VERSION) \
		$(if $(FIT_KEY_NAME),-S $(FIT_KEY_NAME)) \
		$(if $(FIT_KEY_ALG),-b $(FIT_KEY_ALG)) \
		$(if $(FW_AR_VER),-r $(FW_AR_VER)) \
		$(if $(CONFIG_TARGET_ROOTFS_SQUASHFS), \
			$(if $(ROOTFS_ENCRYPT), \
				-R $(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME))-hashed-$(firstword $(DEVICE_DTS)), \
				-R $(ROOTFS/$(FILESYSTEMS)/$(DEVICE_NAME)))) \
		$(if $(ROOTFS_ENCRYPT),-m $(addsuffix -rfsk.enc,$(basename $@)))
	PATH=$(LINUX_DIR)/scripts/dtc:$(PATH) mkimage \
		-f $@.its \
		$(if $(FIT_KEY_DIR),-k $(FIT_KEY_DIR)) \
		-r \
		$@.new
	@mv $@.new $@
	$(if $(ROOTFS_ENCRYPT),$(call Build/fit-secret))
endef

# default all platform image(fit) build 
define Device/Default
  PROFILES = Default $$(DEVICE_NAME)
  KERNEL_NAME := Image
  KERNEL = kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS = kernel-bin | lzma | \
	fit lzma $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  FILESYSTEMS := squashfs
  DEVICE_DTS_DIR := $(DTS_DIR)
  IMAGES := sysupgrade.bin
  IMAGE/sysupgrade.bin := append-kernel | pad-to 128k | append-rootfs | \
	pad-rootfs | append-metadata
  FIT_KEY_DIR :=
  FIT_KEY_NAME :=
  FIT_KEY_ALG :=
  ROE_KEY_DIR :=
  ROE_KEY_NAME :=
endef

include $(SUBTARGET).mk

define Image/Build
	$(call Image/Build/$(1),$(1))
endef

$(eval $(call BuildImage))

