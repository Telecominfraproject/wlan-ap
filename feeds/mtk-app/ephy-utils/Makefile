include $(TOPDIR)/rules.mk

PKG_NAME:=ephy-utils
PKG_VERSION:=1.0.0

PKG_CONFIG_DEPENDS:=

include $(INCLUDE_DIR)/package.mk

define Package/ephy-utils
  SECTION:=MTK Properties
  CATEGORY:=MTK Properties
  SUBMENU:=Misc
  TITLE:=Ethernet Phy Utilities
  DEPENDS:=+mii_mgr +switch
endef

define Package/ephy-utils/description
  MediaTek Rebb ethernet phy utilities
endef

define Build/Compile/Default
endef

Build/Compile = $(Build/Compile/Default)

define Package/ephy-utils/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/usr/sbin/ephy-utils

	cp ./files/ephy-utils.init $(1)/etc/init.d/ephy-utils
	cp -r ./files/*.sh $(1)/usr/sbin/ephy-utils/
	cp ./files/mtk_ephy $(1)/usr/sbin/mtk_ephy
endef

$(eval $(call BuildPackage,ephy-utils))
