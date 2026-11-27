include $(TOPDIR)/rules.mk

PKG_NAME:=ucentral-wireguard
PKG_RELEASE:=1

PKG_LICENSE:=BSD-3-Clause

include $(INCLUDE_DIR)/package.mk

define Package/ucentral-wireguard
  SECTION:=ucentral
  CATEGORY:=uCentral
  TITLE:=WireGuard support for the uCentral wireguard service
  DEPENDS:=+wireguard-tools +(LINUX_5_4||LINUX_4_4):kmod-wireguard-backport +!(LINUX_5_4||LINUX_4_4):kmod-wireguard
endef

define Package/ucentral-wireguard/description
 Selects wireguard-tools and the WireGuard kernel module that matches the
 target kernel, so that services.wireguard can bring up wg0. Kernels
 without in-tree WireGuard use the backport module.
endef

define Build/Compile

endef

define Package/ucentral-wireguard/install
	true
endef

$(eval $(call BuildPackage,ucentral-wireguard))
