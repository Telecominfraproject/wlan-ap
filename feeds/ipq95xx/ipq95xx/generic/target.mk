
SUBTARGET:=generic
BOARDNAME:=QTI IPQ95xx(64bit) based boards
CPU_TYPE:=cortex-a73
KERNELNAME:=Image dtbs

DEFAULT_PACKAGES += \
	sysupgrade-helper

define Target/Description
	Build images for IPQ95xx 64 bit system.
endef
