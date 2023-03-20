OTHER_MENU:=Other modules

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


define KernelPackage/tpm-tis-spi
  SUBMENU:=$(OTHER_MENU)
  TITLE:=TPM SPI Interface Specification
        DEPENDS:= +kmod-tpm +kmod-tpm-tis-core +kmod-lib-crc-ccitt
  KCONFIG:= CONFIG_TCG_TIS_SPI
  FILES:= $(LINUX_DIR)/drivers/char/tpm/tpm_tis_spi.ko
  AUTOLOAD:=$(call AutoLoad,40,tpm_tis_spi,1)
endef
define KernelPackage/tpm-tis-spi/description
        If you have a TPM security chip which is connected to a regular
  I2C master (i.e. most embedded platforms) that is compliant with the
  TCG TPM SPI Interface Specification say Yes and it will be accessible from
  within Linux.
endef
$(eval $(call KernelPackage,tpm-tis-spi))

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
