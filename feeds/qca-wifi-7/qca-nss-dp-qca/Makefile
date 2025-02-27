include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-dp-qca
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=ginger_open
PKG_RELEASE:=1
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/nss-dp
PKG_MIRROR_HASH:=4963f82e16dbe0662de92658a744ea2c5571b21047ac9bfa88e6e55633e820a1
PKG_VERSION:=fb3311980ad4a8211ca99c82079b2d7c36f6c353

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)
include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-nss-dp-qca
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  DEPENDS:=@TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq807x||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq60xx||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64||TARGET_ipq50xx||TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx \
		+kmod-qca-ssdk-qca-nohnat \
		+TARGET_ipq95xx:kmod-qca-nss-ppe \
		+TARGET_ipq53xx:kmod-qca-nss-ppe \
		+TARGET_ipq54xx:kmod-qca-nss-ppe
  TITLE:=Kernel driver for NSS data plane
  FILES:=$(PKG_BUILD_DIR)/qca-nss-dp.ko
  AUTOLOAD:=$(call AutoLoad,31,qca-nss-dp)
endef

define KernelPackage/qca-nss-dp-qca/Description
This package contains a NSS data plane driver for QCA chipset
endef

define Build/InstallDev
	mkdir -p $(1)/usr/include/qca-nss-dp
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-dp/
endef

EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-ssdk \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe

ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

ifeq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
EXTRA_CFLAGS+= -DNSS_DP_MEM_PROFILE_LOW
else ifeq ($(CONFIG_LOWMEM_FLASH),y)
EXTRA_CFLAGS+= -DNSS_DP_MEM_PROFILE_LOW
else ifeq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),512)
EXTRA_CFLAGS+= -DNSS_DP_MEM_PROFILE_MEDIUM
endif

ifeq ($(CONFIG_TARGET_ipq95xx_generic_QSDK_MinEnt), y)
EXTRA_CFLAGS+= -DNSS_DP_POINT_OFFLOAD
endif

ifneq ($(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq54xx),)
EXTRA_CFLAGS+= -DNSS_DP_NETSTANDBY
MAKE_OPTS+=dp-net-standby=y
MAKE_OPTS+=dp-ppe-ds=y
endif

NSS_DP_HAL_DIR:=$(PKG_BUILD_DIR)/hal
ifneq (, $(findstring _64, $(subtarget)))
hal_arch:=$(subst _64,,$(subtarget))
else ifneq (, $(findstring _32, $(subtarget)))
hal_arch:=$(subst _32,,$(subtarget))
else
hal_arch:=$(subtarget)
endif

ifneq (, $(findstring $(hal_arch),"ipq95xx" "ipq53xx" "ipq54xx"))
define KernelPackage/qca-nss-dp-qca/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/qca-nss-dp.init $(1)/etc/init.d/qca-nss-dp
	$(INSTALL_BIN) ./files/qca-nss-dp.conf $(1)/etc/config/qca_nss_dp
	$(INSTALL_BIN) ./files/edma_dump $(1)/usr/bin/edma_dump
endef
endif

define Build/Configure
	$(LN) $(NSS_DP_HAL_DIR)/soc_ops/$(hal_arch)/nss_$(hal_arch).h \
		$(PKG_BUILD_DIR)/exports/nss_dp_arch.h
endef

define Build/Compile
	$(KERNEL_MAKE) $(strip $(MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" SoC="$(hal_arch)" \
		modules
endef

$(eval $(call KernelPackage,qca-nss-dp-qca))
