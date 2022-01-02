define KernelPackage/crypto-core
  SUBMENU:=$(CRYPTO_MENU)
  TITLE:=Core CryptoAPI modules
  KCONFIG:= \
    CONFIG_CRYPTO=y \
    CONFIG_CRYPTO_HW=y \
    CONFIG_CRYPTO_BLKCIPHER \
    CONFIG_CRYPTO_ALGAPI \
    $(foreach mod,$(CRYPTO_MODULES),$(call crypto_confvar,$(mod)))
  FILES:=$(foreach mod,$(CRYPTO_MODULES),$(call crypto_file,$(mod)))
endef

$(eval $(call KernelPackage,crypto-core))


define KernelPackage/bridge
  SUBMENU:=$(NETWORK_SUPPORT_MENU)
  TITLE:=Ethernet bridging support
  DEPENDS:=+kmod-stp
  KCONFIG:= \
       CONFIG_BRIDGE \
       CONFIG_BRIDGE_IGMP_SNOOPING=y
  FILES:=$(LINUX_DIR)/net/bridge/bridge.ko
  AUTOLOAD:=$(call AutoLoad,11,bridge)
endef

define KernelPackage/bridge/description
 Kernel module for Ethernet bridging.
endef

$(eval $(call KernelPackage,bridge))

define KernelPackage/llc
  SUBMENU:=$(NETWORK_SUPPORT_MENU)
  TITLE:=ANSI/IEEE 802.2 LLC support
  KCONFIG:=CONFIG_LLC
  FILES:= \
       $(LINUX_DIR)/net/llc/llc.ko \
       $(LINUX_DIR)/net/802/p8022.ko \
       $(LINUX_DIR)/net/802/psnap.ko
  AUTOLOAD:=$(call AutoLoad,09,llc p8022 psnap)
endef

define KernelPackage/llc/description
 Kernel module for ANSI/IEEE 802.2 LLC support.
endef

$(eval $(call KernelPackage,llc))

define KernelPackage/stp
  SUBMENU:=$(NETWORK_SUPPORT_MENU)
  TITLE:=Ethernet Spanning Tree Protocol support
  DEPENDS:=+kmod-llc
  KCONFIG:=CONFIG_STP
  FILES:=$(LINUX_DIR)/net/802/stp.ko
  AUTOLOAD:=$(call AutoLoad,10,stp)
endef

define KernelPackage/stp/description
 Kernel module for Ethernet Spanning Tree Protocol support.
endef

$(eval $(call KernelPackage,stp))

define KernelPackage/8021q
  SUBMENU:=$(NETWORK_SUPPORT_MENU)
  TITLE:=802.1Q VLAN support
  KCONFIG:=CONFIG_VLAN_8021Q \
               CONFIG_VLAN_8021Q_GVRP=n
  FILES:=$(LINUX_DIR)/net/8021q/8021q.ko
  AUTOLOAD:=$(call AutoLoad,12,8021q)
endef

define KernelPackage/8021q/description
 Kernel module for 802.1Q VLAN support
endef

$(eval $(call KernelPackage,8021q))

define KernelPackage/ipv6
  SUBMENU:=$(NETWORK_SUPPORT_MENU)
  TITLE:=IPv6 support
  KCONFIG:= \
       CONFIG_IPV6 \
       CONFIG_IPV6_PRIVACY=y \
       CONFIG_IPV6_MULTIPLE_TABLES=y \
       CONFIG_IPV6_MROUTE=y \
       CONFIG_IPV6_PIMSM_V2=n \
       CONFIG_IPV6_SUBTREES=y
  FILES:=$(LINUX_DIR)/net/ipv6/ipv6.ko
  AUTOLOAD:=$(call AutoLoad,20,ipv6)
endef

define KernelPackage/ipv6/description
 Kernel modules for IPv6 support
endef

$(eval $(call KernelPackage,ipv6))


