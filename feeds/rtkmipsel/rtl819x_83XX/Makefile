#
# Copyright (C) 2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=rtl819x_83XX
PKG_VERSION:=0.1
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk
include $(TOPDIR)/.config

define KernelPackage/rtl819x_83XX
  SUBMENU:=Network Support
  TITLE:=rtl819x nic driver for SPW2M 83XX ethernet switch
  FILES:=$(PKG_BUILD_DIR)/rtl819x_83xx.ko
  DEFAULT:=y
  KCONFIG:= \
	CONFIG_RTL_819X_SWCORE=m
endef

define KernelPackage/rtl819x_83XX/description
 Kernel module rtl819x_83XX
endef

include $(INCLUDE_DIR)/kernel-defaults.mk

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) -a $(LINUX_DIR)/drivers/net/rtl819x/* $(PKG_BUILD_DIR)/
endef

	EXTRA_KCONFIG:= \
		CONFIG_RTL_819X_SWCORE=m \
		CONFIG_RTL_83XX_SUPPORT=y \
		CONFIG_RTL_8363NB_SUPPORT=y

	EXTRA_KFLAGS:= \
		$(patsubst CONFIG_%, -DCONFIG_%, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
		$(patsubst CONFIG_%, -DCONFIG_%, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG))))

	MAKE_OPTS:= \
		ARCH="$(LINUX_KARCH)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		SUBDIRS="$(PKG_BUILD_DIR)" \
		EXTRA_KFLAGS="$(EXTRA_KFLAGS)" \
		$(EXTRA_KCONFIG)

define Build/Compile
	$(MAKE) -C $(LINUX_DIR) $(MAKE_OPTS) modules
	$(CP) $(PKG_BUILD_DIR)/rtl819x.ko $(PKG_BUILD_DIR)/rtl819x_83xx.ko
endef

define Package/kmod-rtl819x_83XX/extra_provides
	echo "rtl819x.ko";
endef

$(eval $(call KernelPackage,rtl819x_83XX))
