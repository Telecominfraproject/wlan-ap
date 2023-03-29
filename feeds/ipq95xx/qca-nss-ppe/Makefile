include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ppe
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/nss-ppe
PKG_MIRROR_HASH:=f3355f9e5b6635954806eb9653751ebf40a04cf7b8113c8abca17b075de30619
PKG_VERSION:=15a863594d33ad56044d7f11905bcbb41e3eb948

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

define KernelPackage/qca-nss-ppe
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for PPE (core driver)
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx \
		+kmod-qca-ssdk-nohnat +kmod-nat46
  FILES:=$(PKG_BUILD_DIR)/drv/ppe_drv/qca-nss-ppe.ko
  AUTOLOAD:=$(call AutoLoad,31,qca-nss-ppe)
endef

define KernelPackage/qca-nss-ppe/Description
Kernel module for PPE core driver
endef

define KernelPackage/qca-nss-ppe-bridge-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS PPE bridge manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq95xx_32||TARGET_ipq53xx \
		+kmod-qca-nss-ppe \
		+kmod-qca-nss-ppe-vlan-mgr \
		+PACKAGE_kmod-bonding:kmod-bonding \
		+PACKAGE_kmod-qca-ovsmgr:kmod-qca-ovsmgr
  FILES:=$(PKG_BUILD_DIR)/clients/bridge/qca-nss-ppe-bridge-mgr.ko
  AUTOLOAD:=$(call AutoLoad,51,qca-nss-ppe-bridge-mgr)
endef

define KernelPackage/qca-nss-ppe-bridge-mgr/Description
Kernel modules for NSS PPE bridge manager
endef

define KernelPackage/qca-nss-ppe-vlan-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS PPE vlan manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq95xx_32||TARGET_ipq53xx \
		+kmod-qca-nss-ppe \
		+PACKAGE_kmod-bonding:kmod-bonding
  FILES:=$(PKG_BUILD_DIR)/clients/vlan/qca-nss-ppe-vlan.ko
  AUTOLOAD:=$(call AutoLoad,51,qca-nss-ppe-vlan)
endef

define KernelPackage/qca-nss-ppe-vlan-mgr/Description
Kernel modules for NSS PPE vlan manager
endef

define KernelPackage/qca-nss-ppe-pppoe-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS PPE PPPoE manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq95xx_32||TARGET_ipq53xx \
		+kmod-qca-nss-ppe \
		+kmod-pppoe @!LINUX_3_18 \
		+PACKAGE_kmod-bonding:kmod-bonding
  FILES:=$(PKG_BUILD_DIR)/clients/pppoe/qca-nss-ppe-pppoe-mgr.ko
  AUTOLOAD:=$(call AutoLoad,51,qca-nss-ppe-pppoe-mgr)
endef

define KernelPackage/qca-nss-ppe-pppoe-mgr/Description
Kernel modules for NSS PPE PPPoE manager
endef

define KernelPackage/qca-nss-ppe-lag-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS LAG manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq95xx_32||TARGET_ipq53xx \
		+kmod-qca-nss-ppe \
		+kmod-qca-nss-ppe-vlan-mgr \
		+kmod-bonding
  FILES:=$(PKG_BUILD_DIR)/clients/lag/qca-nss-ppe-lag.ko
  AUTOLOAD:=$(call AutoLoad,51,qca-nss-ppe-lag)
endef

define KernelPackage/qca-nss-ppe-lag-mgr/Description
Kernel modules for NSS PPE LAG manager
endef

define Build/InstallDev/qca-nss-ppe
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-ppe
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-ppe/
	$(CP) $(PKG_BUILD_DIR)/drv/exports/* $(1)/usr/include/qca-nss-ppe/
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-ppe,$(1))
endef

EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init \
	-I$(STAGING_DIR)/usr/include/nat46 \

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-bridge-mgr),)
MAKE_OPTS+=bridge-mgr=y
# enable OVS bridge if ovsmgr is enabled
ifneq ($(CONFIG_PACKAGE_kmod-qca-ovsmgr),)
MAKE_OPTS+=NSS_PPE_BRIDGE_MGR_OVS_ENABLE=y
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-ovsmgr
endif
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-vlan-mgr),)
MAKE_OPTS+=vlan-mgr=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-pppoe-mgr),)
MAKE_OPTS+=pppoe-mgr=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-lag-mgr),)
MAKE_OPTS+=lag-mgr=y
endif

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" $(strip $(MAKE_OPTS)) \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		BUILD_MODPOST_WARN=1 \
		modules
endef

$(eval $(call KernelPackage,qca-nss-ppe))
$(eval $(call KernelPackage,qca-nss-ppe-bridge-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-vlan-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-pppoe-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-lag-mgr))
