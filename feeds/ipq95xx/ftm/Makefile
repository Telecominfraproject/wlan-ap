include $(TOPDIR)/rules.mk

PKG:=ftm
PKG_NAME:=$(PKG)
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/common-tools/ftm

PKG_VERSION:=12.3

#PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QTI software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  TITLE:= QCA ftm utils
  DEPENDS:= @TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64||TARGET_ipq_ipq60xx||TARGET_ipq_ipq60xx_64||TARGET_ipq_ipq50xx||TARGET_ipq_ipq50xx_64||TARGET_ipq807x||TARGET_ipq50xx||TARGET_ipq60xx||TARGET_ipq95xx||TARGET_ipq53xx +libnl +libtcmd +qca-diag +librt +kmod-diag-char
endef

define Package/$(PKG_NAME)/description/Default
	FTM Package Support for QCA WIFI 11 drivers
endef

TARGET_CFLAGS += -DCONFIG_FTM_WLAN -DDEBUG -DFTM_DEBUG -DWIN_AP_HOST
TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/qca-diag
TARGET_CFLAGS += -MMD -O2 -Wall -g
TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include
TARGET_CFLAGS += -fpie
TARGET_LDFLAGS += -ldiag -lnl-3 -lnl-genl-3 -lrt -ltcmd
TARGET_CSRCS := ftm_main.c ftm_wlan.c ftm_write_to_flash.c
TARGET_LDFLAGS += -pie

ifeq ($(CONFIG_FEATURE_IPQ_PROVISION_SUPPORT),y)
        TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/qti-mfg-provision
        TARGET_CFLAGS += -DWIN_AP_AFC
        TARGET_LDFLAGS += -lprovision
endif

ifneq ($(CONFIG_PACKAGE_kmod-mac80211),)
        TARGET_CFLAGS+=-DWIN_AP_HOST_OPEN=1
endif

ifeq ($(CONFIG_FEATURE_QCA_IOT),y)
TARGET_CFLAGS += -DIPQ_AP_HOST_IOT -DIPQ_AP_HOST_IOT_QCA402X -ggdb3 -DCONFIG_DAEMON_MODE
TARGET_CSRCS += ftm_iot.c
TARGET_LDFLAGS += -lpthread
TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/qca-iot
TARGET_LDFLAGS += -ldiag_demo
endif

ifeq ($(CONFIG_FEATURE_IPQ_IOT_SUPPORT),y)
TARGET_CFLAGS += -DIPQ_AP_HOST_IOT -DIPQ_AP_HOST_IOT_IPQ -ggdb3 -I$(STAGING_DIR)/usr/include/btdaemon
TARGET_CSRCS += ftm_iot.c
TARGET_LDFLAGS += -lpthread -lbtdaemon
endif

define Build/Compile
$(MAKE) -C $(PKG_BUILD_DIR) \
	CC="$(TARGET_CC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	CSRCS="$(TARGET_CSRCS)"
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ftm $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ftm.init $(1)/etc/init.d/ftm
	$(INSTALL_DIR) $(1)/lib/wifi
	$(INSTALL_BIN) ./files/compress_vart.sh $(1)/lib/compress_vart.sh
ifneq (, $(findstring ipq95xx, $(CONFIG_TARGET_BOARD)))
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) ./files/ftm_qcc710_start.sh $(1)/sbin/ftm_qcc710_start
endif
endef

$(eval $(call BuildPackage,ftm))
