include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=cig-wifi-mode-switch
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/cig-wifi-mode-sw
  SUBMENU:=Other modules
  TITLE:=CIG wifi mode switch tool
  FILES:=$(PKG_BUILD_DIR)/cig_rf_switch.ko
  AUTOLOAD:=$(call AutoLoad,60,cig_rf_switch)
endef

define KernelPackage/cig-wifi-mode-sw/description
  CIG wifi mode switch tool for configure wifi mode 2 bands or 3 bands
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

define KernelPackage/cig-wifi-mode-sw/install
	$(INSTALL_DIR) $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/cig_wifi_mode_sw $(1)/usr/sbin/cig_wms
endef

$(eval $(call KernelPackage,cig-wifi-mode-sw))
