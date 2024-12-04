include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/version.mk

PKG_NAME:=ath12k-wifi
PKG_RELEASE:=1
PKG_FLAGS:=nonshared

include $(INCLUDE_DIR)/package.mk

RSTRIP:=:
STRIP:=:

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile
endef

#
# This is intended to be used on an interim basis until device-specific
# board data for new devices is available through the upstream compilation
#
# Please send a mail with your device-specific board files upstream.
# You can find instructions and examples on the linux-wireless wiki:
# <https://wireless.wiki.kernel.org/en/users/drivers/ath10k/boardfiles>
#

define Package/ath12k-wifi-default
  SUBMENU:=ath12k Board-Specific Overrides
  SECTION:=firmware
  CATEGORY:=Firmware
  DEPENDS:=@TARGET_ipq95xx
  TITLE:=Custom Board
endef

define Package/ath12k-wifi-qcom-qcn92xx
$(call Package/ath12k-wifi-default)
    TITLE:=board-2.bin for QCOM QCN92XX eval kits
endef

define Package/ath12k-wifi-qcom-qcn92xx/install
	$(INSTALL_DIR) $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/
	$(INSTALL_DATA) ./board-2.bin.QCN92XX $(1)/lib/firmware/ath12k/QCN92XX/hw1.0/board-2.bin
endef

$(eval $(call BuildPackage,ath12k-wifi-qcom-qcn92xx))
