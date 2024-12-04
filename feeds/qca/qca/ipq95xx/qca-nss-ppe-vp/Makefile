include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ppe-vp
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=ginger_open
PKG_RELEASE:=2
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/nss-ppe
PKG_MIRROR_HASH:=02bbe74dbd71df67cee9b1793fce01955757ac0c449765dfb28c3427d06d56ca
PKG_VERSION:=62ec9067ccb476e22df4a9d98d3285c8161a5a7f

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

define KernelPackage/qca-nss-ppe-vp
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for PPE virtual port (core driver)
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx +kmod-qca-nss-ppe +kmod-qca-nss-dp-qca
  FILES:=$(PKG_BUILD_DIR)/drv/ppe_vp/qca-nss-ppe-vp.ko
  AUTOLOAD:=$(call AutoLoad,52,qca-nss-ppe-vp)
endef

define KernelPackage/qca-nss-ppe-vp/Description
   Kernel module for PPE virtual port core driver
endef

define Build/InstallDev/qca-nss-ppe-vp
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-ppe
	$(CP) $(PKG_BUILD_DIR)/drv/exports/* $(1)/usr/include/qca-nss-ppe/
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-ppe-vp,$(1))
endef

EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-nss-dp \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/fal \
	-I$(STAGING_DIR)/usr/include/qca-ssdk/init \

MAKE_OPTS+=ppe-vp=y

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		SoC="$(subtarget)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-ppe-vp))
