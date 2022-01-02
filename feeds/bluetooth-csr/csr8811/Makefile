include $(TOPDIR)/rules.mk

PKG_NAME:=csr8811
PKG_RELEASE:=1

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk

define Package/csr8811
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=TIP CSR8811 support package
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef
Build/Compile = $(Build/Compile/Default)

define Package/csr8811/install
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,csr8811))
