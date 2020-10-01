include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-fw-hk
PKG_BRANCH:=nss
PKG_VERSION:=383
PKG_RELEASE:=1
PKG_MD5SUM:=skip

NSS_VER:=2.0
NSS_SOC:=HK
NSS_PROFILE:=R

PKG_SOURCE:=BIN-NSS.$(NSS_SOC).$(NSS_VER)-$(PKG_VERSION)-$(NSS_PROFILE).tar.bz2
PKG_SOURCE_URL:=http://qcaswexport-scl.qualcomm.com/NSS.FW/$(NSS_VER)/$(NSS_SOC)/$(PKG_VERSION)/
PKG_BUILD_DIR:=$(BUILD_DIR)/BIN-NSS.$(NSS_SOC).$(NSS_VER)-$(PKG_VERSION)-$(NSS_PROFILE)

include $(INCLUDE_DIR)/package.mk

define NssFirmware

  define Package/$(PKG_NAME)-$(1)
    SECTION:=kernel
    CATEGORY:=Kernel modules
    SUBMENU:=Network Devices
    DEPENDS:=@TARGET_ipq807x_ipq807x
    TITLE:=NSS firmware binaries - $(1)
    PROVIDES:=$(PKG_NAME)
    MENU:=1
  endef

  define Package/$(PKG_NAME)-$(1)/description
  This package contains the NSS firmwares ($(1)) for QCA chipset
  endef

  define Package/$(PKG_NAME)-$(1)/install
	$(INSTALL_DIR) $$(1)/lib/firmware/qca
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/$(2)0.bin $$(1)/lib/firmware/qca-nss0-$(1).bin
  ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/$(2)1.bin $$(1)/lib/firmware/qca-nss1-$(1).bin
  endif
  endef
  $$(eval $$(call BuildPackage,$(PKG_NAME)-$(1)))
endef

Build/Compile=true

$(eval $(call NssFirmware,retail,retail_router))
