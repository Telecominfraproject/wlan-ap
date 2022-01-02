KERNEL_LOADADDR := 0x48080000

define Device/mt7986a-ax6000-nor-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax6000-nor-rfb
  DEVICE_DTS := mt7986a-nor-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
endef
TARGET_DEVICES += mt7986a-ax6000-nor-rfb

define Device/mt7986a-ax6000-spim-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax6000-spim-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986a-spim-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986a-spim-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986a-ax6000-spim-nand-rfb

define Device/mt7986a-ax8400-spim-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax8400-spim-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986a-spim-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986a-spim-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986a-ax8400-spim-nand-rfb

define Device/mt7986a-ax6000-snfi-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax6000-snfi-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986a-snfi-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986a-snfi-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986a-ax6000-snfi-nand-rfb

define Device/mt7986a-ax6000-emmc-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax6000-emmc-rfb
  DEVICE_DTS := mt7986a-emmc-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986a-emmc-rfb
  DEVICE_PACKAGES := mkf2fs e2fsprogs blkid blockdev losetup kmod-fs-ext4 \
		     kmod-mmc kmod-fs-f2fs kmod-fs-vfat kmod-nls-cp437 \
		     kmod-nls-iso8859-1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986a-ax6000-emmc-rfb

define Device/mt7986a-ax6000-2500wan-nor-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax6000-2500wan-nor-rfb
  DEVICE_DTS := mt7986a-2500wan-nor-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
endef
TARGET_DEVICES += mt7986a-ax6000-2500wan-nor-rfb

define Device/mt7986a-ax6000-2500wan-spim-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax6000-2500wan-spim-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986a-2500wan-spim-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986a-2500wan-spim-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986a-ax6000-2500wan-spim-nand-rfb

define Device/mt7986a-ax8400-2500wan-spim-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax8400-2500wan-spim-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986a-2500wan-spim-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986a-2500wan-spim-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986a-ax8400-2500wan-spim-nand-rfb

define Device/mt7986a-ax7800-nor-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax7800-nor-rfb
  DEVICE_DTS := mt7986a-nor-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
endef
TARGET_DEVICES += mt7986a-ax7800-nor-rfb

define Device/mt7986a-ax7800-spim-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax7800-spim-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986a-spim-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986a-spim-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986a-ax7800-spim-nand-rfb

define Device/mt7986a-ax7800-2500wan-nor-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax7800-2500wan-nor-rfb
  DEVICE_DTS := mt7986a-2500wan-nor-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
endef
TARGET_DEVICES += mt7986a-ax7800-2500wan-nor-rfb

define Device/mt7986a-ax7800-2500wan-spim-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986a-ax7800-2500wan-spim-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986a-2500wan-spim-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986a-2500wan-spim-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986a-ax7800-2500wan-spim-nand-rfb

define Device/mt7986b-ax6000-nor-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986b-ax6000-nor-rfb
  DEVICE_DTS := mt7986b-nor-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
endef
TARGET_DEVICES += mt7986b-ax6000-nor-rfb

define Device/mt7986b-ax6000-spim-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986b-ax6000-spim-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986b-spim-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986b-spim-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986b-ax6000-spim-nand-rfb

define Device/mt7986b-ax6000-snfi-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986b-ax6000-snfi-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986b-snfi-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986b-snfi-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986b-ax6000-snfi-nand-rfb

define Device/mt7986b-ax6000-emmc-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986b-ax6000-emmc-rfb
  DEVICE_DTS := mt7986b-emmc-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986b-emmc-rfb
  DEVICE_PACKAGES := mkf2fs e2fsprogs blkid blockdev losetup kmod-fs-ext4 \
		     kmod-mmc kmod-fs-f2fs kmod-fs-vfat kmod-nls-cp437 \
		     kmod-nls-iso8859-1
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986b-ax6000-emmc-rfb

define Device/mt7986b-ax6000-2500wan-nor-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986b-ax6000-2500wan-nor-rfb
  DEVICE_DTS := mt7986b-2500wan-nor-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
endef
TARGET_DEVICES += mt7986b-ax6000-2500wan-nor-rfb

define Device/mt7986b-ax6000-2500wan-spim-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986b-ax6000-2500wan-spim-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986b-2500wan-spim-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986b-2500wan-spim-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986b-ax6000-2500wan-spim-nand-rfb

define Device/mt7986b-ax6000-2500wan-snfi-nand-rfb
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := mt7986b-ax6000-2500wan-snfi-nand-rfb (SPI-NAND,UBI)
  DEVICE_DTS := mt7986b-2500wan-snfi-nand-rfb
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986b-2500wan-snfi-nand-rfb
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mt7986b-ax6000-2500wan-snfi-nand-rfb

define Device/mediatek_mt7986-fpga
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := MTK7986 FPGA
  DEVICE_DTS := mt7986-fpga
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  IMAGE/sysupgrade.bin := append-kernel | pad-to 256k | \
       append-rootfs | pad-rootfs | append-metadata
endef
TARGET_DEVICES += mediatek_mt7986-fpga

define Device/mediatek_mt7986-fpga-ubi
  DEVICE_VENDOR := MediaTek
  DEVICE_MODEL := MTK7986 FPGA (UBI)
  DEVICE_DTS := mt7986-fpga-ubi
  DEVICE_DTS_DIR := $(DTS_DIR)/mediatek
  SUPPORTED_DEVICES := mediatek,mt7986-fpga,ubi
  UBINIZE_OPTS := -E 5
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  IMAGE_SIZE := 65536k
  KERNEL_IN_UBI := 1
  IMAGES += factory.bin
  IMAGE/factory.bin := append-ubi | check-size $$$$(IMAGE_SIZE)
  IMAGE/sysupgrade.bin := sysupgrade-tar | append-metadata
endef
TARGET_DEVICES += mediatek_mt7986-fpga-ubi
