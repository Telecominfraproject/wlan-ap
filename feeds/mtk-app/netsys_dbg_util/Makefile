#
# MTK NETSYS debug script
#

include $(TOPDIR)/rules.mk

PKG_NAME:=netsys_dbg_util
PKG_VERSION:=1
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_CONFIG_DEPENDS:=

include $(INCLUDE_DIR)/package.mk

define Package/netsys_dbg_util
  SECTION:=MTK Properties
  CATEGORY:=MTK Properties
  SUBMENU:=Misc
  TITLE:=mtk netsys debug utility
  VERSION:=$(PKG_VERSION)-r$(PKG_RELEASE)
endef

define Package/netsys_dbg_util/description
  mtk netsys debug utility for dumping all of netsys registers
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default
endef

Build/Compile = $(Build/Compile/Default)

define Package/netsys_dbg_util/install
	$(INSTALL_DIR) $(1)/sbin

	$(INSTALL_BIN) ./files/memdump.sh $(1)/sbin/
	$(INSTALL_BIN) ./files/netsys_dbg.sh $(1)/sbin/
endef

$(eval $(call BuildPackage,netsys_dbg_util))
