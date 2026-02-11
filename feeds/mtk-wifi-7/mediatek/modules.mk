define KernelPackage/ata-ahci-mtk
  TITLE:=Mediatek AHCI Serial ATA support
  KCONFIG:=CONFIG_AHCI_MTK
  FILES:= \
	$(LINUX_DIR)/drivers/ata/ahci_mtk.ko \
	$(LINUX_DIR)/drivers/ata/libahci_platform.ko
  AUTOLOAD:=$(call AutoLoad,40,libahci libahci_platform ahci_mtk,1)
  $(call AddDepends/ata)
  DEPENDS+=@(TARGET_mediatek_mt7622||TARGET_mediatek_mt7623)
endef

define KernelPackage/ata-ahci-mtk/description
 Mediatek AHCI Serial ATA host controllers
endef

$(eval $(call KernelPackage,ata-ahci-mtk))

define KernelPackage/btmtkuart
  SUBMENU:=Other modules
  TITLE:=MediaTek HCI UART driver
  DEPENDS:=@TARGET_mediatek_mt7622 +kmod-bluetooth +mt7622bt-firmware
  KCONFIG:=CONFIG_BT_MTKUART
  FILES:= \
	$(LINUX_DIR)/drivers/bluetooth/btmtkuart.ko
  AUTOLOAD:=$(call AutoProbe,btmtkuart)
endef

$(eval $(call KernelPackage,btmtkuart))

define KernelPackage/iio-mt6577-auxadc
  TITLE:=Mediatek AUXADC driver
  DEPENDS:=@(TARGET_mediatek_mt7622||TARGET_mediatek_filogic)
  KCONFIG:=CONFIG_MEDIATEK_MT6577_AUXADC
  FILES:= \
	$(LINUX_DIR)/drivers/iio/adc/mt6577_auxadc.ko
  AUTOLOAD:=$(call AutoProbe,mt6577_auxadc)
  $(call AddDepends/iio)
endef
$(eval $(call KernelPackage,iio-mt6577-auxadc))

define KernelPackage/mtk-2p5ge-phy
  SUBMENU:=$(NETWORK_DEVICES_MENU)
  TITLE:=Mediatek 2.5Gb Ethernet PHY support
  KCONFIG:=CONFIG_MEDIATEK_2P5GE_PHY=m
  FILES:=$(LINUX_DIR)/drivers/net/phy/mediatek/mtk-2p5ge.ko
  AUTOLOAD:=$(call AutoLoad,42,mtk-2p5ge,1)
endef

define KernelPackage/mtk-2p5ge-phy/description
 Mediatek 2.5Gb Ethernet PHY support
endef

$(eval $(call KernelPackage,mtk-2p5ge-phy))

define KernelPackage/realtek-phy
  SUBMENU:=$(NETWORK_DEVICES_MENU)
  TITLE:=Realtek PHY support
  KCONFIG:=CONFIG_REALTEK_PHY=m
  FILES:=$(LINUX_DIR)/drivers/net/phy/realtek/realtek.ko
  AUTOLOAD:=$(call AutoLoad,43,realtek,1)
endef

define KernelPackage/realtek-phy/description
 Realtek PHY support
endef

$(eval $(call KernelPackage,realtek-phy))
