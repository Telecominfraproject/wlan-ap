include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=cig-poe-judgment
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/cig-poe-judgment
  SUBMENU:=Other modules
  TITLE:=CIG POE judgment
  FILES:=$(PKG_BUILD_DIR)/cig_poe_judgment.ko
  AUTOLOAD:=$(call AutoLoad,99,cig_poe_judgment)
endef

define KernelPackage/cig-poe-judgment/description
  Find POE type
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

$(eval $(call KernelPackage,cig-poe-judgment))
