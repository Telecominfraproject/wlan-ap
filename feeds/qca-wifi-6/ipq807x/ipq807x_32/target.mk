
ARCH:=arm
SUBTARGET:=ipq807x_32
BOARDNAME:=QCA IPQ807x(32bit) based boards
CPU_TYPE:=cortex-a7

DEFAULT_PACKAGES += \
	uboot-2016-ipq807x uboot-2016-ipq807x_tiny lk-ipq807x \
	fwupgrade-tools

define Target/Description
	Build firmware image for IPQ807x SoC devices.
endef
