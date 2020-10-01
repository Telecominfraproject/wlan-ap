include $(TOPDIR)/rules.mk

PKG_NAME:=ath11k-firmware
PKG_SOURCE:=WLAN.HK.2.4.0.1.r1-00026-QCAHKSWPL_SILICONZ-2.tar.bz2
PKG_VERSION:=26

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk

define Package/ath11k-firmware-default
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  DEPENDS:=
endef

define Package/ath11k-firmware-ipq60xx
$(Package/ath11k-firmware-default)
  TITLE:=ath11k firmware for IPQ60xx devices
  DEPENDS:=@TARGET_ipq807x_ipq60xx
endef

define Package/ath11k-firmware-ipq807x
$(Package/ath11k-firmware-default)
  TITLE:=ath11k firmware for IPQ807x devices
  DEPENDS:=@TARGET_ipq807x_ipq807x
endef

define Package/ath11k-firmware-ipq60xx/description
Standard ath11k firmware for IPQ60xx from QCA
endef

define Package/ath11k-firmware-ipq807x/description
Standard ath11k firmware for IPQ807x from QCA
endef

define Build/Compile

endef

define Package/ath11k-firmware-ipq60xx/install
	$(INSTALL_DIR) $(1)/lib/firmware/IPQ6018
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/IPQ6018/* \
		$(1)/lib/firmware/IPQ6018/
endef

define Package/ath11k-firmware-ipq807x/install
	$(INSTALL_DIR) $(1)/lib/firmware/IPQ8074
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/IPQ8074/* \
		$(1)/lib/firmware/IPQ8074
endef

$(eval $(call BuildPackage,ath11k-firmware-ipq60xx))
$(eval $(call BuildPackage,ath11k-firmware-ipq807x))
