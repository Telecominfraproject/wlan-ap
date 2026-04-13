include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-phy
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/qca-nss-phy
PKG_MIRROR_HASH:=74d48d057e0422cf69dce25d03cc9263f578c43a3be648e571aef46a72c4653a
PKG_SOURCE_VERSION:=ad5f76af81697d6d2f0864b8c72a6f38da5079a9
PKG_VERSION:=1.0

ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-nss-phy
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA-NSS-PHY driver
  FILES:=$(PKG_BUILD_DIR)/nss_ext/qca-nss-phy.ko
  PROVIDES:=qca-nss-phy
  AUTOLOAD:=$(call AutoLoad,08,qca-nss-phy)
endef

define KernelPackage/qca-mdio
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA MDIO driver
  KCONFIG:= \
	CONFIG_MDIO_BITBANG \
	CONFIG_MDIO_GPIO \
	CONFIG_MDIO_IPQ4019
  FILES:= \
	$(LINUX_DIR)/drivers/net/mdio/mdio-gpio.ko \
	$(LINUX_DIR)/drivers/net/mdio/mdio-bitbang.ko \
	$(LINUX_DIR)/drivers/net/mdio/mdio-ipq4019.ko
  AUTOLOAD:=$(call AutoLoad,01,mdio-ipq4019 mdio-bitbang mdio-gpio)
endef

define KernelPackage/qca-mdio/description
 QCOM MDIO driver and MDIO GPIO driver
endef

define KernelPackage/qca-nss-phy/Description
This package contains drivers for QCA PHYs such as Manhattan PHY and NAPA PHY.
It also contains vendor specific extensions for QCA PHYs which are not supported
by upstream drivers.
endef

define KernelPackage/qca81xx
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA81XX PHY driver
  FILES:=$(PKG_BUILD_DIR)/linux_std/qca81xx/qca81xx-phy.ko
  DEPENDS:=+kmod-hwmon-core
  PROVIDES:=qca81xx
  AUTOLOAD:=$(call AutoLoad,07,qca81xx-phy)
endef

define KernelPackage/qca81xx/Description
This package contains qca81xx linux phy driver, it will be upstream PHY driver
of LAGUNA PHY.
endef

define KernelPackage/qca8084
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA8084 PHY driver
  FILES:=$(PKG_BUILD_DIR)/linux_std/qca8084/qca8084-phy.ko
  DEPENDS:=+kmod-qca8k-cc
  PROVIDES:=qca8084
  AUTOLOAD:=$(call AutoLoad,07,qca8084-phy)
endef

define KernelPackage/qca8084/Description
This package contains qca8084 Linux PHY driver
endef

define KernelPackage/qca8xxx-phc
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA8xxx PHC driver
  DEPENDS:=+kmod-qca81xx
  FILES:=$(PKG_BUILD_DIR)/linux_std/ptp/qca8xxx-phc.ko
  AUTOLOAD:=$(call AutoLoad, 31, qca8xxx-phc)
endef

define KernelPackage/qca8xxx-phc/Description
Linux PHC driver of QCA8XXX PHY
endef

define KernelPackage/qca8k-cc
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=QCA8K clock controller driver
  FILES:=$(PKG_BUILD_DIR)/linux_std/clock/qca8k-cc.ko
  AUTOLOAD:=$(call AutoLoad, 06, qca8k-cc)
endef

define KernelPackage/qca8k-cc/Description
Clock driver for QCA8386 and QCA8084 devices.
endef

ifdef CONFIG_TARGET_NAME
QCA_NSS_PHY_CONFIG_OPTS+= \
	TARGET_NAME=$(CONFIG_TARGET_NAME)
endif

QCA_NSS_PHY_CONFIG_OPTS+= \
	LINUX_DIR=$(LINUX_DIR) \
	CROSS_COMPILE=$(TARGET_CROSS) \
	ARCH=$(LINUX_KARCH)

ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
ifneq ($(CONFIG_LOWMEM_FLASH),y)
QCA_NSS_PHY_CONFIG_OPTS+= nss-phy-ptp=y
endif
endif

SUPPORTED_PHYS = CONFIG_NSSPHY_QCA808X CONFIG_NSSPHY_QCA81XX CONFIG_NSSPHY_QCA803X

ifneq (, $(findstring $(CONFIG_TARGET_BOARD), "ipq95xx" "ipq52xx" "ipq96xx"))
    SUPPORTED_PHYS+=CONFIG_NSSPHY_QCA807X
endif

QCA_NSS_PHY_CONFIG_OPTS+= supported_phys="$(SUPPORTED_PHYS)"

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" \
		$(strip $(QCA_NSS_PHY_CONFIG_OPTS)) \
		modules
endef

define Build/InstallDev
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/qca-nss-phy
	$(CP) -rf $(PKG_BUILD_DIR)/nss_ext/*h $(STAGING_DIR)/usr/include/qca-nss-phy
	$(CP) -rf $(PKG_BUILD_DIR)/linux_std/clock/*h $(STAGING_DIR)/usr/include/qca-nss-phy
	$(CP) -rf $(PKG_BUILD_DIR)/linux_std/*h $(STAGING_DIR)/usr/include/qca-nss-phy
endef

define KernelPackage/qca-nss-phy/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/qca-nss-phy.init $(1)/etc/init.d/qca-nss-phy
endef

$(eval $(call KernelPackage,qca-mdio))
$(eval $(call KernelPackage,qca8k-cc))
$(eval $(call KernelPackage,qca8084))
$(eval $(call KernelPackage,qca81xx))
$(eval $(call KernelPackage,qca8xxx-phc))
$(eval $(call KernelPackage,qca-nss-phy))
