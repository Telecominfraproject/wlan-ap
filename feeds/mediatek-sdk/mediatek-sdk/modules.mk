define KernelPackage/ata-ahci-mtk
  TITLE:=Mediatek AHCI Serial ATA support
  KCONFIG:=CONFIG_AHCI_MTK
  FILES:= \
	$(LINUX_DIR)/drivers/ata/ahci_mtk.ko \
	$(LINUX_DIR)/drivers/ata/libahci_platform.ko
  AUTOLOAD:=$(call AutoLoad,40,libahci libahci_platform ahci_mtk,1)
  $(call AddDepends/ata)
  DEPENDS+=@TARGET_mediatek_mt7622
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

define KernelPackage/sdhci-mtk
  SUBMENU:=Other modules
  TITLE:=Mediatek SDHCI driver
  DEPENDS:=@TARGET_mediatek_mt7622 +kmod-sdhci
  KCONFIG:=CONFIG_MMC_MTK 
  FILES:= \
	$(LINUX_DIR)/drivers/mmc/host/mtk-sd.ko
  AUTOLOAD:=$(call AutoProbe,mtk-sd,1)
endef

$(eval $(call KernelPackage,sdhci-mtk))

define KernelPackage/crypto-hw-mtk
  TITLE:= MediaTek's Crypto Engine module
  DEPENDS:=@TARGET_mediatek
  KCONFIG:= \
	CONFIG_CRYPTO_HW=y \
	CONFIG_CRYPTO_AES=y \
	CONFIG_CRYPTO_AEAD=y \
	CONFIG_CRYPTO_SHA1=y \
	CONFIG_CRYPTO_SHA256=y \
	CONFIG_CRYPTO_SHA512=y \
	CONFIG_CRYPTO_HMAC=y \
	CONFIG_CRYPTO_DEV_MEDIATEK
  FILES:=$(LINUX_DIR)/drivers/crypto/mediatek/mtk-crypto.ko
  AUTOLOAD:=$(call AutoLoad,90,mtk-crypto)
  $(call AddDepends/crypto)
endef

define KernelPackage/crypto-hw-mtk/description
  MediaTek's EIP97 Cryptographic Engine driver.
endef

$(eval $(call KernelPackage,crypto-hw-mtk))

define KernelPackage/sound-soc-mt79xx
  TITLE:=MT79xx SoC sound support
  KCONFIG:=\
	CONFIG_SND_SOC_MEDIATEK \
	CONFIG_SND_SOC_MT79XX \
	CONFIG_SND_SOC_MT79XX_WM8960
  FILES:= \
	$(LINUX_DIR)/sound/soc/mediatek/common/snd-soc-mtk-common.ko \
	$(LINUX_DIR)/sound/soc/mediatek/mt79xx/mt79xx-wm8960.ko \
	$(LINUX_DIR)/sound/soc/mediatek/mt79xx/snd-soc-mt79xx-afe.ko \
	$(LINUX_DIR)/sound/soc/codecs/snd-soc-wm8960.ko
  AUTOLOAD:=$(call AutoLoad,57,regmap-i2c snd-soc-wm8960 snd-soc-mtk-common snd-soc-mt79xx-afe)
  DEPENDS:=@TARGET_mediatek +kmod-regmap-i2c +kmod-sound-soc-core
  $(call AddDepends/sound,+kmod-regmap-i2c)
endef

define KernelPackage/sound-soc-mt79xx/description
 Support for MT79xx Platform sound
endef

$(eval $(call KernelPackage,sound-soc-mt79xx))

define KernelPackage/mediatek_hnat
  SUBMENU:=Network Devices
  TITLE:=Mediatek HNAT module
  DEPENDS:=@TARGET_mediatek +kmod-nf-conntrack
  KCONFIG:= \
	CONFIG_BRIDGE_NETFILTER=y \
	CONFIG_NETFILTER_FAMILY_BRIDGE=y \
	CONFIG_NET_MEDIATEK_HNAT
  FILES:= \
        $(LINUX_DIR)/drivers/net/ethernet/mediatek/mtk_hnat/mtkhnat.ko
endef

define KernelPackage/mediatek_hnat/description
  Kernel modules for MediaTek HW NAT offloading
endef

$(eval $(call KernelPackage,mediatek_hnat))

