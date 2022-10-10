include $(TOPDIR)/rules.mk

PKG_NAME:=usteer2
PKG_RELEASE:=1
PKG_LICENSE:=ISC

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk

define Package/usteer2
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+ucrun
  TITLE:=wifi client steering
endef

define Build/Compile/Default

endef
Build/Compile = $(Build/Compile/Default)

define Package/usteer2/install
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,usteer2))
