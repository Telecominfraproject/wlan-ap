OTHER_MENU:=Other modules

define KernelPackage/switch-rtl8367c
  SUBMENU:=$(NETWORK_DEVICES_MENU)
  TITLE:=Realtek RTL8367C/S switch support
  DEPENDS:=+kmod-switch-rtl8366-smi
  KCONFIG:=CONFIG_RTL8367C_PHY=y
  FILES:=$(LINUX_DIR)/drivers/net/phy/rtl8367c.ko
  AUTOLOAD:=$(call AutoLoad,43,rtl8367c,1)
endef

define KernelPackage/switch-rtl8367c/description
 Realtek RTL8367C/S switch support
endef

$(eval $(call KernelPackage,switch-rtl8367c))

define KernelPackage/tpm-tis-core
  SUBMENU:=$(OTHER_MENU)
  TITLE:=TPM TIS 1.2 Interface / TPM 2.0 FIFO Interface
	DEPENDS:= +kmod-tpm
  KCONFIG:= CONFIG_TCG_TIS
  FILES:= \
	$(LINUX_DIR)/drivers/char/tpm/tpm_tis.ko \
	$(LINUX_DIR)/drivers/char/tpm/tpm_tis_core.ko
  AUTOLOAD:=$(call AutoLoad,20,tpm_tis,1)
endef

define KernelPackage/tpm-tis-core/description
	If you have a TPM security chip that is compliant with the
	TCG TIS 1.2 TPM specification (TPM1.2) or the TCG PTP FIFO
	specification (TPM2.0) say Yes and it will be accessible from
	within Linux.
endef

$(eval $(call KernelPackage,tpm-tis-core))


define KernelPackage/tpm-tis-i2c
  SUBMENU:=$(OTHER_MENU)
  TITLE:=TPM I2C Interface Specification
        DEPENDS:= +kmod-tpm +kmod-i2c-core +kmod-tpm-tis-core +kmod-lib-crc-ccitt
  KCONFIG:= CONFIG_TCG_TIS_I2C
  FILES:= $(LINUX_DIR)/drivers/char/tpm/tpm_tis_i2c.ko
  AUTOLOAD:=$(call AutoLoad,40,tpm_tis_i2c,1)
endef
define KernelPackage/tpm-tis-i2c/description
        If you have a TPM security chip which is connected to a regular
  I2C master (i.e. most embedded platforms) that is compliant with the
  TCG TPM I2C Interface Specification say Yes and it will be accessible from
  within Linux.
endef
$(eval $(call KernelPackage,tpm-tis-i2c))

define KernelPackage/usb-dwc3-internal
  TITLE:=DWC3 USB controller driver
  DEPENDS:=+USB_GADGET_SUPPORT:kmod-usb-gadget
  KCONFIG:= \
        CONFIG_USB_DWC3 \
        CONFIG_USB_DWC3_HOST=n \
        CONFIG_USB_DWC3_GADGET=n \
        CONFIG_USB_DWC3_DUAL_ROLE=y \
        CONFIG_EXTCON=y \
        CONFIG_USB_DWC3_DEBUG=n \
        CONFIG_USB_DWC3_VERBOSE=n
  FILES:= $(LINUX_DIR)/drivers/usb/dwc3/dwc3.ko
  AUTOLOAD:=$(call AutoLoad,84,dwc3)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-dwc3-internal/description
 This driver provides support for the Dual Role SuperSpeed
 USB Controller based on the Synopsys DesignWare USB3 IP Core
endef

$(eval $(call KernelPackage,usb-dwc3-internal))

define KernelPackage/usb-dwc3-qcom-internal
  TITLE:=DWC3 QTI USB driver
  DEPENDS:=@!LINUX_4_14 @(TARGET_ipq40xx||TARGET_ipq806x||TARGET_ipq807x||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq50xx||TARGET_ipq53xx) +kmod-usb-dwc3-internal
  KCONFIG:= CONFIG_USB_DWC3_QCOM
  FILES:= $(LINUX_DIR)/drivers/usb/dwc3/dwc3-qcom.ko
  AUTOLOAD:=$(call AutoLoad,83,dwc3-qcom)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-dwc3-qcom-internal/description
 Some QTI SoCs use DesignWare Core IP for USB2/3 functionality.
 This driver also handles Qscratch wrapper which is needed for
 peripheral mode support.
endef

$(eval $(call KernelPackage,usb-dwc3-qcom-internal))

define KernelPackage/usb-configfs
 TITLE:= USB functions
  KCONFIG:=CONFIG_USB_CONFIGFS \
	CONFIG_USB_CONFIGFS_SERIAL=n \
	CONFIG_USB_CONFIGFS_ACM=n \
	CONFIG_USB_CONFIGFS_OBEX=n \
	CONFIG_USB_CONFIGFS_NCM=n \
	CONFIG_USB_CONFIGFS_ECM=n \
	CONFIG_USB_CONFIGFS_ECM_SUBSET=n \
	CONFIG_USB_CONFIGFS_RNDIS=n \
	CONFIG_USB_CONFIGFS_EEM=n \
	CONFIG_USB_CONFIGFS_MASS_STORAGE=n \
	CONFIG_USB_CONFIGFS_F_LB_SS=n \
	CONFIG_USB_CONFIGFS_F_FS=n \
	CONFIG_USB_CONFIGFS_F_UAC1=n \
	CONFIG_USB_CONFIGFS_F_UAC2=n \
	CONFIG_USB_CONFIGFS_F_MIDI=n \
	CONFIG_USB_CONFIGFS_F_HID=n \
	CONFIG_USB_CONFIGFS_F_PRINTER=n \
	CONFIG_USB_CONFIGFS_F_QDSS=n
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-configfs/description
 USB functions
endef

$(eval $(call KernelPackage,usb-configfs))

define KernelPackage/usb-f-diag
  TITLE:=USB DIAG
  KCONFIG:=CONFIG_USB_F_DIAG \
	CONFIG_USB_CONFIGFS_F_DIAG=y \
	CONFIG_DIAG_OVER_USB=y
  DEPENDS:=+kmod-usb-lib-composite +kmod-usb-configfs
  FILES:=$(LINUX_DIR)/drivers/usb/gadget/function/usb_f_diag.ko
  AUTOLOAD:=$(call AutoLoad,52,usb_f_diag)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-f-diag/description
 USB DIAG
endef

$(eval $(call KernelPackage,usb-f-diag))

define KernelPackage/usb-uas
  TITLE:=USB Attched SCSI support
  DEPENDS:= +kmod-scsi-core +kmod-usb-storage
  KCONFIG:=CONFIG_USB_UAS
  FILES:=$(LINUX_DIR)/drivers/usb/storage/uas.ko
  AUTOLOAD:=$(call AutoProbe,uas)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-uas/description
 Kernel support for USB Attached SCSI devices (UAS)
endef

$(eval $(call KernelPackage,usb-uas))


define KernelPackage/diag-char
  TITLE:=CHAR DIAG
  KCONFIG:= CONFIG_DIAG_MHI=y@ge5.4 \
	  CONFIG_DIAG_OVER_PCIE=n@ge5.4 \
	  CONFIG_DIAGFWD_BRIDGE_CODE=y \
	  CONFIG_DIAG_CHAR
  DEPENDS:=+kmod-lib-crc-ccitt +USB_CONFIGFS_F_DIAG:kmod-usb-f-diag +USB_CONFIGFS_F_DIAG:kmod-usb-core
  FILES:=$(LINUX_DIR)/drivers/char/diag/diagchar.ko
ifneq (,$(findstring $(CONFIG_KERNEL_IPQ_MEM_PROFILE), 256)$(CONFIG_LOWMEM_FLASH))
   AUTOLOAD:=
else
   AUTOLOAD:=$(call AutoLoad,52,diagchar)
endif
endef

define KernelPackage/diag-char/description
 CHAR DIAG
endef

$(eval $(call KernelPackage,diag-char))

define KernelPackage/usb-f-qdss
  TITLE:=USB QDSS
  KCONFIG:=CONFIG_USB_F_QDSS \
	CONFIG_USB_CONFIGFS_F_QDSS=y
  DEPENDS:=@TARGET_ipq807x||TARGET_ipq60xx||TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64 +kmod-usb-lib-composite +kmod-usb-configfs +kmod-lib-crc-ccitt +kmod-usb-dwc3 +TARGET_ipq_ipq60xx:kmod-usb-dwc3-qcom +TARGET_ipq_ipq60xx_64:kmod-usb-dwc3-qcom +TARGET_ipq_ipq50xx:kmod-usb-dwc3-qcom +TARGET_ipq_ipq50xx_64:kmod-usb-dwc3-qcom +TARGET_ipq_ipq807x:kmod-usb-dwc3-of-simple +TARGET_ipq_ipq807x_64:kmod-usb-dwc3-of-simple +TARGET_ipq60xx:kmod-usb-dwc3-qcom +TARGET_ipq60xx:kmod-usb-dwc3-of-simple +TARGET_ipq807x:kmod-usb-dwc3-qcom +TARGET_ipq807x:kmod-usb-dwc3-of-simple
  FILES:=$(LINUX_DIR)/drivers/usb/gadget/function/usb_f_qdss.ko \
	$(LINUX_DIR)/drivers/usb/gadget/function/u_qdss.ko
  AUTOLOAD:=$(call AutoLoad,52,usb_f_qdss)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-f-qdss/description
 USB QDSS
endef

$(eval $(call KernelPackage,usb-f-qdss))

define KernelPackage/usb-gdiag
  TITLE:=USB GDIAG support
  KCONFIG:=CONFIG_USB_G_DIAG
  FILES:=\
       $(LINUX_DIR)/drivers/usb/gadget/legacy/g_diag.ko
  AUTOLOAD:=$(call AutoLoad,52,g_diag)
  DEPENDS:=@TARGET_ipq807x||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq50xx||TARGET_ipq53xx +kmod-usb-gadget +kmod-usb-lib-composite
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-gdiag/description
 Kernel support for USB gdiag mode
endef

$(eval $(call KernelPackage,usb-gdiag))

define KernelPackage/usb-phy-ipq5018
  TITLE:=DWC3 USB PHY driver for IPQ5018
  DEPENDS:=@TARGET_ipq50xx||TARGET_ipq53xx
  KCONFIG:= \
	CONFIG_USB_QCA_M31_PHY \
	CONFIG_PHY_IPQ_UNIPHY_USB
  FILES:= \
	$(LINUX_DIR)/drivers/usb/phy/phy-qca-m31.ko \
	$(LINUX_DIR)/drivers/phy/phy-qca-uniphy.ko@le4.4 \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qca-uniphy.ko@ge5.4
  AUTOLOAD:=$(call AutoLoad,85,phy-qca-m31 phy-qca-uniphy)
  $(call AddDepends/usb)
endef

define KernelPackage/usb-phy-ipq5018/description
 This driver provides support for the USB PHY drivers
 within the IPQ5018 SoCs.
endef

$(eval $(call KernelPackage,usb-phy-ipq5018))

define KernelPackage/bootconfig
  SUBMENU:=Other modules
  TITLE:=Bootconfig partition for failsafe
  KCONFIG:=CONFIG_BOOTCONFIG_PARTITION
  FILES:=$(LINUX_DIR)/drivers/platform/ipq/bootconfig.ko@ge4.4
  AUTOLOAD:=$(call AutoLoad,56,bootconfig,1)
endef

define KernelPackage/bootconfig/description
  Bootconfig partition for failsafe
endef

$(eval $(call KernelPackage,bootconfig))
