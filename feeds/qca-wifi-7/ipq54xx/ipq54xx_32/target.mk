
ARCH:=arm
SUBTARGET:=ipq54xx_32
BOARDNAME:=QTI IPQ54xx(32bit) based boards
CPU_TYPE:=cortex-a7

define Target/Description
	Build firmware image for IPQ54xx SoC devices.
endef

DEFAULT_PACKAGES += \
	uboot-ipq5424-mmc32 \
	uboot-ipq5424-norplusmmc32 \
	uboot-ipq5424-nand32 \
	uboot-ipq5424-norplusnand32 \
	uboot-ipq5424-tiny_nand32 \
	uboot-ipq5424-tiny_norplusnand32 \
	uboot-2025-ipq5424-mmc32 \
	uboot-2025-ipq5424-norplusmmc32 \
	uboot-2025-ipq5424-nand32 \
	uboot-2025-ipq5424-norplusnand32 \
	uboot-2025-ipq5424-tiny_nand32 \
	uboot-2025-ipq5424-tiny_norplusnand32
