include $(TOPDIR)/rules.mk

PKG_NAME:=ratelimit
PKG_RELEASE:=1

PKG_MAINTAINER:=John Crispin <john@phrozen.org>

include $(INCLUDE_DIR)/package.mk

define Package/ratelimit
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Wireless ratelimiting
  DEPENDS:=+tc +kmod-ifb
endef

define Package/ratelimit/description
	Allow Wireless client rate limiting
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
endef

define Build/Compile/Default

endef
Build/Compile = $(Build/Compile/Default)

define Package/ratelimit/install
	$(CP) ./files/* $(1)
ifeq ($(CONFIG_TARGET_mediatek),y)
	$(SED) 's|qdisc replace dev $$$${iface} parent 1:$$$${id} handle $$$${id}: fq_codel flows 128 limit 800 quantum 300 noecn|qdisc add dev $$$${iface} parent 1:$$$${id} sfq perturb 10 limit 2000 quantum 1514|' $(1)/usr/bin/ratelimit
endif
endef

$(eval $(call BuildPackage,ratelimit))
