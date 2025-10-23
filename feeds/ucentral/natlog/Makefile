include $(TOPDIR)/rules.mk

PKG_NAME:=natlog
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_MAINTAINER:=kmk

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

# Define dependencies
define KernelPackage/natlog
  SUBMENU:=Netfilter Extensions
  TITLE:=NFLOG NAT translation logger
  FILES:=$(PKG_BUILD_DIR)/natlog.ko
  DEPENDS:=+kmod-nf-conntrack +kmod-nf-ipt
endef

define KernelPackage/natlog/description
  Kernel module for logging NAT translations via NFLOG.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

define KernelPackage/natlog/install
	$(CP) ./files/* $(1)/
endef

$(eval $(call KernelPackage,natlog))
