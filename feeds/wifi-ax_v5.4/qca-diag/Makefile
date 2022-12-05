
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-diag
#PKG_SOURCE_PROTO:=git
#PKG_BRANCH:=master
PKG_RELEASE:=1

PKG_VERSION:=11.5
#PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
#PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
#PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/qca-diag
  SECTION:=QCA
  CATEGORY:=QCA
  TITLE:=QCA Linux diag software
  DEPENDS:=@TARGET_ipq50xx||TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64||TARGET_ipq807x||TARGET_ipq60xx||TARGET_ipq95xx +libpthread
endef

define Package/qca-diag/Description
  This package contains a Linux qca diag application such as on device logging
  and socket logging for QCA chipset.
  This package enables diag messages to log over sockets and also expose
  interface for other modules to route the diag message to kernel diag
  stack.
endef


QCASSDK_CONFIG_OPTS+= TOOL_PATH=$(TOOLCHAIN_DIR)/bin/ \
                SYS_PATH=$(LINUX_DIR) \
                TOOLPREFIX=$(TARGET_CROSS) \
                KVER=$(LINUX_VERSION) \
                ARCH=$(LINUX_KARCH)


define Build/InstallDev
	$(INSTALL_DIR) $(STAGING_DIR)/usr/include/qca-diag $(STAGING_DIR)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/include/* $(STAGING_DIR)/usr/include/qca-diag/
	$(CP) $(PKG_BUILD_DIR)/lib/libdiag.so $(STAGING_DIR)/usr/lib/
endef


define Package/qca-diag/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/diag_socket_app $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/diag_stress_app $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/registerReboot $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/qld_server $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/diag_mdlog $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/libdiag.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/qdss_setup.sh $(1)/usr/bin/
endef


$(eval $(call BuildPackage,qca-diag))

define KernelPackage/coresight-stream-sock
  TITLE:=Add Coresight network stream module
  KCONFIG:=CONFIG_CORESIGHT_STREAM
  DEPENDS:=\
	+kmod-udptunnel4 \
	+IPV6:kmod-udptunnel6
  FILES:=$(LINUX_DIR)/drivers/hwtracing/coresight/coresight-stream.ko
endef

define KernelPackage/coresight-stream-sock/description
  Add Coresight network stream module
endef

$(eval $(call KernelPackage,coresight-stream-sock))
