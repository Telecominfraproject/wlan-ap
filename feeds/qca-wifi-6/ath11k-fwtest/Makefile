include $(TOPDIR)/rules.mk

PKG:=ath11k-fwtest
PKG_NAME:=$(PKG)
PKG_VERSION:=1.0

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG)

include $(INCLUDE_DIR)/package.mk

define Package/ath11k-fwtest
  SECTION:=QCA
  CATEGORY:=QCA
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros, Inc.
  TITLE:=QCA ath11k fwtest utility
  DEPENDS:= +libnl
endef

define Package/ath11k-fwtest/description
  This Package contains ath11k fwtest utility
endef

TARGET_CFLAGS += -O1 -Wall -fpie -I. -I$(STAGING_DIR)/usr/include/ -I $(STAGING_DIR)/usr/include/mac80211/uapi -I $(STAGING_DIR)/usr/include/libnl3
TARGET_LDFLAGS += -lnl-3 -lnl-genl-3

define Build/Compile
	mkdir -p $(PKG_BUILD_DIR)/install/sbin
	$(MAKE) -C $(PKG_BUILD_DIR)/ \
	CC="$(TARGET_CC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ath11k-fwtest $(1)/usr/sbin
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ath11k-fwtest $(1)/usr/sbin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
