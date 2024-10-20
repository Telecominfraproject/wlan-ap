SUBTARGET:=generic
BOARDNAME:=QTI IPQ53xx(64bit) based boards
CPU_TYPE:=cortex-a53

define Target/Description
	Build images for ipq53xx 64 bit system.
endef

DEFAULT_PACKAGES += \
	uboot-ipq5332-mmc uboot-ipq5332-norplusmmc \
	uboot-ipq5332-norplusnand uboot-ipq5332-nand \
	sysupgrade-helper
