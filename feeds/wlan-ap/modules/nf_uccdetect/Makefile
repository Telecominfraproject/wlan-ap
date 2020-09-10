#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=nf_uccdetect
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/nf_uccdetect
  SUBMENU:=Other modules
  TITLE:=UCC Session detect using netfilter hook
  DEPENDS:=
  FILES:=$(PKG_BUILD_DIR)/nf_uccdetect.ko
  AUTOLOAD:=$(call AutoLoad,10,nf_uccdetect)
endef

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	SUBDIRS="$(PKG_BUILD_DIR)"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,nf_uccdetect))
