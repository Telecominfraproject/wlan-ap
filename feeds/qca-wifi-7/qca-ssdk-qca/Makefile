
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-ssdk-qca
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/qca-ssdk
PKG_MIRROR_HASH:=e59418ce9a553d5b765407c9743001aa04f4e1b54205b967bf6f14b6ad30305d
PKG_SOURCE_VERSION:=4a51edc04910792fde91098b6c1a1655e63800b6
PKG_VERSION:=1.0

ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

ifeq ($(CONFIG_TARGET_ipq),y)
	subtarget:=$(SUBTARGET)
else
	subtarget:=$(CONFIG_TARGET_BOARD)
endif

ifneq (, $(findstring _64, $(subtarget)))
	hal_arch:=$(subst _64,,$(subtarget))
else ifneq (, $(findstring _32, $(subtarget)))
	hal_arch:=$(subst _32,,$(subtarget))
else
	hal_arch:=$(subtarget)
endif

ifdef KERNEL_PATCHVER
  LINUX_VERSION_NUMBERS:=$(KERNEL_PATCHVER)$(strip $(LINUX_VERSION-$(KERNEL_PATCHVER)))
else
  LINUX_VERSION_NUMBERS:=$(LINUX_VERSION)
endif

LOCAL_VARIANT=$(patsubst qca-ssdk-qca-%,%,$(patsubst qca-ssdk-qca-%,%,$(BUILD_VARIANT)))
include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-ssdk-qca/default-nohnat
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  TITLE:=Kernel driver for QCA SSDK
  FILES:=$(PKG_BUILD_DIR)/build/bin/qca-ssdk.ko
  AUTOLOAD:=$(call AutoLoad,30,qca-ssdk)
  PROVIDES:=qca-ssdk-qca
endef

define KernelPackage/qca-ssdk-qca-nohnat
$(call KernelPackage/qca-ssdk-qca/default-nohnat)
  DEPENDS:= +kmod-qca-nss-phy +kmod-qca8k-cc +kmod-qca8084
  VARIANT:=nohnat

  ifneq (, $(findstring $(hal_arch), "ipq50xx"))
    ifeq (, $(CONFIG_LOWMEM_FLASH))
      ifneq ($(LINUX_VERSION_NUMBERS),$(filter 5.4%,$(LINUX_VERSION_NUMBERS)))
      DEPENDS+=kmod-ipt-filter +kmod-ppp
      QCASSDK_CONFIG_OPTS+= HNAT_FEATURE=enable
      endif
    endif
  endif
endef

define KernelPackage/qca-ssdk-qca-nohnat/Description
This package contains a qca-ssdk driver for QCA chipset
endef

GCC_VERSION=$(shell echo "$(CONFIG_GCC_VERSION)" | sed 's/[^0-9.]*\([0-9.]*\).*/\1/')
ifeq ($(GCC_VERSION), 4.8)
	GCC_VERSION=4.8.3
endif

ifdef CONFIG_TOOLCHAIN_BIN_PATH
	TOOLCHAIN_BIN_PATH=$(CONFIG_TOOLCHAIN_BIN_PATH)
else
	TOOLCHAIN_BIN_PATH=$(TOOLCHAIN_DIR)/bin
endif

ifdef CONFIG_TARGET_NAME
	QCASSDK_CONFIG_OPTS+= \
			      TARGET_NAME=$(CONFIG_TARGET_NAME)
else ifneq ($(CONFIG_ARCH),)
	QCASSDK_CONFIG_OPTS+= \
			      TARGET_NAME=$(subst ",,$(CONFIG_ARCH))-openwrt-linux$(if $(CONFIG_TARGET_SUFFIX),-$(subst ",,$(CONFIG_TARGET_SUFFIX)))
endif

ifdef CONFIG_TARGET_PROFILE
TARGET_PROFILE=$(subst ",,$(CONFIG_TARGET_PROFILE))
PATCH_PROFILE_NAME=patches-$(subst DEVICE_,,$(TARGET_PROFILE))
FILES_PROFILE_NAME=files-$(subst DEVICE_,,$(TARGET_PROFILE))
endif

QCASSDK_CONFIG_OPTS+= TOOL_PATH=$(TOOLCHAIN_BIN_PATH) \
                SYS_PATH=$(LINUX_DIR) \
                TOOLPREFIX=$(TARGET_CROSS) \
                KVER=$(LINUX_VERSION_NUMBERS) \
                ARCH=$(LINUX_KARCH) \
		TARGET_SUFFIX=$(CONFIG_TARGET_SUFFIX) \
		GCC_VERSION=$(GCC_VERSION) \
		SoC=$(hal_arch) \
		CFLAGS="-I$(STAGING_DIR)/usr/include -fno-PIE"

ifneq (, $(findstring $(hal_arch), "ipq95xx"))
	QCASSDK_CONFIG_OPTS+= CHIP_TYPE=APPE
else ifneq (, $(findstring $(hal_arch), "ipq807x"))
	QCASSDK_CONFIG_OPTS+= CHIP_TYPE=HPPE
else ifneq (, $(findstring $(hal_arch), "ipq60xx"))
	QCASSDK_CONFIG_OPTS+= CHIP_TYPE=CPPE
else ifneq (, $(findstring $(hal_arch), "ipq50xx"))
	QCASSDK_CONFIG_OPTS+= CHIP_TYPE=MP
else ifneq (, $(findstring $(hal_arch), "ipq53xx"))
	QCASSDK_CONFIG_OPTS+= CHIP_TYPE=MPPE
else ifneq (, $(findstring $(hal_arch), "ipq54xx"))
	QCASSDK_CONFIG_OPTS+= CHIP_TYPE=MRPPE
endif

ifneq (, $(findstring $(CONFIG_KERNEL_IPQ_MEM_PROFILE), 256)$(CONFIG_LOWMEM_FLASH))
	QCASSDK_CONFIG_OPTS+= MINI_SSDK=enable
	ifeq ($(CONFIG_LOWMEM_FLASH),y)
		QCASSDK_CONFIG_OPTS+= LOWMEM_FLASH=enable
	endif
endif

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/qca-ssdk
	$(INSTALL_DIR) $(1)/usr/include/qca-ssdk/api
	$(INSTALL_DIR) $(1)/usr/include/qca-ssdk/ref
	$(INSTALL_DIR) $(1)/usr/include/qca-ssdk/fal
	$(INSTALL_DIR) $(1)/usr/include/qca-ssdk/sal
	$(INSTALL_DIR) $(1)/usr/include/qca-ssdk/init
	$(CP) -rf $(PKG_BUILD_DIR)/include/api/sw_ioctl.h $(1)/usr/include/qca-ssdk/api
	if [ -f $(PKG_BUILD_DIR)/include/ref/ref_vsi.h ]; then \
	$(CP) -rf $(PKG_BUILD_DIR)/include/ref/ref_vsi.h $(1)/usr/include/qca-ssdk/ref/; \
	fi
	if [ -f $(PKG_BUILD_DIR)/include/ref/ref_fdb.h ]; then \
	$(CP) -rf $(PKG_BUILD_DIR)/include/ref/ref_fdb.h $(1)/usr/include/qca-ssdk/ref/; \
	fi
	if [ -f $(PKG_BUILD_DIR)/include/ref/ref_port_ctrl.h ]; then \
	$(CP) -rf $(PKG_BUILD_DIR)/include/ref/ref_port_ctrl.h $(1)/usr/include/qca-ssdk/ref/; \
	fi
	$(CP) -rf $(PKG_BUILD_DIR)/include/init $(1)/usr/include/qca-ssdk
	$(CP) -rf $(PKG_BUILD_DIR)/include/fal $(1)/usr/include/qca-ssdk
	$(CP) -rf $(PKG_BUILD_DIR)/include/common/*.h $(1)/usr/include/qca-ssdk
	$(CP) -rf $(PKG_BUILD_DIR)/include/sal/os/linux/*.h $(1)/usr/include/qca-ssdk
	$(CP) -rf $(PKG_BUILD_DIR)/include/sal/os/*.h $(1)/usr/include/qca-ssdk
	touch $(PKG_BUILD_DIR)/.built
endef

LNX_CONFIG_OPTS = LNX_MAKEOPTS='$(KERNEL_MAKEOPTS)' MODULE_TYPE=KSLIB modules

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/mk $(strip $(QCASSDK_CONFIG_OPTS)) $(LNX_CONFIG_OPTS)
endef

define KernelPackage/qca-ssdk-qca-nohnat/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/qca-ssdk $(1)/etc/init.d/qca-ssdk
endef

define patch_profile
	$(call PatchDir/Default,$(PKG_BUILD_DIR),./$(PATCH_PROFILE_NAME))
endef

define files_profile

	if [ -d "./$(FILES_PROFILE_NAME)" ]; then \
		$(CP) ./$(FILES_PROFILE_NAME)/* ./files/ ; \
	fi
endef

Hooks/Prepare/Post += patch_profile
Hooks/Prepare/Post += files_profile

$(eval $(call KernelPackage,qca-ssdk-qca-nohnat))
