#
# Copyright (C) 2008-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qosify
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

PKG_BUILD_DEPENDS:=bpf-headers

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk
include $(INCLUDE_DIR)/bpf.mk

define Package/qosify
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  TITLE:=QoS classifier eBPF module
  DEPENDS:=+libbpf +libubox +libubus +kmod-sched-cake +tc-full
  PKGFLAGS+=nonshared
endef

BPF_DOC = $(wildcard $(patsubst %,$(BPF_HEADERS_DIR)/scripts/%.py,bpf_doc bpf_helpers_doc))

define Build/Compile
	$(call CompileBPF,$(PKG_BUILD_DIR)/qosify-bpf.c)
	$(Build/Compile/Default)
endef

define Package/qosify/conffiles
/etc/config/qosify
/etc/qosify-defaults.conf
endef

define Package/qosify/install
	$(INSTALL_DIR) $(1)/lib/bpf $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config $(1)/etc/hotplug.d/net
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/qosify-bpf.o $(1)/lib/bpf
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/qosify $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/qosify.init $(1)/etc/init.d/qosify
	$(INSTALL_DATA) ./files/qosify-defaults.conf $(1)/etc/qosify-defaults.conf
	$(INSTALL_DATA) ./files/qosify.conf $(1)/etc/config/qosify
	$(INSTALL_DATA) ./files/qosify.hotplug $(1)/etc/hotplug.d/net/10-qosify
endef

$(eval $(call BuildPackage,qosify))
