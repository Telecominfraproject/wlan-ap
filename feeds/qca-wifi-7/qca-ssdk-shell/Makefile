
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
PKG_MIRROR_HASH:=ca026260c1d3c15b0a86b58f0a2cc9375c3647a3e320dbc888c460b853b0353f
# ATH12.4.0CS
# PKG_MIRROR_HASH:=acdfd941a1da1934a19d5974f98fa113e0eac3b506e94bc832393fe19bf9c2a7
# PKG_VERSION:=f07a66e3d52174c2f0806b9d11015fb5a4880972
# ATH12.4.0ED2
PKG_VERSION:=271fab9973c9fd75783ee87b90a1b5dadc5c20ff

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

	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/build/bin/libssdk_us.so $(1)/usr/lib/
endef


$(eval $(call BuildPackage,qca-ssdk-shell))
