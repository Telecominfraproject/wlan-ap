#
# MTK-factory read and write
#

include $(TOPDIR)/rules.mk

PKG_NAME:=smp_util
PKG_VERSION:=1
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_CONFIG_DEPENDS:=

include $(INCLUDE_DIR)/package.mk

define Package/smp_util
  SECTION:=net
  CATEGORY:=Network
  TITLE:=mtk smp utility
  VERSION:=$(PKG_VERSION)-r$(PKG_RELEASE)
endef

define Package/smp_util/description
  mtk smp util to init iptables rule for flowblock
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default
endef

Build/Compile = $(Build/Compile/Default)

define Package/smp_util/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_DIR) $(1)/etc/hotplug.d/net

	$(INSTALL_BIN) ./files/flowtable.sh $(1)/sbin/
	$(INSTALL_BIN) ./files/smp-mt76.sh $(1)/sbin/
	$(INSTALL_BIN) ./files/smp.init $(1)/etc/init.d/smp
	$(INSTALL_BIN) ./files/99-smp $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/99-flowtable_update $(1)/etc/hotplug.d/net
endef

$(eval $(call BuildPackage,smp_util))
