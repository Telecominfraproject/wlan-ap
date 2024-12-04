
ARCH:=arm
SUBTARGET:=ipq53xx_32
BOARDNAME:=QTI IPQ53xx(32bit) based boards
CPU_TYPE:=cortex-a7

define Target/Description
	Build firmware image for IPQ53xx SoC devices.
endef

DEFAULT_PACKAGES += \
	uboot-2016-ipq5332 uboot-ipq5332-mmc32 \
	uboot-ipq5332-norplusmmc32 uboot-ipq5332-norplusnand32 \
	uboot-ipq5332-nand32 fwupgrade-tools \
	sysupgrade-helper
