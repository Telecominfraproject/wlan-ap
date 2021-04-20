include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-wifi
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/blogic/ucentral-wifi.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-04-13
PKG_SOURCE_VERSION:=a601cf055673eb27a1e8dd499528e3e1921e0306

PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/ucentral-wifi
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=OpenWrt wifi status daemon
  DEPENDS:=+libubox +libubus +libnl-tiny
endef

TARGET_CFLAGS += \
	-I$(STAGING_DIR)/usr/include \
	-I$(STAGING_DIR)/usr/include/libnl-tiny

define Package/ucentral-wifi/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ucentral-wifi $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ucentral-wifi $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,ucentral-wifi))
