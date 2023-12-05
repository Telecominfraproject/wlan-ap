include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-psdk
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/qca-psdk
PKG_VERSION:=402973acc8a17d9b685ed3c3490b7d6289f4798f

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-psdk
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=PSDK driver
  FILES:=$(PKG_BUILD_DIR)/obj/qca808x.ko
  PROVIDES:=qca-psdk
endef

define Package/qca-psdk-cli
  SECTION:=QCA
  CATEGORY:=Utilities
  TITLE:=Shell application for QCA PSDK
endef

define KernelPackage/qca-psdk/Description
This package contains a qca-psdk driver for QCA chipset
endef

ifdef CONFIG_TOOLCHAIN_BIN_PATH
TOOLCHAIN_BIN_PATH=$(CONFIG_TOOLCHAIN_BIN_PATH)
else
TOOLCHAIN_BIN_PATH=$(TOOLCHAIN_DIR)/bin
endif

ifdef CONFIG_TARGET_NAME
QCAPSDK_CONFIG_OPTS+= \
		      TARGET_NAME=$(CONFIG_TARGET_NAME)
endif

QCAPSDK_CONFIG_OPTS+= TOOL_PATH=$(TOOLCHAIN_BIN_PATH) \
                LINUX_DIR=$(LINUX_DIR) \
                CROSS_COMPILE=$(TARGET_CROSS) \
                ARCH=$(LINUX_KARCH)

define Build/Prepare
	cd $(LOCAL_SRC)/scripts;perl qca-ssdk2psdk.pl
	$(CP) $(LOCAL_SRC)/* $(PKG_BUILD_DIR)/  -a
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(strip $(QCAPSDK_CONFIG_OPTS))
endef

define Build/InstallDev
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/qca-psdk
endef

define Package/qca-psdk-cli/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/obj/psdk_cli $(1)/usr/sbin/
endef

#$(eval $(call KernelPackage,qca-psdk))
#$(eval $(call BuildPackage,qca-psdk-cli))
