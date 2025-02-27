include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk


PKG_NAME:=platform_drivers
PKG_VERSION:=1

define Build/Prepare
endef

define Build/Configure
endef

define Build/Compile
endef

OTHER_MENU:=Other modules

define KernelPackage/bootconfig
  SUBMENU:=$(OTHER_MENU)
  TITLE:=Bootconfig partition for failsafe
  DEPENDS+=@TARGET_ipq95xx||TARGET_ipq53xx
  KCONFIG:=CONFIG_BOOTCONFIG_PARTITION
  FILES:=$(LINUX_DIR)/drivers/platform/msm/bootconfig.ko@lt4.4 \
	$(LINUX_DIR)/drivers/platform/ipq/bootconfig.ko@ge4.4
  AUTOLOAD:=$(call AutoLoad,56,bootconfig,1)
endef

define KernelPackage/bootconfig/description
  Bootconfig partition for failsafe
endef

$(eval $(call KernelPackage,bootconfig))

define KernelPackage/qseecom
  SUBMENU:=$(OTHER_MENU)
  TITLE:=QSEECOM Client Test Driver
  KCONFIG:=CONFIG_QSEECOM
  FILES:=$(LINUX_DIR)/drivers/misc/qseecom.ko
  AUTOLOAD:=$(call AutoLoad,09,qseecom)
endef

define KernelPackage/qseecom/description
 QSEECOM Client Test Driver based on SCM interface
endef

$(eval $(call KernelPackage,qseecom))

define KernelPackage/fw-auth-test
  CATEGORY:=Kernel modules
  SUBMENU:=$(OTHER_MENU)
  TITLE:=Test module for Wifi FW Authentication
  KCONFIG:=CONFIG_FW_AUTH_TEST
  FILES:=$(LINUX_DIR)/drivers/misc/fw_auth_test.ko
endef

define KernelPackage/fw-auth-test/description
  This test module provides sysfs interface to provide data to test
the FW authentication API and display the results.
endef

$(eval $(call KernelPackage,fw-auth-test))

define KernelPackage/test-udelay
  CATEGORY:=Kernel modules
  SUBMENU:=$(OTHER_MENU)
  TITLE:=Test module for Udelay
  KCONFIG:=CONFIG_TEST_UDELAY
  FILES:=$(LINUX_DIR)/kernel/time/test_udelay.ko
endef

define KernelPackage/test-udelay/description
  This test module provides sysfs interface to test that udelay() is working
properly.
endef

$(eval $(call KernelPackage,test-udelay))

SOUND_MENU:=Sound Support

define AddPlatformDepends/sound
  SUBMENU:=$(SOUND_MENU)
  DEPENDS+=kmod-sound-core $(1)
endef

define KernelPackage/sound-soc-ipq
  TITLE:=Qualcomm-Atheros IPQ board soundcard support
  DEPENDS:=@TARGET_ipq806x||TARGET_ipq807x||TARGET_ipq @!TARGET_ipq_ipq60xx @!TARGET_ipq_ipq60xx_64 +kmod-sound-soc-core
  KCONFIG:= \
	CONFIG_SND_QCA_SOC \
	CONFIG_SND_QCA_SOC_IPQ40XX \
	CONFIG_SND_SOC_QCOM=y \
	CONFIG_SND_SOC_IPQ=y \
	CONFIG_SND_SOC_IPQ_CODEC=y
  FILES:= \
	$(LINUX_DIR)/sound/soc/qca/snd-soc-ipq40xx-codec.ko@lt4.4 \
	$(LINUX_DIR)/sound/soc/qcom/ipq/snd-soc-ipq-codec.ko@ge4.4
  $(call AddPlatformDepends/sound)
endef

define KernelPackage/sound-soc-ipq/description
 This enables sound card support for Qualcomm-Atheros IPQ board
endef

$(eval $(call KernelPackage,sound-soc-ipq))

VIDEO_MENU:=Video Support

define KernelPackage/qpic_panel_ili_qvga
  SUBMENU:=$(VIDEO_MENU)
  TITLE:=ILI QVGA Panel support
  KCONFIG:=CONFIG_FB_MSM_QPIC_ILI_QVGA_PANEL
  FILES:=$(LINUX_DIR)/drivers/video/msm/mdss/qpic_panel_ili_qvga.ko
endef

define KernelPackage/qpic_panel_ili_qvga/description
 Kernel support for qpic_panel_ili_qvga  lcd panel
endef

$(eval $(call KernelPackage,qpic_panel_ili_qvga))

define KernelPackage/qpic_panel_ertft
  SUBMENU:=$(VIDEO_MENU)
  TITLE:=ERT TFT  Panel support
  KCONFIG:=CONFIG_FB_MSM_QPIC_ER_SSD1963_PANEL@lt4.4 \
	CONFIG_FB_QCOM_QPIC_ER_SSD1963_PANEL@le4.4 \
	CONFIG_FB_QTI_QPIC_ER_SSD1963_PANEL@ge5.4
  FILES:=$(LINUX_DIR)/drivers/video/msm/mdss/qpic_panel_er_ssd1963.ko@lt4.4 \
	$(LINUX_DIR)/drivers/video/fbdev/qcom/qpic_panel_er_ssd1963.ko@le4.4 \
	$(LINUX_DIR)/drivers/video/fbdev/qti/qpic_panel_er_ssd1963.ko@ge5.4
endef

define KernelPackage/qpic_panel_ertft/description
 Kernel support for qpic_panel_ertft  lcd panel
endef

$(eval $(call KernelPackage,qpic_panel_ertft))

CRYPTO_MENU:=Cryptographic API modules

define AddPlatformDepends/crypto
  SUBMENU:=$(CRYPTO_MENU)
  DEPENDS+=+LINUX_4_4:kmod-crypto-core $(1)
endef

define KernelPackage/crypto-aes-internal
  TITLE:=AES cipher CryptoAPI module
  KCONFIG:=CONFIG_CRYPTO_AES CONFIG_CRYPTO_AES_586
  FILES:=$(LINUX_DIR)/crypto/aes_generic.ko
  AUTOLOAD:=$(call AutoLoad,09,aes_generic)
  $(call AddDepends/crypto)
endef

$(eval $(call KernelPackage,crypto-aes-internal))


define KernelPackage/crypto-qcrypto
  TITLE:=QTI crypto CE engine
  KCONFIG:= \
	CONFIG_CRYPTO_DEV_QCRYPTO \
	CONFIG_CRYPTO_DEV_QCEDEV
  FILES:= \
	$(LINUX_DIR)/drivers/crypto/msm/qcrypto.ko \
	$(LINUX_DIR)/drivers/crypto/msm/qcedev.ko
  AUTOLOAD:=$(call AutoLoad,09,qcrypto)
  DEPENDS:=@TARGET_ipq806x||TARGET_ipq40xx||TARGET_ipq_ipq40xx +kmod-crypto-manager +kmod-crypto-aes +kmod-crypto-hash +kmod-crypto-des
  $(call AddPlatformDepends/crypto)
endef

$(eval $(call KernelPackage,crypto-qcrypto))

define KernelPackage/crypto-qce
  TITLE:=QTI Crypto Engine (QCE)
  KCONFIG:= \
	CONFIG_CRYPTO_DEV_QCE \
	CONFIG_CRYPTO_DEV_QCE_ENABLE_ALL=y@ge5.4 \
	CONFIG_CRYPTO_DEV_QCE_SKCIPHER=y \
	CONFIG_CRYPTO_DEV_QCE_AEAD=y \
	CONFIG_CRYPTO_DEV_QCE_ENABLE_SKCIPHER=n@ge5.4 \
	CONFIG_CRYPTO_DEV_QCE_ENABLE_SHA=n@ge5.4 \
	CONFIG_CRYPTO_DEV_QCE_SW_MAX_LEN=0@ge5.4 \
	CONFIG_CRYPTO_DEV_QCE_ENABLE_AEAD=n \
	CONFIG_CRYPTO_DEV_QCE_SHA=y
  FILES:= \
	$(LINUX_DIR)/drivers/crypto/qce/qcrypto.ko
  AUTOLOAD:=$(call AutoLoad,09,qcrypto)
  DEPENDS:=@TARGET_ipq807x||TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq60xx||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64||TARGET_ipq95xx||TARGET_ipq50xx +kmod-crypto-manager +LINUX_4_4:kmod-crypto-aes-internal +kmod-crypto-hash +LINUX_4_4:kmod-crypto-des +kmod-crypto-authenc
  $(call AddPlatformDepends/crypto)
endef

$(eval $(call KernelPackage,crypto-qce))

define KernelPackage/crypto-seccrypt
  TITLE:=QTI Crypto Secure Crypt (QSC)
  KCONFIG:= \
	CONFIG_SECCRYPT
  FILES:= \
	$(LINUX_DIR)/drivers/crypto/seccrypt.ko
  AUTOLOAD:=$(call AutoLoad,09,seccrypt)
  DEPENDS:=@TARGET_ipq53xx
  $(call AddPlatformDepends/crypto)
endef

$(eval $(call KernelPackage,crypto-seccrypt))

USB_MENU:=USB Support

define AddPlatformDepends/usb
  SUBMENU:=$(USB_MENU)
  DEPENDS+=+kmod-usb-core $(1)
endef

define KernelPackage/usb-ath79
  TITLE:=ATH79 USB driver
  DEPENDS:=@TARGET_ar71xx +kmod-usb2
  KCONFIG:=CONFIG_USB_EHCI_HCD_ATH79
  FILES:=$(LINUX_DIR)/drivers/usb/host/ehci-ath79.ko
  AUTOLOAD:=$(call AutoLoad,80,ehci-ath79)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-ath79/description
 This driver provides USB Device Controller support for the
 ATH79 EHCI USB
endef

$(eval $(call KernelPackage,usb-ath79))

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

define KernelPackage/usb-chipidea-qti
  TITLE:=ChipIdea controllers support
  DEPENDS:= +kmod-usb2 +USB_GADGET_SUPPORT:kmod-usb-gadget @TARGET_ipq_ipq806x
  KCONFIG:=\
	CONFIG_USB_CHIPIDEA \
	CONFIG_USB_CHIPIDEA_HOST=y \
	CONFIG_USB_CHIPIDEA_UDC=y \
	CONFIG_USB_CHIPIDEA_DEBUG=y
	FILES:=\
	  $(LINUX_DIR)/drivers/extcon/extcon.ko@ge4.3 \
	  $(LINUX_DIR)/drivers/usb/chipidea/ci_hdrc.ko \
	  $(LINUX_DIR)/drivers/usb/chipidea/ci_hdrc_msm.ko
  AUTOLOAD :=$(call AutoLoad,51,extcon ci_hdrc ci_hdrc_msm)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-chipidea-msm/description
  Kernel support for USB ChipIdea controllers
endef

$(eval $(call KernelPackage,usb-chipidea-qti))


define KernelPackage/usb-phy-ci-qti
  TITLE:=Chipidea USB PHY driver
  DEPENDS:=+kmod-usb-chipidea-qti @TARGET_ipq_ipq806x
  KCONFIG:=CONFIG_USB_MSM_OTG
  FILES:=$(LINUX_DIR)/drivers/usb/phy/phy-msm-usb.ko
  AUTOLOAD:=$(call AutoLoad,50,phy-msm-usb)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-phy-ci-qti/description
  Kernel support for qti chipidea USB PHY
endef

$(eval $(call KernelPackage,usb-phy-ci-qti))
define KernelPackage/usb-gzero
  TITLE:=USB GZERO support
  KCONFIG:=CONFIG_USB_ZERO
  FILES:=\
       $(LINUX_DIR)/drivers/usb/gadget/g_zero.ko
  AUTOLOAD:=$(call AutoLoad,45,g_zero)
  DEPENDS:=+kmod-usb-lib-composite +kmod-usb-configfs +kmod-lib-crc-ccitt
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-gzero/description
 Kernel support for USB gzero mode
endef

$(eval $(call KernelPackage,usb-gzero))

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

define KernelPackage/usb-f_ss_lb
  TITLE:=USB F_SS_LB support
  KCONFIG:=CONFIG_USB_F_SS_LB
  FILES:=\
        $(LINUX_DIR)/drivers/usb/gadget/usb_f_ss_lb.ko
  AUTOLOAD:=$(call AutoLoad,45,usb_f_ss_lb)
  DEPENDS:=+kmod-usb-lib-composite +kmod-usb-configfs +kmod-lib-crc-ccitt
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-f_ss_lb/description
 Kernel support for USB f_ss_lb mode
endef

$(eval $(call KernelPackage,usb-f_ss_lb))

define KernelPackage/usb-phy-ipq807x
  TITLE:=DWC3 USB QTI PHY drivers for IPQ807x based targets
  DEPENDS:=@TARGET_ipq||TARGET_ipq807x||TARGET_ipq95xx||TARGET_ipq60xx||TARGET_ipq54xx
  KCONFIG:= \
	CONFIG_PHY_QCOM_QUSB2 \
	CONFIG_PHY_QCOM_QMP=y \
	CONFIG_PHY_QCOM_QMP_USB=y \
	CONFIG_USB_QCOM_QUSB_PHY \
	CONFIG_USB_QCOM_QMP_PHY
  FILES:= \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qcom-qusb2.ko@ge5.4 \
	$(LINUX_DIR)/drivers/usb/phy/phy-msm-qusb.ko@le4.4 \
	$(LINUX_DIR)/drivers/usb/phy/phy-msm-ssusb-qmp.ko@le4.4
  AUTOLOAD:=$(call AutoLoad,85,phy-qcom-qusb2 phy-msm-qusb phy-msm-ssusb-qmp)
  $(call AddPlatformDepends/usb)
endef

define KernelPackage/usb-phy-ipq807x/description
  Support for USB PHY drivers in IPQ807x based SoCs.
endef

$(eval $(call KernelPackage,usb-phy-ipq807x))

define KernelPackage/usb-dwc3-internal
  TITLE:=DWC3 USB controller driver
  DEPENDS:=+USB_GADGET_SUPPORT:kmod-usb-gadget +USB_GADGET_SUPPORT:kmod-usb-roles
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
  DEPENDS:=@!LINUX_4_14 @(TARGET_ipq40xx||TARGET_ipq806x||TARGET_ipq807x||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq50xx||TARGET_ipq53xx||TARGET_ipq54xx) +kmod-usb-dwc3-internal
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

LEDS_MENU:=LED modules

define KernelPackage/leds-tlc591xx
  SUBMENU:=$(LEDS_MENU)
  TITLE:=TLC591XX LED support
  DEPENDS:=+kmod-i2c-core +kmod-regmap
  KCONFIG:=CONFIG_LEDS_TLC591XX
  FILES:=$(LINUX_DIR)/drivers/leds/leds-tlc591xx.ko
  AUTOLOAD:=$(call AutoLoad,60,leds-tlc591xx)
endef

define KernelPackage/leds-tlc591xx/description
  Driver for the TLC591XX I2C LED controllers.
endef

$(eval $(call KernelPackage,leds-tlc591xx))

define KernelPackage/mhitest_mod
  SUBMENU:=$(OTHER_MENU)
  TITLE:=MHI Test Module for qcn9000.
  DEPENDS+=@TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq50xx||TARGET_ipq807x||TARGET_ipq60xx||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx
  KCONFIG:=CONFIG_MHI_BUS_TEST
  FILES:=$(LINUX_DIR)/drivers/bus/mhi/test/mhitest_mod.ko
endef

define KernelPackage/mhitest_mod/description
  Add MHI Test Module.
endef

$(eval $(call KernelPackage,mhitest_mod))

define KernelPackage/mhi_fuse_blower_mod
  SUBMENU:=$(OTHER_MENU)
  TITLE:=MHI Fuse Blower Module for qcn9224.
  DEPENDS+=@TARGET_ipq95xx||TARGET_ipq53xx
  KCONFIG:=
  FILES:=$(LINUX_DIR)/drivers/bus/mhi/fuse_blower/mhi_fuse_blower_mod.ko
endef

define KernelPackage/mhi_fuse_blower_mod/description
  Add MHI Fuse blower Module.
endef

$(eval $(call KernelPackage,mhi_fuse_blower_mod))

#Add in these flags for kernel specific configs
#@le4.4 - kernel version less than 4.4
#@ge5.4 - kernel version greater than 5.4
#@eq5.4 - kernel version equal to 5.4
define KernelPackage/msm-mproc
  SUBMENU:=$(OTHER_MENU)
  TITLE:= Default kernel configs
  DEPENDS+= @TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64
  KCONFIG:= \
	  CONFIG_DIAG_OVER_QRTR=y \
	  CONFIG_IPQ_SUBSYSTEM_RESTART=y \
	  CONFIG_MAILBOX=y \
	  CONFIG_MSM_RPM_RPMSG=y \
	  CONFIG_QCOM_APCS_IPC=y \
	  CONFIG_QCOM_GLINK_SSR=y \
	  CONFIG_QCOM_Q6V5_WCSS=y \
	  CONFIG_QCOM_QMI_HELPERS=y \
	  CONFIG_QCOM_SYSMON=y \
	  CONFIG_QRTR=y \
	  CONFIG_QRTR_SMD=y \
	  CONFIG_QTI_Q6V5_ADSP=y \
	  CONFIG_REGULATOR_RPM_GLINK=y \
	  CONFIG_RPMSG=y \
	  CONFIG_RPMSG_CHAR=y \
	  CONFIG_RPMSG_QCOM_GLINK_RPM=y \
	  CONFIG_RPMSG_QCOM_GLINK_SMEM=y \
	  CONFIG_RPMSG_QCOM_SMD=y \
	  CONFIG_SAMPLES=y \
	  CONFIG_SAMPLE_CONFIGFS=n \
	  CONFIG_SAMPLE_HW_BREAKPOINT=n \
	  CONFIG_SAMPLE_KFIFO=n \
	  CONFIG_SAMPLE_KOBJECT=n \
	  CONFIG_SAMPLE_KPROBES=n \
	  CONFIG_SAMPLE_KRETPROBES=n \
	  CONFIG_SAMPLE_QMI_CLIENT=m \
	  CONFIG_SAMPLE_RPMSG_CLIENT=n \
	  CONFIG_SAMPLE_TRACE_EVENTS=n
endef

define KernelPackage/msm-mproc/description
Default kernel configs.
endef

$(eval $(call KernelPackage,msm-mproc))

define KernelPackage/clk-test
  TITLE:=CLK-TEST driver
  KCONFIG:=CONFIG_CLK_TEST_5018
  DEPENDS:=@TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64
  FILES:= $(LINUX_DIR)/drivers/clk/qcom/clk_test_ipq5018.ko
endef

define KernelPackage/clk-test/description
This driver provides support for the testing of GCC clocks and resets.
endef

$(eval $(call KernelPackage,clk-test))

define KernelPackage/qmi_sample_client
  SUBMENU:=$(OTHER_MENU)
  TITLE:= Sample qmi test application.
  DEPENDS+=@TARGET_ipq807x||TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq60xx||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq50xx||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx

  KCONFIG:= \
	  CONFIG_SAMPLE_QMI_CLIENT
  FILES:= \
	  $(LINUX_DIR)/samples/qmi/qmi_sample_client.ko
  AUTOLOAD:=$(call AutoLoad,53,qmi_sample_client)
endef

define KernelPackage/qmi_sample_client/description
Add QMI ping_pong test application
endef

$(eval $(call KernelPackage,qmi_sample_client))

define KernelPackage/ipq-thermal
  TITLE:= IPQ thermal driver.
  DEPENDS+= @TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64

  KCONFIG:= \
	  CONFIG_THERMAL=y \
	  CONFIG_THERMAL_DEFAULT_GOV_STEP_WISE=y \
	  CONFIG_THERMAL_GOV_STEP_WISE=y \
	  CONFIG_THERMAL_GOV_USER_SPACE=y \
	  CONFIG_THERMAL_HWMON=y \
	  CONFIG_THERMAL_OF=y \
	  CONFIG_THERMAL_WRITABLE_TRIPS=y \
	  CONFIG_NVMEM=y
endef

define KernelPackage/ipq-thermal/description
Add IPQ Thermal driver
endef

$(eval $(call KernelPackage,ipq-thermal))

define KernelPackage/ipq-coresight
  TITLE:= IPQ CoreSight Tracing Support Driver.
  DEPENDS:= @TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64
  KCONFIG:= \
	CONFIG_CORESIGHT=y \
	CONFIG_CORESIGHT_CSR=y \
	CONFIG_CORESIGHT_CTI=y \
	CONFIG_CORESIGHT_EVENT=y \
	CONFIG_CORESIGHT_HWEVENT=y \
	CONFIG_CORESIGHT_LINKS_AND_SINKS=y \
	CONFIG_CORESIGHT_LINK_AND_SINK_TMC=y \
	CONFIG_CORESIGHT_QCOM_REPLICATOR=y \
	CONFIG_CORESIGHT_SINK_TPIU=y \
	CONFIG_CORESIGHT_SOURCE_ETM3X=y \
	CONFIG_CORESIGHT_SOURCE_ETM4X=y \
	CONFIG_CORESIGHT_STM=y \
	CONFIG_CORESIGHT_TPDA=y \
	CONFIG_CORESIGHT_TPDM=y \
	CONFIG_SPS=y \
	CONFIG_SPS_SUPPORT_NDP_BAM=y
endef

define KernelPackage/ipq-coresight/description
Add IPQ CoreSight Tracing Support
endef

$(eval $(call KernelPackage,ipq-coresight))

define KernelPackage/bt_tty
  TITLE:= BT Inter-processor Communication
  DEPENDS+= @TARGET_ipq50xx||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64
  KCONFIG:= \
	CONFIG_QTI_BT_INTERFACE \
	CONFIG_QTI_BT_PIL \
	CONFIG_QTI_BT_TTY \
	CONFIG_QCOM_MDT_LOADER=y
  FILES:= $(LINUX_DIR)/drivers/soc/qcom/bt_interface.ko@ge5.4 \
	$(LINUX_DIR)/drivers/remoteproc/bt_rproc.ko@ge5.4 \
	$(LINUX_DIR)/drivers/soc/qcom/bt/bt_rproc.ko@le4.4 \
	$(LINUX_DIR)/drivers/soc/qcom/bt/bt_interface.ko@le4.4
  AUTOLOAD:=$(call AutoLoad,53,bt_interface bt_rproc)
endef

define KernelPackage/bt_tty/description
BT Interprocessor Communication support specific to IPQ50xx
endef

$(eval $(call KernelPackage,bt_tty))

define KernelPackage/usb-phy-ipq5018
  TITLE:=DWC3 USB PHY driver for IPQ5018
  DEPENDS:=@TARGET_ipq50xx||TARGET_ipq53xx
  KCONFIG:= \
	CONFIG_USB_QCA_M31_PHY \
	CONFIG_PHY_QCOM_M31_USB \
	CONFIG_PHY_IPQ_UNIPHY_USB
  FILES:= \
	$(LINUX_DIR)/drivers/usb/phy/phy-qca-m31.ko@lt6.1 \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qcom-m31.ko@ge6.1 \
	$(LINUX_DIR)/drivers/phy/phy-qca-uniphy.ko@le4.4 \
	$(LINUX_DIR)/drivers/phy/qualcomm/phy-qca-uniphy.ko@ge5.4
  AUTOLOAD:=$(call AutoLoad,85,phy-qca-m31 phy-qcom-m31 phy-qca-uniphy)
  $(call AddDepends/usb)
endef

define KernelPackage/usb-phy-ipq5018/description
 This driver provides support for the USB PHY drivers
 within the IPQ5018 SoCs.
endef

$(eval $(call KernelPackage,usb-phy-ipq5018))

define KernelPackage/llcc_perfmon
  SUBMENU:=$(OTHER_MENU)
  TITLE:=LLCC Perfmon Driver
  DEPENDS+=@TARGET_ipq54xx
  KCONFIG:=CONFIG_QCOM_LLCC_PERFMON
  FILES:=$(LINUX_DIR)/drivers/soc/qcom/llcc_perfmon.ko
endef

define KernelPackage/llcc_perfmon/description
  LLCC Perfmonance driver support
endef

$(eval $(call KernelPackage,llcc_perfmon))
