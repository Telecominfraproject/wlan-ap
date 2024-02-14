
SUBTARGET:=generic
BOARDNAME:=QTI IPQ53xx(64bit) based boards
CPU_TYPE:=cortex-a53
KERNELNAME:=Image dtbs

DEFAULT_PACKAGES += \
	sysupgrade-helper

define Target/Description
	Build images for IPQ53xx 64 bit system.
endef
