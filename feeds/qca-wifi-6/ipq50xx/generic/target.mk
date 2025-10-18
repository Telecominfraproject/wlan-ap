
CPU_TYPE:=cortex-a53
SUBTARGET:=generic
BOARDNAME:=QTI IPQ50XX(64bit) based boards
KERNELNAME:=Image dtbs

DEFAULT_PACKAGES += \
	sysupgrade-helper kmod-usb-phy-ipq5018 kmod-usb-dwc3-qcom-internal kmod-bt_tty \
	kmod-clk-test

define Target/Description
        Build images for IPQ50xx 64 bit system.
endef
