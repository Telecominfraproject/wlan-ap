include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/version.mk

PKG_NAME:=ath11k-wifi
PKG_RELEASE:=1
PKG_FLAGS:=nonshared

include $(INCLUDE_DIR)/package.mk

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile

endef

define Package/ath11k-wifi-default
  SUBMENU:=ath11k Board-Specific Overrides
  SECTION:=firmware
  CATEGORY:=Firmware
  DEPENDS:=@TARGET_ipq95xx
  TITLE:=Custom Board
endef

define Package/ath11k-wifi-qcom-ipq95xx
$(call Package/ath11k-wifi-default)
    TITLE:=board-2.bin for QCOM IPQ9574 eval kits
endef

define Package/ath11k-wifi-qcom-ipq95xx/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath11k/IPQ9574/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.IPQ9574 $(1)/lib/firmware/ath11k/IPQ9574/hw1.0/board-2.bin
endef

$(eval $(call BuildPackage,ath11k-wifi-qcom-ipq95xx))
