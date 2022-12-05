include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

IMG_PREFIX:=$(VERSION_DIST_SANITIZED)-$(IMG_PREFIX_VERNUM)$(IMG_PREFIX_VERCODE)$(IMG_PREFIX_EXTRA)$(BOARD)

# default all platform image(fit) build 
define Device/Default
  PROFILES = Default $$(DEVICE_NAME)
  FILESYSTEMS := squashfs
  DEVICE_DTS_DIR := $(DTS_DIR)/qcom
  KERNEL_IN_UBI := 1
  ROOTFSNAME_IN_UBI := ubi_rootfs
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGES := sysupgrade.tar nand-factory.bin
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE/nand-factory.bin := append-ubi | qsdk-ipq-factory-nand
  KERNEL_NAME := Image
  KERNEL = kernel-bin | gzip | fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  #KERNEL_INITRAMFS = kernel-bin | gzip | fit gzip $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
  KERNEL_INITRAMFS = kernel-bin | fit none $$(KDIR)/image-$$(firstword $$(DEVICE_DTS)).dtb
endef

include ipq60xx.mk

$(eval $(call BuildImage))
