include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-wifi-plugins
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/nss-plugins
PKG_MIRROR_HASH:=275a46a0481b9bd89815b86f084ef5ae7ef225fb2de06f354360a75cfbb4dcc6
PKG_VERSION:=605d7fdf82c0309e39983142ab6e72552f7183ad

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-nss-wifi-plugins
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=NSS Plugin Module
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq54xx||TARGET_ipq53xx||TARGET_ipq52xx||TARGET_ipq96xx +kmod-qca-nss-ppe +kmod-qca-nss-ppe-vp +kmod-qca-nss-ppe-ds @!LINUX_3_18
  FILES:=$(PKG_BUILD_DIR)/qca-nss-wifi-plugins.ko
  AUTOLOAD:=$(call AutoLoad,53,qca-nss-wifi-plugins)
endef

define KernelPackage/qca-nss-wifi-plugins/Description
Kernel module for NSS-WiFi Plugins
endef


EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe-ds \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe-vp

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef

define Build/InstallDev/qca-nss-wifi-plugins
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-wifi-plugins
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-wifi-plugins,$(1))
endef

$(eval $(call KernelPackage,qca-nss-wifi-plugins))

