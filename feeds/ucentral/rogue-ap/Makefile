include $(TOPDIR)/rules.mk

PKG_NAME:=rogue-ap
PKG_VERSION:=1.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/rogue-ap
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Radio resource management for rogue AP detection
endef

define Build/Compile
endef

define Package/rogue-ap/install
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/init.d $(1)/usr/share/rogueap
	$(CP) ./files/etc/config/rogueap $(1)/etc/config
	$(CP) ./files/etc/init.d/rogueap.init $(1)/etc/init.d/rogueap
	$(CP) ./files/usr/share/rogueap/rogueap-wifi6.uc $(1)/usr/share/rogueap/rogueap-wifi6.uc
	$(CP) ./files/usr/share/rogueap/rogueap-wifi7.uc $(1)/usr/share/rogueap/rogueap-wifi7.uc
endef

$(eval $(call BuildPackage,rogue-ap))
