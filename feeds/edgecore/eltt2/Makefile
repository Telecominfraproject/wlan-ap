include $(TOPDIR)/rules.mk

PKG_NAME:=eltt2
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/eltt2
	SECTION:=base
	CATEGORY:=Utilities
	TITLE:=eltt2
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/eltt2/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/eltt2 $(1)/bin/
endef

define Package/eltt2/extra_provides
    echo "libc.so.6";
endef

$(eval $(call BuildPackage,eltt2))
