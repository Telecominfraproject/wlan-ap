include $(TOPDIR)/rules.mk

PKG_NAME:=ath12k-firmware
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

RSTRIP:=:
STRIP:=:

define Package/ath12k-firmware-default
  SECTION:=firmware
  CATEGORY:=Firmware
  URL:=$(PKG_SOURCE_URL)
  DEPENDS:=
endef

define Package/ath12k-firmware-qcn92xx
$(Package/ath12k-firmware-default)
  TITLE:=ath12k firmware for QCN92XX devices
  DEPENDS:=@TARGET_ipq95xx
endef

define Package/ath12k-firmware-qcn92xx/description
Standard ath12k firmware for QCN92XX from QCA
endef

define Build/Compile

endef

define Package/ath12k-firmware-qcn92xx/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DATA) ./files/QCN92XX/* \
		$(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
endef

$(eval $(call BuildPackage,ath12k-firmware-qcn92xx))
