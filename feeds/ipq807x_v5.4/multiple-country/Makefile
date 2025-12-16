include $(TOPDIR)/rules.mk

PKG_NAME:=multiple-country
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKGARCH:=all

include $(INCLUDE_DIR)/package.mk

define Build/Compile
endef

define Package/multiple-country
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Watch /etc/config/wireless country change (ipq50xx only)
  DEPENDS:=@TARGET_ipq50xx
endef

define Package/multiple-country/install
	$(INSTALL_DIR)  $(1)/etc/init.d
	$(INSTALL_BIN)  ./files/countrywatch      $(1)/etc/init.d/

	$(INSTALL_DIR)  $(1)/usr/bin
	$(INSTALL_BIN)  ./files/countrywatch.sh   $(1)/usr/bin/
	$(INSTALL_BIN)  ./files/set_country.sh    $(1)/usr/bin/
endef

$(eval $(call BuildPackage,multiple-country))
