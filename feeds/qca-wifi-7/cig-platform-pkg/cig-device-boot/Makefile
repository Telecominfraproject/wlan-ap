include $(TOPDIR)/rules.mk

PKG_NAME:=cig-device-boot
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/cig-device-boot
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+kmod-usb-serial-xr
  TITLE:=CIG device init
endef

define Package/cig-device-boot/description
  Initialize particular functions of the CIG device
endef

define Build/Compile

endef

define Package/cig-device-boot/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/cig-device.init $(1)/etc/init.d/cig-device-boot
endef

$(eval $(call BuildPackage,cig-device-boot))
