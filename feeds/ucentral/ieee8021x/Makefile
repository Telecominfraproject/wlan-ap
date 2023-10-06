include $(TOPDIR)/rules.mk

PKG_NAME:=ieee8021x
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=John Crispin <john@phrozen.org>
PKG_SOURCE_DATE:=2023-10-05

include $(INCLUDE_DIR)/package.mk

define Package/ieee8021x
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Wired 802.1x
  DEPENDS:=+libubox +libubus +libuci
endef

define Build/Compile

endef

define Package/ieee8021x/install
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,ieee8021x))
