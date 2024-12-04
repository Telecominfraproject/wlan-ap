include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ppe-clients
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=ginger_open
PKG_RELEASE:=2
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/nss-ppe
PKG_MIRROR_HASH:=ca9862331fee79cd8fda2d0f5defbb9d3e14b9b2b57d8d09016eb46510b3e9c2
PKG_VERSION:=62ec9067ccb476e22df4a9d98d3285c8161a5a7f

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

define KernelPackage/qca-nss-ppe-bridge-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS PPE bridge manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx \
		+kmod-qca-nss-ppe \
		+kmod-qca-nss-ppe-vlan-mgr \
		+PACKAGE_kmod-bonding:kmod-bonding \
		+PACKAGE_kmod-qca-ovsmgr:kmod-qca-ovsmgr
  FILES:=$(PKG_BUILD_DIR)/clients/bridge/qca-nss-ppe-bridge-mgr.ko
  AUTOLOAD:=$(call AutoLoad,52,qca-nss-ppe-bridge-mgr)
endef

define KernelPackage/qca-nss-ppe-bridge-mgr/Description
Kernel modules for NSS PPE bridge manager
endef

define KernelPackage/qca-nss-ppe-vlan-mgr
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for NSS PPE vlan manager
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx \
		+kmod-qca-nss-ppe \
		+kmod-qca-nss-ppe-vp \
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
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx \
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
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx \
		+kmod-qca-nss-ppe \
		+kmod-qca-nss-ppe-vlan-mgr \
		+kmod-bonding
  FILES:=$(PKG_BUILD_DIR)/clients/lag/qca-nss-ppe-lag.ko
  AUTOLOAD:=$(call AutoLoad,52,qca-nss-ppe-lag)
endef

define KernelPackage/qca-nss-ppe-lag-mgr/Description
Kernel modules for NSS PPE LAG manager
endef

EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init \

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-bridge-mgr),)
MAKE_OPTS+=bridge-mgr=y
# enable OVS bridge if ovsmgr is enabled
ifneq ($(CONFIG_PACKAGE_kmod-qca-ovsmgr),)
MAKE_OPTS+=NSS_PPE_BRIDGE_MGR_OVS_ENABLE=y
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-ovsmgr
endif
# disable FDB learning for open profile
ifeq ($(CONFIG_TARGET_ipq95xx_generic_QSDK_Open)$(CONFIG_TARGET_ipq95xx_ipq95xx_32_QSDK_Open)$(CONFIG_TARGET_ipq53xx_generic_QSDK_Open)$(CONFIG_TARGET_ipq53xx_ipq53xx_32_QSDK_Open), y)
EXTRA_CFLAGS+=-DNSS_PPE_BRIDGE_MGR_FDB_DISABLE
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

define KernelPackage/qca-nss-ppe-bridge-mgr/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/qca-nss-ppe-bridge-mgr.init $(1)/etc/init.d/qca-nss-ppe-bridge-mgr
endef

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-ppe-bridge-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-vlan-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-pppoe-mgr))
$(eval $(call KernelPackage,qca-nss-ppe-lag-mgr))
