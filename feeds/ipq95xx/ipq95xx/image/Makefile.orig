include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

UBIFS_OPTS = -m 2048 -e 124KiB -c 4096 -U -F
UBI_OPTS = -m 2048 -p 128KiB

IPQ95XX_KERNEL_SIZE=6044k
IPQ95XX_ROOTFS_SIZE=40036k
IPQ95XX_32_KERNEL_LOADADDR = 0x42008000
IPQ95XX_KERNEL_LOADADDR = 0x42080000

define CompressLzma
	$(STAGING_DIR_HOST)/bin/lzma e $(1) -lc1 -lp2 -pb2 $(2)
endef

define sizecheck
	@[ $$(($(subst k,* 1024,$(subst m, * 1024k,$(1))))) -ge "$$(stat -c%s $(2))" ] || { \
		echo "WARNING: Image file $(2) is too big, size is $$(stat -c%s $(2)) compare to max allowed $(1)" >&2; \
		rm -f $(2); \
	}
endef

define Image/BuildKernel/FIT
	$(CP) $(KDIR)/Image $(BIN_DIR)/$(IMG_PREFIX)-vmlinux.bin
	gzip -9 -c $(KDIR)/Image > $(KDIR)/Image.gz
	$(call CompressLzma,$(KDIR)/Image,$(KDIR)/Image.lzma)
	$(call Image/BuildKernel/MkFIT,$(1), $(KDIR)/Image.lzma, $(DTS_DIR)/$(1).dtb,lzma,$(2),$(2))
	$(CP) $(KDIR)/fit-$(1).itb $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fit-uImage.itb
	mkdir -p $(BIN_DIR)/dtbs/
	$(CP) $(DTS_DIR)/ipq95*.dtb $(BIN_DIR)/dtbs/
	$(CP) $(KDIR)/Image $(BIN_DIR)/dtbs/
endef

define Image/BuildKernel/FITInitramfs
	$(CP) $(KDIR)/Image-initramfs $(BIN_DIR)/dtbs/
	$(CP) $(KDIR)/Image-initramfs $(BIN_DIR)/$(IMG_PREFIX)-vmlinux-initramfs.bin
	$(call Image/BuildKernel/MkFIT,$(1), $(KDIR)/Image-initramfs, $(DTS_DIR)/$(1).dtb, none,$(2),$(2))
	$(CP) $(KDIR)/fit-$(1).itb $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fit-uImage-initramfs.itb
endef

define Image/Prepare
	$(CP) $(LINUX_DIR)/vmlinux $(KDIR)/$(IMG_PREFIX)-vmlinux.elf
endef

define Image/BuildKernel/MultiDTBFIT
	$(CP) $(KDIR)/Image $(BIN_DIR)/$(IMG_PREFIX)-vmlinux.bin
	gzip -9 -c $(KDIR)/Image > $(KDIR)/Image.gz
	$(foreach dtb, $(2), gzip -9 -c $(DTS_DIR)/$(dtb).dtb > $(DTS_DIR)/$(dtb).dtb.gz;)
	$(call CompressLzma,$(KDIR)/Image,$(KDIR)/Image.lzma)
	$(call Image/BuildKernel/MkFITMulti,$(1),$(KDIR)/Image.lzma,$(2),lzma,$(3),$(3),$(4),$(5))
	$$(if $$(CONFIG_DEBUG), ,$$(call sizecheck, $(IPQ95XX_KERNEL_SIZE), $(KDIR)/fit-$(1).itb))
	$(CP) $(KDIR)/fit-$(1).itb $(BIN_DIR)/$(IMG_PREFIX)-$(1)-fit-uImage.itb
endef

# some tree's don't have all the device tree's so we filter out
# the ones that are not present
# $(1) is the filter to apply
FindDeviceTrees ?= $(notdir $(basename $(shell find $(DTS_DIR)/ -name $(1)*.dtb $(2) 2>/dev/null)))

# For imagebuilder builds, check if debug file exists before copying
DebugImageName ?= $(KDIR)/vmlinux$(subst openwrt-ipq95xx$(if $(SUBTARGET),-$(SUBTARGET)),,$(IMG_PREFIX)).debug
DebugFileCheck ?= $(shell if [ -f $(call DebugImageName) ];then echo "1"; fi)

define Image/BuildKernel/template
	$(if $(CONFIG_IB_STANDALONE), $(if $(call DebugFileCheck), $(CP) $(call DebugImageName) $(BIN_DIR)/$(IMG_PREFIX)-vmlinux.elf), $(CP) $(call DebugImageName) $(BIN_DIR)/$(IMG_PREFIX)-vmlinux.elf)
endef

define Image/BuildKernel/ipq95xx_32
	$(foreach device, $(call FindDeviceTrees, ipq9574-), \
		$(call Image/BuildKernel/$(1),$(device),$(IPQ95XX_32_KERNEL_LOADADDR));)

	$(call Image/BuildKernel/MultiDTBFIT,ipq9574-alxx, \
		$(call FindDeviceTrees, ipq9574-), $(IPQ95XX_32_KERNEL_LOADADDR))
endef

define Image/BuildKernel/generic
	$(foreach device, $(call FindDeviceTrees, ipq9574-), \
		$(call Image/BuildKernel/$(1),$(device),$(IPQ95XX_KERNEL_LOADADDR));)

	$(call Image/BuildKernel/MultiDTBFIT,ipq9574-alxx, \
		$(call FindDeviceTrees, ipq9574-), $(IPQ95XX_KERNEL_LOADADDR))
endef

define Image/BuildKernel
	$(call Image/BuildKernel/template,FIT)
	$(call Image/BuildKernel/$(SUBTARGET),FIT)
endef

define Image/BuildKernel/Initramfs
	$(CP) $(KDIR)/vmlinux-initramfs$(subst openwrt-ipq95xx$(if $(SUBTARGET),-$(SUBTARGET)),,$(IMG_PREFIX)).debug $(BIN_DIR)/$(IMG_PREFIX)-vmlinux-initramfs.elf
	$(call Image/BuildKernel/template,FITInitramfs)
	$(call Image/BuildKernel/$(SUBTARGET),FITInitramfs)
endef

define Image/Build/squashfs
	$(call prepare_generic_squashfs,$(KDIR)/root.squashfs)
	$$(if $$(CONFIG_DEBUG), ,$$(call sizecheck, $(IPQ95XX_ROOTFS_SIZE), $(KDIR)/root.squashfs))
endef

define Image/Build
	$(call Image/Build/$(1),$(1))
	dd if=$(KDIR)/root$(2).$(1) of=$(BIN_DIR)/$(IMG_PREFIX)-$(1)-root$(3).img bs=2k conv=sync
endef

$(eval $(call BuildImage))
