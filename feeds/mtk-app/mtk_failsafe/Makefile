#
# MTK-factory read and write
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mtk_failsafe
PKG_VERSION:=1
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_CONFIG_DEPENDS:=

include $(INCLUDE_DIR)/package.mk

define Package/mtk_failsafe
  SECTION:=MTK Properties
  CATEGORY:=MTK Properties
  SUBMENU:=Misc
  TITLE:=mtk failsafe script
  VERSION:=$(PKG_RELEASE)-$(REVISION)
endef

define Package/mtk_failsafe/description
  mtk init script for failsafe mode
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default
endef

Build/Compile = $(Build/Compile/Default)

define Package/mtk_failsafe/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) ./files/mtk_failsafe.sh $(1)/sbin/mtk_failsafe.sh
endef

$(eval $(call BuildPackage,mtk_failsafe))

