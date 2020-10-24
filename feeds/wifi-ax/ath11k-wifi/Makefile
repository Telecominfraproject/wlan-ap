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

#
# This is intended to be used on an interim basis until device-specific
# board data for new devices is available through the upstream compilation
#
# Please send a mail with your device-specific board files upstream.
# You can find instructions and examples on the linux-wireless wiki:
# <https://wireless.wiki.kernel.org/en/users/drivers/ath10k/boardfiles>
#

ALLWIFIBOARDS:= \
	cig-wf188 \
	cig-wf188n \
	cig-wf194c \
	edgecore-eap101 \
	sercomm-wallaby \
	edgecore-eap102

ALLWIFIPACKAGES:=$(foreach BOARD,$(ALLWIFIBOARDS),ath11k-wifi-$(BOARD))

define Package/ath11k-wifi-default
  SUBMENU:=ath11k Board-Specific Overrides
  SECTION:=firmware
  CATEGORY:=Firmware
  DEPENDS:=@TARGET_ipq807x
  TITLE:=Custom Board
endef

define Package/ath11k-wifi-qcom-ipq8074
$(call Package/ath11k-wifi-default)
    TITLE:=board-2.bin for QCOM IPQ807x eval kits
endef

define Package/ath11k-wifi-qcom-ipq6018
$(call Package/ath11k-wifi-default)
    TITLE:=board-2.bin for QCOM IPQ60xx eval kits
endef

define ath11k-wifi-install-one-to
  $(INSTALL_DIR)  $(2)/lib/firmware/$(3)/
  $(INSTALL_DATA) $(1) $(2)/lib/firmware/$(3)/board.bin
endef

define ath11k-wifi-install-one
  $(if $(filter $(suffix $(1)),.IPQ6018),\
    $(call ath11k-wifi-install-one-to,$(1),$(2),IPQ6018),\
  )
  $(if $(filter $(suffix $(1)),.IPQ8074),\
    $(call ath11k-wifi-install-one-to,$(1),$(2),IPQ8074),\
  )

endef
# Blank line required at end of above define due to foreach context

define generate-ath11k-wifi-package
  define Package/ath11k-wifi-$(1)
    $(call Package/ath11k-wifi-default)
    TITLE:=bdwlan.bin Overrides for $(2)
    CONFLICTS:=$(PREV_BOARD)
  endef

  define Package/ath11k-wifi-$(1)/description
The $(2) requires board-specific, reference ("cal") data
that is not yet present in the upstream wireless firmware distribution.

This package supplies bdwlan.bin file(s) that, in the interim,
overwrite those supplied by the ath11k-firmware-* packages.

This is package is only necessary for the $(2).

Do not install it for any other device!
  endef

  define Package/ath11k-wifi-$(1)/install-overlay
    $$$$(foreach ATH11K_WIFI_BOARD_FILE,$$$$(wildcard board-$(1).*),\
      $$$$(call ath11k-wifi-install-one,$$$$(ATH11K_WIFI_BOARD_FILE),$$(1)))
  endef

  PREV_BOARD+=ath11k-wifi-$(1)
endef

define Package/ath11k-wifi-qcom-ipq8074/install
	$(INSTALL_DIR) $(1)/lib/firmware/IPQ8074/
	$(INSTALL_DATA) ./board-2.bin.IPQ8074 $(1)/lib/firmware/IPQ8074/board-2.bin
endef

define Package/ath11k-wifi-qcom-ipq6018/install
	$(INSTALL_DIR) $(1)/lib/firmware/IPQ6018/
	$(INSTALL_DATA) ./board-2.bin.IPQ6018 $(1)/lib/firmware/IPQ6018/board-2.bin
endef

$(eval $(call generate-ath11k-wifi-package,cig-wf188,Cigtech WF188))
$(eval $(call generate-ath11k-wifi-package,cig-wf188n,Cigtech WF188n))
$(eval $(call generate-ath11k-wifi-package,cig-wf194c,Cigtech WF194c))
$(eval $(call generate-ath11k-wifi-package,sercomm-wallaby,Sercomm Kiwi))
$(eval $(call generate-ath11k-wifi-package,edgecore-eap101,EdgeCore EAP101))
$(eval $(call generate-ath11k-wifi-package,edgecore-eap102,Edgecore EAP102))

$(foreach PACKAGE,$(ALLWIFIPACKAGES),$(eval $(call BuildPackage,$(PACKAGE))))
$(eval $(call BuildPackage,ath11k-wifi-qcom-ipq8074))
$(eval $(call BuildPackage,ath11k-wifi-qcom-ipq6018))
