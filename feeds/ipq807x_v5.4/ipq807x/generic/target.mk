
SUBTARGET:=generic
BOARDNAME:=QCA IPQ807x(64bit) based boards
CPU_TYPE:=cortex-a53
KERNELNAME:=Image dtbs

DEFAULT_PACKAGES += \
	sysupgrade-helper

define Target/Description
	Build firmware image for IPQ807x 64 bit system.
endef
