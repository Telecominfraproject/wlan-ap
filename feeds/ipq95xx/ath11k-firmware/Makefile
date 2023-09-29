include $(TOPDIR)/rules.mk

PKG_NAME:=ath11k-firmware
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=main
PKG_SOURCE_URL:=https://github.com/quic/upstream-wifi-fw.git
PKG_MIRROR_HASH:=557505744764b0442c887be3f5bb0ce924323d785b6d207899f19fad992a6020
PKG_SOURCE_VERSION:=3417bb86645c5ff4c58258db7cc33e43260b4222

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk

define Package/ath11k-firmware-default
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  DEPENDS:=
endef

define Package/ath11k-firmware-ipq95xx
  $(Package/ath11k-firmware-default)
  TITLE:=ath11k firmware for IPQ95xx devices
  DEPENDS:=@TARGET_ipq95xx
endef

define Build/Compile

endef

define Package/ath11k-firmware-ipq95xx/install
	$(INSTALL_DIR) $(1)/lib/firmware/IPQ9574/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/ath11k-firmware/IPQ9574/hw1.0/2.9.0.1/WLAN.HK.2.9.0.1-01890-QCAHKSWPL_SILICONZ-1/* \
		$(1)/lib/firmware/IPQ9574/
endef

$(eval $(call BuildPackage,ath11k-firmware-ipq95xx))
