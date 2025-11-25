include $(TOPDIR)/rules.mk

PKG_NAME:=cloud_discovery
PKG_RELEASE:=1

PKG_LICENSE:=BSD-3-Clause
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk

define Package/cloud_discovery
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=TIP cloud_discovery
  DEPENDS:=+certificates +bind-dig
endef

Build/Compile=

define Package/cloud_discovery/install
	$(CP) ./files/* $(1)
endef

$(eval $(call BuildPackage,cloud_discovery))
