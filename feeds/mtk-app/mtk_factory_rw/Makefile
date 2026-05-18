#
# MTK-factory read and write
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mtk_factory_rw
PKG_VERSION:=1
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_CONFIG_DEPENDS:=

include $(INCLUDE_DIR)/package.mk

define Package/mtk_factory_rw
  SECTION:=MTK Properties
  CATEGORY:=MTK Properties
  SUBMENU:=Misc
  TITLE:=mtk factory read and write
  VERSION:=$(PKG_VERSION)-r$(PKG_RELEASE)
endef

define Package/mtk_factory_rw/description
  mtk factory's data read and write
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default
endef

Build/Compile = $(Build/Compile/Default)

define Package/mtk_factory_rw/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) ./files/mtk_factory_rw.sh $(1)/sbin/mtk_factory_rw.sh
endef

$(eval $(call BuildPackage,mtk_factory_rw))

