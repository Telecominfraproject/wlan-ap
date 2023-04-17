include $(TOPDIR)/rules.mk

PKG_NAME:=phy-check
PKG_RELEASE:=1

PKG_LICENSE:=BSD-3-Clause
PKG_MAINTAINER:=Dino Chang <dino_chang@accton.com>

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=edgecore
	CATEGORY:=Edge-corE Proprietary
	TITLE:=Edge-corE phy check
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef
Build/Compile = $(Build/Compile/Default)

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/phy-check.sh $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/phy-check.init $(1)/etc/init.d/phy-check
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
