include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-sfe
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/shortcut-fe
PKG_MIRROR_HASH:=e6b2de1ad37f00d8b337cab01566a62f045f4f18e1312624a93f9ba535753f37
PKG_SOURCE_VERSION:=5818706ab48c7a8bf45ffeee8bc3a5f5bcbc38b0
PKG_VERSION:=1.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-nss-sfe
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  DEPENDS:=@TARGET_ipq95xx||TARGET_ipq50xx||TARGET_ipq53xx||TARGET_ipq54xx||TARGET_ipq96xx||TARGET_ipq52xx +kmod-pppoe \
	   +TARGET_ipq95xx:kmod-qca-nss-ppe-rule \
	   +TARGET_ipq53xx:kmod-qca-nss-ppe-rule \
	   +TARGET_ipq54xx:kmod-qca-nss-ppe-rule \
	   +TARGET_ipq96xx:kmod-qca-nss-ppe-rule \
	   +TARGET_ipq52xx:kmod-qca-nss-ppe-rule
  TITLE:=Kernel data path for SFE
  FILES:=$(PKG_BUILD_DIR)/qca-nss-sfe.ko
  KCONFIG:=CONFIG_NF_CONNTRACK_EVENTS=y
  AUTOLOAD:=$(call AutoLoad,09,qca-nss-sfe)
endef

define KernelPackage/qca-nss-sfe/Description
Shortcut is an in-Linux-kernel IP packet forwarding engine.
endef

define KernelPackage/qca-nss-sfe/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/usr/bin/sfe_dump $(1)/usr/bin
endef

ifneq (, $(filter $(CONFIG_IPV6),m y))
SFE_MAKE_OPTS:=SFE_SUPPORT_IPV6=y
endif

SFE_MAKE_OPTS+=SFE_PROCESS_LOCAL_OUT=y

# Disable SFE Bridge VLAN Filtering support when global kernel config is disabled
ifneq ($(CONFIG_BRIDGE_VLAN_FILTERING),)
SFE_MAKE_OPTS+=SFE_BRIDGE_VLAN_FILTERING_ENABLE=y
endif

EXTRA_CFLAGS+= -I$(PKG_BUILD_DIR)/exports

ifeq ($(CONFIG_LOWMEM_FLASH),y)
SFE_MAKE_OPTS+=SFE_16M_PROFILE=y
endif

ifeq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
SFE_MAKE_OPTS+=SFE_256M_PROFILE=y
EXTRA_CFLAGS+= -DSFE_MEM_PROFILE_LOW
ifneq ($(CONFIG_TARGET_ipq53xx),)
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-ppe
SFE_MAKE_OPTS+=SFE_RFS_SUPPORTED=y
endif
else ifeq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),512)
EXTRA_CFLAGS+= -DSFE_MEM_PROFILE_MEDIUM
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-ppe
SFE_MAKE_OPTS+=SFE_RFS_SUPPORTED=y
else
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-nss-ppe
SFE_MAKE_OPTS+=SFE_RFS_SUPPORTED=y
endif

ifneq ($(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq54xx),)
SFE_MAKE_OPTS+=SFE_DSA_ACCEL_ENABLE=y
endif

#PPE_QOS is supported only if PPE Qdisc is enabled
ifeq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-qdisc),y)
SFE_MAKE_OPTS+=SFE_PPE_QOS_SUPPORTED=y
endif

#TSO is supported only on Alder, Miami and Marina
ifeq ($(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq54xx), y)
EXTRA_CFLAGS+= -DSFE_TSO_MAX_SEG_LIMIT_ENABLE=y
EXTRA_CFLAGS+= -DSFE_TSO_SEG_MAX=32
else ifeq ($(CONFIG_TARGET_ipq54xx), y)
EXTRA_CFLAGS+= -DSFE_TSO_MAX_SEG_LIMIT_ENABLE=y
EXTRA_CFLAGS+= -DSFE_TSO_SEG_MAX=48
endif

#L2TPv3 is supported only on Alder, Miami and Marina Premium and 512M profile
ifeq ($(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq54xx), y)
ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
SFE_MAKE_OPTS+=SFE_L2TPV3_SUPPORTED=y
endif
endif

define Build/Compile
	$(KERNEL_MAKE)  $(strip $(SFE_MAKE_OPTS))\
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
		modules
endef

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-sfe),)
define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-sfe
	$(CP) -rf $(PKG_BUILD_DIR)/exports/sfe_api.h $(1)/usr/include/qca-nss-sfe
endef
endif

$(eval $(call KernelPackage,qca-nss-sfe))
