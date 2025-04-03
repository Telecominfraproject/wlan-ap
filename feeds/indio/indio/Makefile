include $(TOPDIR)/rules.mk

PKG_NAME:=indio
PKG_VERSION:=1.0
PKG_BUILD_DIR:= $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/indio
	SECTION:=base
	CATEGORY:=Utilities
	TITLE:=indio Package
	DEPENDS:=+base-files
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef

Build/Compile = $(Build/Compile/Default)

define Package/indio/install
	cp -rf ./files/* $(1)
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_DATA) ./files/rc.local $(1)/etc/rc.local
	$(INSTALL_DATA) ./files/sys_led.sh $(1)/bin/sys_led.sh
	$(INSTALL_DATA) ./files/hwwatchdog.sh $(1)/bin/hwwatchdog.sh
endef																						
	$(eval $(call BuildPackage,indio))
