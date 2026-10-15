include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=altitude-tool
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

define Package/altitude-tool
  SECTION:=MTK Properties
  CATEGORY:=MTK Properties
  TITLE:=Altitude Tool
  SUBMENU:=Applications
  DEPENDS:= \
	+python3 \
  	+python3-requests
endef

define Package/altitude-tool/description
  Altitude-tool
endef

define Build/Compile
endef

define Package/altitude-tool/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/altitude-tool $(1)/usr/bin/
endef

$(eval $(call BuildPackage,altitude-tool))
