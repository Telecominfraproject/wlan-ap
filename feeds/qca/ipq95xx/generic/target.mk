
SUBTARGET:=generic
BOARDNAME:=QTI IPQ95xx(64bit) based boards
CPU_TYPE:=cortex-a73
KERNELNAME:=Image dtbs

define Target/Description
	Build images for IPQ95xx 64 bit system.
endef

DEFAULT_PACKAGES += \
	sysupgrade-helper
