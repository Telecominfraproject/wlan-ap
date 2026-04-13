include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-wifi-plugins
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=2
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/nss-plugins
PKG_MIRROR_HASH:=7ba05b01205dc101c4a79a5175ce8b0aab6500de05797f4c08ebc7ee1201aeb6
PKG_SOURCE_VERSION:=605d7fdf82c0309e39983142ab6e72552f7183ad
PKG_VERSION:=1.0

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

