
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-ssdk-shell
PKG_SOURCE_PROTO:=git
# ATH12.4.0CS
# PKG_BRANCH:=ginger_open
# ATH12.4.0ED2
PKG_BRANCH:=NHSS.QSDK.12.4.5.r2
PKG_RELEASE:=1
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/ssdk-shell
# ATH12.4.0CS
# PKG_MIRROR_HASH:=acdfd941a1da1934a19d5974f98fa113e0eac3b506e94bc832393fe19bf9c2a7
# PKG_VERSION:=f07a66e3d52174c2f0806b9d11015fb5a4880972
# ATH12.4.0ED2
PKG_MIRROR_HASH:=a1c7e4e6d088a5e3617be6f919c5f6ba40f1f303fab08de9a2f66cfb3cee6328
PKG_VERSION:=a3afc2660124f9b069bc86b1334622dd3e071ad2

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/qca-ssdk-shell
  SECTION:=QCA
  CATEGORY:=Utilities
  TITLE:=Shell application for QCA SSDK
endef


define Package/qca-ssdk-shell/Description
  This package contains a qca-ssdk shell application for QCA chipset
endef

ifndef CONFIG_TOOLCHAIN_BIN_PATH
CONFIG_TOOLCHAIN_BIN_PATH=$(TOOLCHAIN_DIR)/bin
endif

QCASSDK_CONFIG_OPTS+= TOOL_PATH=$(CONFIG_TOOLCHAIN_BIN_PATH) \
                SYS_PATH=$(LINUX_DIR) \
                TOOLPREFIX=$(TARGET_CROSS) \
                KVER=$(LINUX_VERSION) \
                CFLAGS="$(TARGET_CFLAGS)" \
                LDFLAGS="$(TARGET_LDFLAGS)" \
                ARCH=$(LINUX_KARCH)

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(strip $(QCASSDK_CONFIG_OPTS))
endef

define Package/qca-ssdk-shell/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build/bin/ssdk_sh $(1)/usr/sbin/
endef


$(eval $(call BuildPackage,qca-ssdk-shell))
