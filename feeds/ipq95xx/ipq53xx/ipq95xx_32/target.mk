
ARCH:=arm
SUBTARGET:=ipq95xx_32
BOARDNAME:=QTI IPQ95xx(32bit) based boards
CPU_TYPE:=cortex-a7

DEFAULT_PACKAGES += \
	uboot-2016-ipq9574 -uboot-2016-ipq95xx_tiny -lk-ipq95xx \
	fwupgrade-tools

define Target/Description
	Build firmware image for IPQ95xx SoC devices.
endef
