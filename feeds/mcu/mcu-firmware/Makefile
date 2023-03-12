#
# Copyright (C) 2023 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mcu-firmware
PKG_VERSION:=2023-03-12
PKG_RELEASE:=1

PKG_MAINTAINER:=Piotr Dymacz <pepe2k@gmail.com>

include $(INCLUDE_DIR)/package.mk

define Package/mcu-fw-default
  CATEGORY:=Firmware
  SUBMENU:=MCU firmware
  SECTION:=firmware
  TITLE:=MCU firmware
endef


# Zephyr versions
# 4 fields separated with underscore:
#   version-name_git-sha_pipeline-id_sha256-of-package
ZEPHYR_VERSIONS := \
	zephyr-v3.3.x_7055d10e538e_3877473859_321a0daf6328698a913c6504d19aa85a5170dfce6039b86d31d2e9162d34af7c \
	zephyr-main_cf50a3c570bb_3912606668_10922f34cdde9e16cbee76a13a14fb935b9d1fe13eb9136cba31de27033a9c3e

ZEPHYR_FW_CI_URL := https://gitlab.com/pepe2k/zephyr/-/jobs/

ZEPHYR_FW_TYPES := \
	hello_world \
	hci_uart \
	hci_usb

# Zephyr 'hello_world' targets
ZEPHYR_HELLO_WORLD_TARGETS := \
	cig_wf196_nrf52833 \
	edgecore_eap102_nrf52840 \
	nrf52840dongle_nrf52840

# Zephyr 'hci_uart' targets
ZEPHYR_HCI_UART_TARGETS:= \
	cig_wf196_nrf52833 \
	edgecore_eap102_nrf52840 \
	nrf52840dongle_nrf52840

# Zephyr 'hci_usb' targets
ZEPHYR_HCI_USB_TARGETS := \
	edgecore_eap102_nrf52840 \
	nrf52840dongle_nrf52840

word-underscore = $(word $2,$(subst _, ,$1))

define Download/zephyr-fw
  URL:=$(ZEPHYR_FW_CI_URL)$(call word-underscore,$(2),3)/
  URL_FILE:=artifacts/download?file_type=archive
  FILE:=$(call word-underscore,$(2),1)-$(call word-underscore,$(2),2).zip
  HASH:=$(call word-underscore,$(2),4)
endef
$(foreach FW,$(ZEPHYR_VERSIONS),$(eval $(call Download,zephyr-fw,$(FW))))

# $(1) firmware name (e.g. hci_usb)
define zephyr-fw-host-support
  define Package/zephyr-$(1)-host-support
    $(call Package/mcu-fw-default)
    DEPENDS:=+mcu
    TITLE:=Zephyr '$(1)' common host side support
    HIDDEN:=1
  endef

  define Package/zephyr-$(1)-host-support/install
	$(CP) ./files/$(1)/* $$(1)/
  endef
endef

# $(1) version (e.g zephyr-v3.3.x)
# $(2) firmware name (e.g. hci_usb)
# $(3) target/board name (e.g. nrf52840dongle_nrf52840)
define zephyr-fw
  define Package/$(1)-$(2)-$(3)
    $(call Package/mcu-fw-default)
    DEPENDS:=+mcu $(call zephyr-fw-deps,$(2))
    TITLE:=Zephyr '$(2)'
  endef

  define Package/$(1)-$(2)-$(3)/description
    Zephyr '$(1)' based firmware '$(2)' for '$(3)' board
  endef

  define Package/$(1)-$(2)-$(3)/install
	$(INSTALL_DIR) $$(1)/lib/firmware/mcu/$(3)/$(1)__$(2)
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/$(1)/$(3)/$(1)__$(2)/*.bin \
		$$(1)/lib/firmware/mcu/$(3)/$(1)__$(2)/
  endef
endef

define zephyr-fw-deps
	$(if $(wildcard ./files/$(1)/*),+zephyr-$(1)-host-support) \
	$(if $(findstring hci_u,$1),+bluez-daemon +kmod-bluetooth +kmod-crypto-user)
endef

define zephyr-fw-unzip
	mkdir -p $(PKG_BUILD_DIR)/$(call word-underscore,$(1),1); \
	unzip -q -d $(PKG_BUILD_DIR)/$(call word-underscore,$(1),1) \
	  $(DL_DIR)/$(call word-underscore,$(1),1)-$(call word-underscore,$(1),2).zip; \
	for fw in $(PKG_BUILD_DIR)/$(call word-underscore,$(1),1)/*.tar.gz; do \
		$(TAR) -C $(PKG_BUILD_DIR)/$(call word-underscore,$(1),1) --one-top-level -xzf $$$$fw; \
		rm -rf $$$$fw; \
	done;
endef

define Build/Prepare
	$(foreach FW,$(ZEPHYR_VERSIONS),$(call zephyr-fw-unzip,$(FW)))
endef

define Build/Compile
endef


# Generate host side support packages (per firmware type)
$(foreach FW,$(ZEPHYR_FW_TYPES),\
  $(if $(wildcard ./files/$(FW)/*),$(eval $(call zephyr-fw-host-support,$(FW)))))

$(foreach FW,$(ZEPHYR_FW_TYPES),\
  $(if $(wildcard ./files/$(FW)/*),$(eval $(call BuildPackage,zephyr-$(FW)-host-support))))


# Generate dedicated Zephyr firmware packages (per firmware version, type and board)
$(foreach VER,$(ZEPHYR_VERSIONS),\
  $(foreach TARGET,$(ZEPHYR_HELLO_WORLD_TARGETS),\
    $(eval $(call zephyr-fw,$(call word-underscore,$(VER),1),hello_world,$(TARGET)))))

$(foreach VER,$(ZEPHYR_VERSIONS),\
  $(foreach TARGET,$(ZEPHYR_HCI_UART_TARGETS),\
    $(eval $(call zephyr-fw,$(call word-underscore,$(VER),1),hci_uart,$(TARGET)))))

$(foreach VER,$(ZEPHYR_VERSIONS),\
  $(foreach TARGET,$(ZEPHYR_HCI_USB_TARGETS),\
    $(eval $(call zephyr-fw,$(call word-underscore,$(VER),1),hci_usb,$(TARGET)))))


$(foreach VER,$(ZEPHYR_VERSIONS),\
  $(foreach TARGET,$(ZEPHYR_HELLO_WORLD_TARGETS),\
    $(eval $(call BuildPackage,$(call word-underscore,$(VER),1)-hello_world-$(TARGET)))))

$(foreach VER,$(ZEPHYR_VERSIONS),\
  $(foreach TARGET,$(ZEPHYR_HCI_UART_TARGETS),\
    $(eval $(call BuildPackage,$(call word-underscore,$(VER),1)-hci_uart-$(TARGET)))))

$(foreach VER,$(ZEPHYR_VERSIONS),\
  $(foreach TARGET,$(ZEPHYR_HCI_USB_TARGETS),\
    $(eval $(call BuildPackage,$(call word-underscore,$(VER),1)-hci_usb-$(TARGET)))))
