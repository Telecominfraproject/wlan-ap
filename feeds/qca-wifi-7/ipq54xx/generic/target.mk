SUBTARGET:=generic
BOARDNAME:=QTI IPQ54xx(64bit) based boards
CPU_TYPE:=cortex-a55

define Target/Description
	Build images for ipq54xx 64 bit system.
endef

DEFAULT_PACKAGES += \
	uboot-ipq5424-mmc \
	uboot-ipq5424-norplusmmc \
	uboot-ipq5424-norplusnand \
	uboot-ipq5424-nand \
	uboot-ipq5424-debug-mmc \
	uboot-ipq5424-debug-norplusmmc \
	uboot-ipq5424-debug-norplusnand \
	uboot-ipq5424-debug-nand \
	uboot-2025-ipq5424-mmc \
	uboot-2025-ipq5424-norplusmmc \
	uboot-2025-ipq5424-norplusnand \
	uboot-2025-ipq5424-nand \
	uboot-2025-ipq5424-debug-mmc \
	uboot-2025-ipq5424-debug-norplusmmc \
	uboot-2025-ipq5424-debug-norplusnand \
	uboot-2025-ipq5424-debug-nand
