# 
# Copyright (C) 2006-2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

BlockSize=128k
ifeq ($(CONFIG_MTD_NAND_RTK_BLOCK_SIZE),0x80000)
BlockSize=512k
endif
ifeq ($(CONFIG_MTD_NAND_RTK_BLOCK_SIZE),0x20000)
BlockSize=128k
endif

define mkcmdline
board=$(1) console=$(2),$(3) linuxpart=0x$(4) hwpart=0x$(5)
endef

FWHW_VERSION_STRING=$(call qstrip, $(CONFIG_VERSION_PRODUCT)).v$(call qstrip,$(CONFIG_VERSION_NUMBER))$(if $(call qstrip,$(CONFIG_VERSION_PHASE)),-$(call qstrip,$(CONFIG_VERSION_PHASE))).r$(BUILD_REVISION) $(call qstrip,$(CONFIG_VERSION_BOARD))

SINGLE_PROFILES :=

IMAGE_PREFIX=openwrt
define SingleProfile

  define Image/Build/Profile/$(1)/initramfs
	$(call Image/BuildLoader,loader-$(SUBTARGET)-$(1),bin,$(call mkcmdline,$(1),$(2),$(3)) $(4),$(5))
	cvimg-$(SUBTARGET) linux $(KDIR)/loader-$(SUBTARGET)-$(1).bin $(BIN_DIR)/$(IMG_PREFIX)-$(1)-ramfs.bin $(5) $(6)
  endef
ifeq ($(CONFIG_MTD_NAND_RTK),y)
  define Image/Build/Profile/$(1)/squashfs
	$(call Image/BuildLoader,loader-$(SUBTARGET)-$(1),bin,$(call mkcmdline,$(1),$(2),$(3),$(6),$(7)),$(5))

	cvimg-$(SUBTARGET) linux $(KDIR)/loader-$(SUBTARGET)-$(1).bin $(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux.bin $(5) $(6)
	dd if=$(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux.bin of=$(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux_4k.bin bs=$(BlockSize) conv=sync
	cat $(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux_4k.bin $(KDIR)/root.squashfs-4k > $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fw_4k_cat.bin
	cvimg-$(SUBTARGET) fix_chksum  $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fw_4k_cat.bin $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fw.bin
	rm -rf $(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux_4k.bin $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fw_4k_cat.bin $(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux.bin
  endef
else
  define Image/Build/Profile/$(1)/squashfs
	$(call Image/BuildLoader,loader-$(SUBTARGET)-$(1),bin,$(call mkcmdline,$(1),$(2),$(3),$(6),$(7)),$(5))

	cvimg-$(SUBTARGET) linux $(KDIR)/loader-$(SUBTARGET)-$(1).bin $(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux.bin $(5) $(6)
	echo -n '$(FWHW_VERSION_STRING)' > $(BIN_DIR)/FWHW_FILE
	dd if=/dev/zero of=$(BIN_DIR)/FWHW_FILE bs=1 count=0 seek=64

	dd if=$(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux.bin of=$(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux_4k.bin bs=4k conv=sync
	cat $(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux_4k.bin $(KDIR)/root.squashfs-4k > $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fw_4k_cat.bin
	cvimg-$(SUBTARGET) fix_chksum  $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fw_4k_cat.bin $(BIN_DIR)/$(IMG_PREFIX)-$(1)-4k-fw.bin
	cat $(BIN_DIR)/FWHW_FILE >> $(BIN_DIR)/$(IMG_PREFIX)-$(1)-4k-fw.bin
	mv $(BIN_DIR)/$(IMG_PREFIX)-$(1)-4k-fw.bin $(BIN_DIR)/$(IMG_PREFIX)$(if $(PROFILE_SANITIZED),-$(PROFILE_SANITIZED)).bin
	rm -rf $(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux_4k.bin $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fw_4k_cat.bin

    ifeq ($(1),SP-W2M-AC1200)
	dd if=$(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux.bin of=$(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux_64k.bin bs=64k conv=sync
	cat $(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux_64k.bin $(KDIR)/root.squashfs-64k > $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fw_64k_cat.bin
	cvimg-$(SUBTARGET) fix_chksum  $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fw_64k_cat.bin $(BIN_DIR)/$(IMG_PREFIX)-$(1)-64k-fw.bin
	cat $(BIN_DIR)/FWHW_FILE >> $(BIN_DIR)/$(IMG_PREFIX)-$(1)-64k-fw.bin
	mv $(BIN_DIR)/$(IMG_PREFIX)-$(1)-64k-fw.bin $(BIN_DIR)/$(IMG_PREFIX)$(if $(PROFILE_SANITIZED),-$(PROFILE_SANITIZED))_64k.bin
	rm -rf $(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux_64k.bin $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fw_64k_cat.bin
    endif

	rm -f $(BIN_DIR)/FWHW_FILE $(BIN_DIR)/$(IMG_PREFIX)-$(1)-linux.bin
  endef

endif
  SINGLE_PROFILES += $(1)

endef

define MultiProfile

  define Image/Build/Profile/$(1)/initramfs
	$(foreach p,$(2),
		$$(call Image/Build/Profile/$p/initramfs)
	)
  endef

  define Image/Build/Profile/$(1)/squashfs
	$(foreach p,$(2),
		$$(call Image/Build/Profile/$p/squashfs)
	)
  endef

endef

define Image/Prepare
	lzma e $(KDIR)/vmlinux -lc1 -lp2 -pb2 $(KDIR)/vmlinux.bin.lzma
endef

LOADER_MAKE := $(NO_TRACE_MAKE) -C lzma-loader KDIR=$(KDIR) LINUX_DIR=$(LINUX_DIR)

define Build/Clean
	$(LOADER_MAKE) clean
endef

define Image/BuildLoader
	-rm -rf $(KDIR)/lzma-loader
	$(LOADER_MAKE) LOADER=$(1).$(2) KERNEL_CMDLINE="$(3)" \
		LZMA_TEXT_START=$(4) LOADADDR=0x80000000 \
		LOADER_DATA="$(KDIR)/vmlinux.bin.lzma" BOARD="$(1)" \
		compile loader.$(2)
endef

#
# rtl819xd profiles
#
# 98c move default linux 60000
ifeq ($(CONFIG_TARGET_rtkmips_rtl8198c),y)
# linun partition offset = 0x6000 
# hw setting parttion offset = 0x20000
ifeq ($(CONFIG_MTD_NAND_RTK),y)
$(eval $(call SingleProfile,AP,ttyS0,38400,root=/dev/mtdblock3,0x80500000,800000,0))
else
$(eval $(call SingleProfile,AP,ttyS0,38400,root=/dev/mtdblock2,0x80500000,60000,20000))
endif
endif

ifeq ($(CONFIG_TARGET_rtkmips_rtl8954e),y)
# linun partition offset = 0x6000
# hw setting parttion offset = 0x20000
ifeq ($(CONFIG_MTD_NAND_RTK),y)
$(eval $(call SingleProfile,AP,ttyS0,38400,root=/dev/mtdblock3,0x80500000,800000,0))
else
$(eval $(call SingleProfile,AP,ttyS0,38400,root=/dev/mtdblock2,0x80500000,60000,20000))
endif
endif 

ifeq ($(CONFIG_TARGET_rtkmipsel_rtl8197f),y)
# linun partition offset = 0x6000
# hw setting parttion offset = 0x20000
ifeq ($(CONFIG_MTD_NAND_RTK),y)
$(eval $(call SingleProfile,AP,ttyS0,38400,root=/dev/mtdblock3,0x80500000,800000,0))
else
$(eval $(call SingleProfile,AP,ttyS0,38400,root=/dev/mtdblock2,0x80500000,60000,20000))
$(eval $(call SingleProfile,rtk_IOT,ttyS0,38400,root=/dev/mtdblock2,0x80500000,60000,20000))
$(eval $(call SingleProfile,SP-W2M-AC1200,ttyS0,38400,root=/dev/mtdblock2,0x80500000,60000,20000))
$(eval $(call SingleProfile,SP-W2M-AC1200-POE,ttyS0,38400,root=/dev/mtdblock2,0x80500000,60000,20000))
endif
endif 


#

# RAMFS
define Image/Build/Initramfs
	$(call Image/Build/Profile/$(PROFILE)/initramfs)
endef

# SQUASHFS
ifeq ($(CONFIG_MTD_NAND_RTK),y)
define Image/Build
	dd if=$(KDIR)/root.squashfs of=$(KDIR)/root.squashfs-4k bs=$(BlockSize) conv=sync
	$(call add_jffs2_mark,$(KDIR)/root.squashfs-4k)
	$(call Image/Build/Profile/$(PROFILE)/squashfs)
endef
else
define Image/Build	
	dd if=$(KDIR)/root.squashfs of=$(KDIR)/root.squashfs-4k bs=4k conv=sync
	$(call add_jffs2_mark,$(KDIR)/root.squashfs-4k)
	dd if=$(KDIR)/root.squashfs of=$(KDIR)/root.squashfs-64k bs=64k conv=sync
	$(call add_jffs2_mark,$(KDIR)/root.squashfs-64k)
	$(call Image/Build/Profile/$(PROFILE)/squashfs)
endef
endif

$(eval $(call BuildImage))
