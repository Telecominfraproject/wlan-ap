define KernelPackage/usb-phy-ipq807x
  TITLE:=DWC3 USB QCOM PHY driver for IPQ807x
  DEPENDS:=@TARGET_ipq807x
  KCONFIG:= \
	CONFIG_PHY_QCOM_QUSB2 \
	CONFIG_PHY_QCOM_QMP=y \
	CONFIG_USB_QCOM_QUSB_PHY \
	CONFIG_USB_QCOM_QMP_PHY
  FILES:= \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qcom-qusb2.ko@ge5.4 \
	$(LINUX_DIR)/drivers/usb/phy/phy-msm-qusb.ko@le4.4 \
	$(LINUX_DIR)/drivers/usb/phy/phy-msm-ssusb-qmp.ko@le4.4
  AUTOLOAD:=$(call AutoLoad,45,phy-qcom-qusb2 phy-msm-qusb phy-msm-ssusb-qmp,1)
  $(call AddDepends/usb)
endef

define KernelPackage/usb-phy-ipq807x/description
 This driver provides support for the USB PHY drivers
 within the IPQ807x SoCs.
endef

$(eval $(call KernelPackage,usb-phy-ipq807x))


define KernelPackage/usb-dwc3-internal
  TITLE:=DWC3 USB controller driver
  DEPENDS:=+USB_GADGET_SUPPORT:kmod-usb-gadget +kmod-usb-core
  KCONFIG:= \
        CONFIG_USB_DWC3 \
        CONFIG_USB_DWC3_HOST=n \
        CONFIG_USB_DWC3_GADGET=n \
        CONFIG_USB_DWC3_DUAL_ROLE=y \
        CONFIG_EXTCON=y \
        CONFIG_USB_DWC3_DEBUG=n \
        CONFIG_USB_DWC3_VERBOSE=n
  FILES:= $(LINUX_DIR)/drivers/usb/dwc3/dwc3.ko
  AUTOLOAD:=$(call AutoLoad,54,dwc3,1)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-dwc3-internal/description
 This driver provides support for the Dual Role SuperSpeed
 USB Controller based on the Synopsys DesignWare USB3 IP Core
endef

$(eval $(call KernelPackage,usb-dwc3-internal))

define KernelPackage/usb-dwc3-qcom-internal
  TITLE:=DWC3 QTI USB driver
  DEPENDS:=@!LINUX_4_14 @(TARGET_ipq807x||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq50xx) +kmod-usb-dwc3-internal
  KCONFIG:= CONFIG_USB_DWC3_QCOM
  FILES:= $(LINUX_DIR)/drivers/usb/dwc3/dwc3-qcom.ko
  AUTOLOAD:=$(call AutoLoad,53,dwc3-qcom,1)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-dwc3-qcom-internal/description
 Some QTI SoCs use DesignWare Core IP for USB2/3 functionality.
 This driver also handles Qscratch wrapper which is needed for
 peripheral mode support.
endef

$(eval $(call KernelPackage,usb-dwc3-qcom-internal))

define KernelPackage/bt_tty
  TITLE:= BT Inter-processor Communication
  DEPENDS+= @TARGET_ipq807x
  KCONFIG:= \
          CONFIG_QTI_BT_TTY=y \
          CONFIG_QCOM_MDT_LOADER=y
  FILES:= $(LINUX_DIR)/drivers/soc/qcom/bt/bt_rproc.ko
  AUTOLOAD:=$(call AutoLoad,53,bt_rproc,1)
endef

define KernelPackage/bt_tty/description
BT Interprocessor Communication support specific to IPQ50xx
endef

$(eval $(call KernelPackage,bt_tty))

define KernelPackage/usb-phy-ipq5018
  TITLE:=DWC3 USB PHY driver for IPQ5018
  DEPENDS:=@TARGET_ipq807x_ipq50xx
  KCONFIG:= \
        CONFIG_USB_QCA_M31_PHY \
        CONFIG_PHY_IPQ_UNIPHY_USB
  FILES:= \
	$(LINUX_DIR)/drivers/usb/phy/phy-qca-m31.ko \
        $(LINUX_DIR)/drivers/phy/qualcomm/phy-qca-uniphy.ko
  AUTOLOAD:=$(call AutoLoad,45,phy-qca-m31 phy-qca-uniphy,1)
  $(call AddDepends/usb)
endef

define KernelPackage/usb-phy-ipq5018/description
 This driver provides support for the USB PHY drivers
 within the IPQ5018 SoCs.
endef

$(eval $(call KernelPackage,usb-phy-ipq5018))

define KernelPackage/diag-char
  TITLE:=CHAR DIAG
  KCONFIG:= CONFIG_DIAG_MHI=y@ge5.4 \
          CONFIG_DIAG_OVER_PCIE=n@ge5.4 \
          CONFIG_DIAGFWD_BRIDGE_CODE=y \
          CONFIG_DIAG_CHAR=m
  DEPENDS:=+kmod-lib-crc-ccitt
  FILES:=$(LINUX_DIR)/drivers/char/diag/diagchar.ko
endef

define KernelPackage/diag-char/description
 CHAR DIAG
endef

$(eval $(call KernelPackage,diag-char))
