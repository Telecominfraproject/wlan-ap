include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ppe-ds
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=ginger_open
PKG_RELEASE:=2
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/nss-ppe
PKG_MIRROR_HASH:=41e78523f7d0367380d609aea34230dedc231c3630bdc9bbe7ccc1fbeb5a0500
PKG_VERSION:=30aab9c3973e6b950bfcd442b23d1156c3576148

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

ifneq (, $(findstring _64, $(subtarget)))
hal_arch:=$(subst _64,,$(subtarget))
else ifneq (, $(findstring _32, $(subtarget)))
hal_arch:=$(subst _32,,$(subtarget))
else
hal_arch:=$(subtarget)
endif

define KernelPackage/qca-nss-ppe-ds
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for PPE Direct Switch (core driver)
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq95xx_32||TARGET_ipq53xx||TARGET_ipq53xx_32||TARGET_ipq54xx \
	+kmod-qca-nss-ppe-vp \
	+kmod-qca-nss-dp-qca
  FILES:=$(PKG_BUILD_DIR)/drv/ppe_ds/qca-nss-ppe-ds.ko
  AUTOLOAD:=$(call AutoLoad,51,qca-nss-ppe-ds)
endef

ifneq (, $(findstring $(hal_arch),"ipq95xx" "ipq53xx"))
define KernelPackage/qca-nss-ppe-ds/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/qca-nss-ppe-ds.init $(1)/etc/init.d/qca-nss-ppe-ds
endef
endif

define KernelPackage/qca-nss-ppe-ds/Description
Kernel module for PPE DS core driver
endef

define Build/InstallDev/qca-nss-ppe-ds
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-ppe
	$(CP) $(PKG_BUILD_DIR)/drv/ppe_ds/exports/* $(1)/usr/include/qca-nss-ppe/
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-ppe-ds,$(1))
endef

EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe \
	-I$(STAGING_DIR)/usr/include/qca-nss-dp

MAKE_OPTS+=ppe-ds=y

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-ppe-ds))
