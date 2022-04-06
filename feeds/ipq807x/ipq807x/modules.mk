define KernelPackage/usb-phy-ipq807x
  TITLE:=DWC3 USB QCOM PHY driver for IPQ807x
  DEPENDS:=@TARGET_ipq807x
  KCONFIG:= \
	CONFIG_USB_QCOM_QUSB_PHY \
	CONFIG_USB_QCOM_QMP_PHY
  FILES:= \
	$(LINUX_DIR)/drivers/usb/phy/phy-msm-qusb.ko \
	$(LINUX_DIR)/drivers/usb/phy/phy-msm-ssusb-qmp.ko
  AUTOLOAD:=$(call AutoLoad,45,phy-msm-qusb phy-msm-ssusb-qmp,1)
  $(call AddDepends/usb)
endef

define KernelPackage/usb-phy-ipq807x/description
 This driver provides support for the USB PHY drivers
 within the IPQ807x SoCs.
endef

$(eval $(call KernelPackage,usb-phy-ipq807x))


define KernelPackage/qrtr_mproc
  TITLE:= Ath11k Specific kernel configs for IPQ807x and IPQ60xx
  DEPENDS+= @TARGET_ipq807x
  KCONFIG:= \
          CONFIG_QRTR=y \
          CONFIG_QRTR_MHI=y \
          CONFIG_MHI_BUS=y \
          CONFIG_MHI_QTI=y \
          CONFIG_QCOM_APCS_IPC=y \
          CONFIG_QCOM_GLINK_SSR=y \
          CONFIG_QCOM_Q6V5_WCSS=y \
          CONFIG_MSM_RPM_RPMSG=y \
          CONFIG_RPMSG_QCOM_GLINK_RPM=y \
          CONFIG_REGULATOR_RPM_GLINK=y \
          CONFIG_QCOM_SYSMON=y \
          CONFIG_RPMSG=y \
          CONFIG_RPMSG_CHAR=y \
          CONFIG_RPMSG_QCOM_GLINK_SMEM=y \
          CONFIG_RPMSG_QCOM_SMD=y \
          CONFIG_QRTR_SMD=y \
          CONFIG_QCOM_QMI_HELPERS=y \
          CONFIG_SAMPLES=y \
          CONFIG_SAMPLE_QMI_CLIENT=m \
          CONFIG_SAMPLE_TRACE_EVENTS=n \
          CONFIG_SAMPLE_KOBJECT=n \
          CONFIG_SAMPLE_KPROBES=n \
          CONFIG_SAMPLE_KRETPROBES=n \
          CONFIG_SAMPLE_HW_BREAKPOINT=n \
          CONFIG_SAMPLE_KFIFO=n \
          CONFIG_SAMPLE_CONFIGFS=n \
          CONFIG_SAMPLE_RPMSG_CLIENT=n \
          CONFIG_MAILBOX=y \
          CONFIG_DIAG_OVER_QRTR=y
endef

define KernelPackage/qrtr_mproc/description
Kernel configs for ath11k support specific to ipq807x and IPQ60xx
endef

$(eval $(call KernelPackage,qrtr_mproc))

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
        $(LINUX_DIR)/drivers/phy/phy-qca-uniphy.ko
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

define KernelPackage/usb-dwc3-qcom
  TITLE:=DWC3 Qualcomm USB driver
  DEPENDS:=@(!LINUX_4_14) @TARGET_ipq807x +kmod-usb-dwc3
  KCONFIG:= CONFIG_USB_DWC3_QCOM
  FILES:= $(LINUX_DIR)/drivers/usb/dwc3/dwc3-qcom.ko \
        $(LINUX_DIR)/drivers/usb/dwc3/dbm.ko
  AUTOLOAD:=$(call AutoLoad,53,dwc3-qcom dbm,1)
  $(call AddDepends/usb)
endef

define KernelPackage/usb-dwc3-qcom/description
 Some Qualcomm SoCs use DesignWare Core IP for USB2/3 functionality.
 This driver also handles Qscratch wrapper which is needed for
 peripheral mode support.
endef


$(eval $(call KernelPackage,usb-dwc3-qcom))
