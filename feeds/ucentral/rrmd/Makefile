include $(TOPDIR)/rules.mk

PKG_NAME:=rrmd
PKG_RELEASE:=1
PKG_LICENSE:=ISC

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk

define Package/rrmd
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+ucrun
  TITLE:=radio resource management
endef

define Build/Compile/Default

endef
Build/Compile = $(Build/Compile/Default)

define Package/rrmd/install
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,rrmd))
