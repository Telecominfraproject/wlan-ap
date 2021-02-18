include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-jsonschema
PKG_RELEASE:=1

PKG_SOURCE_URL=https://github.com/blogic/ucentral-jsonschema.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2022-11-19
PKG_SOURCE_VERSION:=98765ffecc5f6ec82ca7023a8ab6f036b359fe38

PKG_LICENSE:=BSD-3-Clause
PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/ucentral-jsonschema
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=JSON Schema Validator
  DEPENDS:=+libjson-c
endef

define Package/ucentral-jsonschema/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/jsonschema $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,ucentral-jsonschema))
