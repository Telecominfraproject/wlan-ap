include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-nss-ecm
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/oss/lklm/qca-nss-ecm
PKG_MIRROR_HASH:=8b33559ee7cd36331f0dbb0e861672f52e80977333a01c3c70d17f2b1d84635d
PKG_VERSION:=7b7ea2753e0918c6c82318b80b87e8d45a3495a3

LOCAL_SRC:=$(TOPDIR)/qca/src/nss/clients/qca-nss-ecm

LOCAL_VARIANT=$(patsubst qca-nss-ecm-%,%,$(patsubst qca-nss-ecm-%,%,$(BUILD_VARIANT)))
include $(INCLUDE_DIR)/package.mk

ifeq ($(CONFIG_QCA_NSS_ECM_EXAMPLES_PCC),y)
       FILES_EXAMPLES=$(PKG_BUILD_DIR)/examples/ecm_pcc_test.ko
endif

ifeq ($(CONFIG_QCA_NSS_ECM_EXAMPLES_MARK),y)
       FILES_EXAMPLES+=$(PKG_BUILD_DIR)/examples/ecm_mark_test.ko
endif

ifeq ($(CONFIG_QCA_NSS_ECM_EXAMPLES_SDX),y)
	FILES_EXAMPLES=$(PKG_BUILD_DIR)/examples/ecm_sdx_pcc.ko
endif

#Explicitly enable OVS external module, if ovsmgr is enabled.
ifneq ($(CONFIG_PACKAGE_kmod-qca-ovsmgr),)
CONFIG_QCA_NSS_ECM_OVS=y
endif

ifeq ($(CONFIG_QCA_NSS_ECM_OVS),y)
       FILES_EXAMPLES+=$(PKG_BUILD_DIR)/examples/ecm_ovs.ko
endif

define KernelPackage/qca-nss-ecm/Default
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  DEPENDS:=+TARGET_ipq806x:kmod-qca-nss-drv \
	+TARGET_ipq_ipq806x:kmod-qca-nss-drv \
	+TARGET_ipq_ipq807x:kmod-qca-nss-drv \
	+TARGET_ipq_ipq807x_64:kmod-qca-nss-drv \
	+TARGET_ipq807x:kmod-qca-nss-drv \
	+TARGET_ipq_ipq60xx:kmod-qca-nss-drv \
	+TARGET_ipq_ipq60xx_64:kmod-qca-nss-drv \
	+TARGET_ipq60xx:kmod-qca-nss-drv \
	+TARGET_ipq_ipq50xx:kmod-qca-nss-drv \
	+TARGET_ipq_ipq50xx_64:kmod-qca-nss-drv \
	+TARGET_ipq50xx:kmod-qca-nss-drv \
	+!(TARGET_ipq53xx_ipq53xx_32_QSDK_256||TARGET_ipq54xx_ipq54xx_32_QSDK_256):iptables-mod-extra \
	+!(TARGET_ipq53xx_ipq53xx_32_QSDK_256||TARGET_ipq54xx_ipq54xx_32_QSDK_256):kmod-ipt-conntrack \
	+(TARGET_ipq53xx_ipq53xx_32_QSDK_256||TARGET_ipq54xx_ipq54xx_32_QSDK_256):kmod-nf-conntrack \
	+!(TARGET_ipq53xx_ipq53xx_32_QSDK_256||TARGET_ipq54xx_ipq54xx_32_QSDK_256):kmod-vxlan \
	+kmod-pppoe @!LINUX_3_18 \
	+PACKAGE_kmod-qca-ovsmgr:kmod-qca-ovsmgr
  TITLE:=QCA NSS Enhanced Connection Manager (ECM)
  FILES:=$(PKG_BUILD_DIR)/*.ko $(FILES_EXAMPLES) $(PKG_BUILD_DIR)/examples/ecm_ae_select.ko
  KCONFIG:=CONFIG_BRIDGE_NETFILTER=y \
	CONFIG_NF_CONNTRACK_EVENTS=y \
	CONFIG_NF_CONNTRACK_CHAIN_EVENTS=y \
	CONFIG_NF_CONNTRACK_DSCPREMARK_EXT=y
  MENU:=1
  PROVIDES:=kmod-qca-nss-ecm
endef

define KernelPackage/qca-nss-ecm-nonss/Default
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  DEPENDS:=@TARGET_ipq50xx +PACKAGE_kmod-qca-hyfi-bridge:kmod-qca-hyfi-bridge +PACKAGE_kmod-qca-mcs:kmod-qca-mcs +kmod-nat46 \
	+iptables-mod-extra +kmod-ipt-conntrack +kmod-pppoe @!LINUX_3_18 +PACKAGE_kmod-qca-ovsmgr:kmod-qca-ovsmgr \
	+kmod-l2tp +kmod-pppol2tp +kmod-pptp +kmod-qca-nss-sfe +TARGET_ipq95xx:kmod-qca-nss-ppe-vxlanmgr +TARGET_ipq53xx:kmod-qca-nss-ppe-vxlanmgr \
	+TARGET_ipq54xx:kmod-qca-nss-ppe-vxlanmgr +kmod-vxlan \
	+kmod-bonding PACKAGE_kmod-emesh-sp:kmod-emesh-sp
  TITLE:=QCA NSS Enhanced Connection Manager (ECM)
  FILES:=$(PKG_BUILD_DIR)/*.ko $(FILES_EXAMPLES) $(PKG_BUILD_DIR)/examples/ecm_ae_select.ko
  KCONFIG:=CONFIG_BRIDGE_NETFILTER=y \
	CONFIG_NF_CONNTRACK_EVENTS=y \
	CONFIG_NF_CONNTRACK_CHAIN_EVENTS=y \
	CONFIG_NF_CONNTRACK_DSCPREMARK_EXT=y
  MENU:=1
  PROVIDES:=kmod-qca-nss-ecm
endef

define KernelPackage/qca-nss-ecm/Description/Default
This package contains the QCA NSS Enhanced Connection Manager
endef

define KernelPackage/qca-nss-ecm/Default/install
	$(INSTALL_DIR) $(1)/etc/firewall.d $(1)/etc/init.d $(1)/usr/bin $(1)/lib/netifd/offload $(1)/etc/config $(1)/etc/uci-defaults $(1)/etc/sysctl.d
	$(INSTALL_DATA) ./files/qca-nss-ecm.firewall $(1)/etc/firewall.d/qca-nss-ecm
	$(INSTALL_BIN) ./files/qca-nss-ecm.init $(1)/etc/init.d/qca-nss-ecm
	$(INSTALL_BIN) ./files/ecm_dump.sh $(1)/usr/bin/
	$(INSTALL_BIN) ./files/on-demand-down $(1)/lib/netifd/offload/on-demand-down
	$(INSTALL_DATA) ./files/qca-nss-ecm.uci $(1)/etc/config/ecm
	$(INSTALL_DATA) ./files/qca-nss-ecm.defaults $(1)/etc/uci-defaults/99-qca-nss-ecm
	$(INSTALL_BIN) ./files/qca-nss-ecm.sysctl $(1)/etc/sysctl.d/qca-nss-ecm.conf
ifeq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
	echo 'net.netfilter.nf_conntrack_max=2048' >> $(1)/etc/sysctl.d/qca-nss-ecm.conf
else ifeq ($(CONFIG_LOWMEM_FLASH),y)
	echo 'net.netfilter.nf_conntrack_max=2048' >> $(1)/etc/sysctl.d/qca-nss-ecm.conf
else ifeq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),512)
	echo 'net.netfilter.nf_conntrack_max=8192' >> $(1)/etc/sysctl.d/qca-nss-ecm.conf
endif
endef

# Variant with additional features enabled for standard profile
define KernelPackage/qca-nss-ecm-standard
  $(call KernelPackage/qca-nss-ecm/Default)
  TITLE+= (with standard features)
  VARIANT:=standard
  DEPENDS+=+PACKAGE_kmod-qca-mcs:kmod-qca-mcs \
	+TARGET_ipq95xx:kmod-qca-nss-sfe +TARGET_ipq95xx:kmod-qca-nss-ppe \
	+TARGET_ipq53xx:kmod-qca-nss-sfe +TARGET_ipq53xx:kmod-qca-nss-ppe \
	+TARGET_ipq54xx:kmod-qca-nss-sfe +TARGET_ipq54xx:kmod-qca-nss-ppe \
	+TARGET_ipq96xx:kmod-qca-nss-sfe +TARGET_ipq54xx:kmod-qca-nss-ppe \
	+TARGET_ipq52xx:kmod-qca-nss-sfe +TARGET_ipq52xx:kmod-qca-nss-ppe \
	+PACKAGE_kmod-qca-hyfi-bridge:kmod-qca-hyfi-bridge \
	+!(TARGET_ipq53xx_ipq53xx_32_QSDK_256||TARGET_ipq54xx_ipq54xx_32_QSDK_256)&&(TARGET_ipq53xx||TARGET_ipq54xx):kmod-bonding \
	+TARGET_ipq95xx:kmod-bonding +PACKAGE_kmod-emesh-sp:kmod-emesh-sp
endef

define KernelPackage/qca-nss-ecm-standard/Description
  $(call KernelPackage/qca-nss-ecm/Description/Default)
endef

define KernelPackage/qca-nss-ecm-standard/install
$(call KernelPackage/qca-nss-ecm/Default/install, $(1))
endef

# Variant with additional features enabled for premium profile
define KernelPackage/qca-nss-ecm-premium/Default
$(call KernelPackage/qca-nss-ecm/Default)
  TITLE+= (with premium features)
  VARIANT:=premium
  DEPENDS+=+PACKAGE_kmod-qca-hyfi-bridge:kmod-qca-hyfi-bridge +PACKAGE_kmod-qca-mcs:kmod-qca-mcs +kmod-nat46 \
	+kmod-l2tp +kmod-pppol2tp +kmod-pptp +TARGET_ipq95xx:kmod-qca-nss-sfe +TARGET_ipq95xx:kmod-qca-nss-ppe \
	+TARGET_ipq96xx:kmod-qca-nss-sfe +TARGET_ipq96xx:kmod-qca-nss-ppe +TARGET_ipq96xx:kmod-qca-nss-ppe-vxlanmgr \
	+TARGET_ipq52xx:kmod-qca-nss-sfe +TARGET_ipq52xx:kmod-qca-nss-ppe +TARGET_ipq52xx:kmod-qca-nss-ppe-vxlanmgr \
	+TARGET_ipq53xx:kmod-qca-nss-sfe +TARGET_ipq53xx:kmod-qca-nss-ppe +TARGET_ipq95xx:kmod-qca-nss-ppe-vxlanmgr +TARGET_ipq53xx:kmod-qca-nss-ppe-vxlanmgr \
	+TARGET_ipq54xx:kmod-qca-nss-sfe +TARGET_ipq54xx:kmod-qca-nss-ppe +TARGET_ipq54xx:kmod-qca-nss-ppe-vxlanmgr \
	+TARGET_ipq95xx:kmod-qca-nss-ppe-tunipip6 +TARGET_ipq53xx:kmod-qca-nss-ppe-tunipip6 +TARGET_ipq54xx:kmod-qca-nss-ppe-tunipip6 +TARGET_ipq96xx:kmod-qca-nss-ppe-tunipip6 +TARGET_ipq52xx:kmod-qca-nss-ppe-tunipip6 \
	+kmod-bonding PACKAGE_kmod-emesh-sp:kmod-emesh-sp
endef

define KernelPackage/qca-nss-ecm-nonss
  $(call KernelPackage/qca-nss-ecm-nonss/Default)
  TITLE+= (with no-nss for host mode)
  VARIANT:=nonss
endef

define KernelPackage/qca-nss-ecm-wifi-plugin
  TITLE+= (with no-nss for host mode)
  DEPENDS+=@TARGET_ipq95xx||TARGET_ipq53xx||TARGET_ipq54xx \
		+PACKAGE_kmod-qca-wifi-unified-profile:kmod-qca-wifi +PACKAGE_kmod-ath:kmod-ath \
		+PACKAGE_kmod-qca-wifi-lowmem-profile:kmod-qca-wifi \
		+PACKAGE_kmod-qca-nss-ecm-premium:kmod-qca-nss-ecm-premium \
		+PACKAGE_kmod-qca-nss-ecm-standard:kmod-qca-nss-ecm-standard \
		+PACKAGE_kmod-qca-nss-ecm-noload:kmod-qca-nss-ecm-noload \
		+PACKAGE_kmod-ath12k:kmod-ath12k

  FILES+=$(PKG_BUILD_DIR)/ecm_wifi_plugins/ecm-wifi-plugin.ko
endef

define KernelPackage/qca-nss-ecm-wifi-plugin/Description/Default
$(call KernelPackage/qca-nss-ecm/Description/Default)
with the ecm-wifi-plugin enabled
endef

define KernelPackage/qca-nss-ecm-premium/Description/Default
$(call KernelPackage/qca-nss-ecm/Description/Default)
with the premium features enabled
endef

define KernelPackage/qca-nss-ecm-nonss/Description/Default
$(call KernelPackage/qca-nss-ecm/Description/Default)
with the No-NSS features enabled
endef

define KernelPackage/qca-nss-ecm-premium/Default/install
$(call KernelPackage/qca-nss-ecm/install)
endef

define KernelPackage/qca-nss-ecm-nonss/install
$(call KernelPackage/qca-nss-ecm-standard/install, $(1))
endef

define KernelPackage/qca-nss-ecm-premium
$(call KernelPackage/qca-nss-ecm-premium/Default)
endef

define KernelPackage/qca-nss-ecm-premium/Description
$(call KernelPackage/qca-nss-ecm-premium/Description/Default)
endef

define KernelPackage/qca-nss-ecm-premium/install
$(call KernelPackage/qca-nss-ecm-standard/install, $(1))
endef

# Variant with additional features enabled for noload profile
define KernelPackage/qca-nss-ecm-noload
  $(call KernelPackage/qca-nss-ecm/Default)
  TITLE+= (with noload features)
  PROVIDES:=kmod-qca-nss-ecm
  VARIANT:=noload
  DEPENDS+=+kmod-l2tp +kmod-pppol2tp +kmod-pptp +TARGET_ipq95xx:kmod-qca-nss-sfe +TARGET_ipq95xx:kmod-qca-nss-ppe \
	   +TARGET_ipq96xx:kmod-qca-nss-sfe +TARGET_ipq96xx:kmod-qca-nss-ppe \
	   +TARGET_ipq52xx:kmod-qca-nss-sfe +TARGET_ipq52xx:kmod-qca-nss-ppe \
	   +TARGET_ipq53xx:kmod-qca-nss-sfe +TARGET_ipq53xx:kmod-qca-nss-ppe +TARGET_ipq53xx:kmod-qca-nss-ppe-tunipip6 \
	   +TARGET_ipq54xx:kmod-qca-nss-sfe +TARGET_ipq54xx:kmod-qca-nss-ppe +TARGET_ipq54xx:kmod-qca-nss-ppe-tunipip6 \
	   +kmod-bonding +PACKAGE_kmod-qca-mcs:kmod-qca-mcs +PACKAGE_kmod-qca-nss-ppe-mapt:kmod-nat46 \
	   +TARGET_ipq95xx:kmod-emesh-sp +TARGET_ipq95xx:kmod-qca-nss-ppe-tunipip6 \
	   +TARGET_ipq52xx:kmod-emesh-sp +TARGET_ipq52xx:kmod-qca-nss-ppe-tunipip6 \
	   +TARGET_ipq96xx:kmod-emesh-sp +TARGET_ipq96xx:kmod-qca-nss-ppe-tunipip6
endef

define KernelPackage/qca-nss-ecm-noload/Description
  $(call KernelPackage/qca-nss-ecm/Description/Default)
  When selected, this package installs the driver but does not load it at init.
endef

define KernelPackage/qca-nss-ecm-noload/install
$(call KernelPackage/qca-nss-ecm/Default/install, $(1))
	#
	# Remove the START line from the init script, so that the symlink
	# in the /etc/rc.d directory is not created.
	#
	sed -i '/START=/d' $(1)/etc/init.d/qca-nss-ecm
endef

define KernelPackage/qca-nss-ecm-premium-noload
  $(call KernelPackage/qca-nss-ecm-premium/Default)
  PROVIDES:=kmod-qca-nss-ecm-premium
endef

define KernelPackage/qca-nss-ecm-premium-noload/Description
  $(call KernelPackage/qca-nss-ecm-premium/Description/Default)
  When selected, this package installs the driver but does not load it at init.
endef

define KernelPackage/qca-nss-ecm-premium-noload/install
$(call KernelPackage/qca-nss-ecm-premium/Default/install, $(1))
endef

define Build/InstallDev/qca-nss-ecm
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-ecm
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-ecm/
endef

define Build/InstallDev
	$(call Build/InstallDev/qca-nss-ecm,$(1))
endef

ifneq ($(VARIANT), nonss)
EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include/qca-nss-drv
endif

ifeq ($(CONFIG_KERNEL_KASAN), y)
EXTRA_CFLAGS+= -Wframe-larger-than=2500
endif

EXTRA_CFLAGS+= \
	-I$(STAGING_DIR)/usr/include \
	-I$(STAGING_DIR)/usr/include/qca-wifi \
	-I$(STAGING_DIR)/usr/include/qca-nss-sfe \
	-I$(STAGING_DIR)/usr/include/qca-nss-ppe

ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
ifneq ($(CONFIG_LOWMEM_FLASH),y)
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/nat46
endif
endif

ifeq ($(CONFIG_PACKAGE_kmod-qca-hyfi-bridge), y)
ECM_MAKE_OPTS:=ECM_CLASSIFIER_HYFI_ENABLE=y
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/hyfibr
endif

ifneq ($(CONFIG_PACKAGE_kmod-ath),)
ECM_MAKE_OPTS+=ECM_OPEN_PROFILE_ENABLE=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ecm-wifi-plugin),)
CONFIG_QCA_NSS_ECM_WIFI_PLUGIN=y
ifneq ($(CONFIG_PACKAGE_kmod-ath),)
ifneq ($(CONFIG_PACKAGE_kmod-emesh-sp),)
ECM_MAKE_OPTS+=ECM_WIFI_PLUGIN_OPEN_PROFILE_ENABLE=y
ECM_MAKE_OPTS+=ECM_FRONT_END_FSE_ENABLE=y
ECM_MAKE_OPTS+=ECM_CLASSIFIER_MSCS_ENABLE=y
endif
else
ifneq ($(CONFIG_PACKAGE_kmod-emesh-sp),)
ECM_MAKE_OPTS+=ECM_CLASSIFIER_MSCS_SCS_ENABLE=y
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/emesh-sp
endif
ECM_MAKE_OPTS+=ECM_FRONT_END_FSE_ENABLE=y
endif
endif

ifeq ($(CONFIG_PACKAGE_kmod-qca-mcs), y)
ECM_MAKE_OPTS+=ECM_MULTICAST_ENABLE=y
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-mcs
endif

ifneq ($(LOCAL_VARIANT),standard)
ECM_MAKE_OPTS+=	ECM_INTERFACE_IPSEC_ENABLE=y \
		ECM_INTERFACE_PPTP_ENABLE=y \
		ECM_INTERFACE_L2TPV2_ENABLE=y \
		ECM_INTERFACE_GRE_TAP_ENABLE=y \
		ECM_INTERFACE_GRE_TUN_ENABLE=y \
		ECM_INTERFACE_SIT_ENABLE=y \
		ECM_INTERFACE_TUNIPIP6_ENABLE=y \
		ECM_INTERFACE_RAWIP_ENABLE=y

ifeq ($(CONFIG_TARGET_ipq_ipq40xx)$(CONFIG_TARGET_ipq40xx),)
ECM_MAKE_OPTS+=ECM_INTERFACE_BOND_ENABLE=y
endif
endif

ifeq ($(LOCAL_VARIANT),standard)
ifeq ($(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq54xx),y)
ifeq ($(CONFIG_TARGET_ipq53xx_ipq53xx_32_QSDK_256)$(CONFIG_TARGET_ipq54xx_ipq54xx_32_QSDK_256),)
ECM_MAKE_OPTS+=ECM_INTERFACE_BOND_ENABLE=y
endif
endif
endif

ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
ifneq ($(CONFIG_LOWMEM_FLASH),y)
ifneq ($(CONFIG_XFRM),)
ECM_MAKE_OPTS+=ECM_XFRM_ENABLE=y
endif
ifeq ($(BUILD_VARIANT), nonss)
ECM_MAKE_OPTS+=ECM_FRONT_END_SFE_ENABLE=y
FILES_EXAMPLES+=$(PKG_BUILD_DIR)/examples/ecm_sfe_l2.ko
endif
ECM_MAKE_OPTS+=ECM_NON_PORTED_SUPPORT_ENABLE=y
endif
endif

ifneq ($(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq54xx),)
ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ecm-premium)$(CONFIG_PACKAGE_kmod-qca-nss-ecm-nonss),)
ECM_MAKE_OPTS+=ECM_INTERFACE_VXLAN_ENABLE=y
endif
ECM_MAKE_OPTS+=ECM_INTERFACE_SKIP_ACCEL_ENABLE=n
ECM_MAKE_OPTS+=ECM_INTERFACE_DSA_ENABLE=y
endif

ifeq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
ECM_MAKE_OPTS+=ECM_FRONT_END_CONN_LIMIT_ENABLE=y
ECM_MAKE_OPTS+=ECM_256M_PROFILE=y
else ifeq ($(CONFIG_LOWMEM_FLASH),y)
ECM_MAKE_OPTS+=ECM_FRONT_END_CONN_LIMIT_ENABLE=y
endif

ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
ifneq ($(CONFIG_LOWMEM_FLASH),y)
ifneq ($(CONFIG_PACKAGE_kmod-nat46),)
ECM_MAKE_OPTS+=ECM_INTERFACE_MAP_T_ENABLE=y
endif
endif
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-drv-ovpn-link),)
ECM_MAKE_OPTS+=ECM_INTERFACE_OVPN_ENABLE=y
endif

ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
ifneq ($(CONFIG_LOWMEM_FLASH),y)
ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-drv-vxlanmgr),)
ECM_MAKE_OPTS+=ECM_INTERFACE_VXLAN_ENABLE=y
endif
endif
endif

ifneq ($(CONFIG_TARGET_ipq95xx),)
ECM_MAKE_OPTS+=ECM_FRONT_END_SFE_ENABLE=y
ECM_MAKE_OPTS+=ECM_FRONT_END_PPE_ENABLE=y
FILES_EXAMPLES+=$(PKG_BUILD_DIR)/examples/ecm_sfe_l2.ko
endif

ifneq ($(CONFIG_TARGET_ipq53xx),)
ECM_MAKE_OPTS+=ECM_FRONT_END_SFE_ENABLE=y
ECM_MAKE_OPTS+=ECM_FRONT_END_PPE_ENABLE=y
ECM_MAKE_OPTS+=ECM_MHT_ENABLE=y
FILES_EXAMPLES+=$(PKG_BUILD_DIR)/examples/ecm_sfe_l2.ko
endif

ifneq ($(CONFIG_TARGET_ipq54xx),)
ECM_MAKE_OPTS+=ECM_FRONT_END_SFE_ENABLE=y
ECM_MAKE_OPTS+=ECM_FRONT_END_PPE_ENABLE=y
ECM_MAKE_OPTS+=ECM_MHT_ENABLE=y
FILES_EXAMPLES+=$(PKG_BUILD_DIR)/examples/ecm_sfe_l2.ko
endif

ifneq ($(CONFIG_TARGET_ipq96xx),)
ECM_MAKE_OPTS+=ECM_FRONT_END_SFE_ENABLE=y
ECM_MAKE_OPTS+=ECM_FRONT_END_PPE_ENABLE=y
ECM_MAKE_OPTS+=ECM_MHT_ENABLE=y
FILES_EXAMPLES+=$(PKG_BUILD_DIR)/examples/ecm_sfe_l2.ko
endif

ifneq ($(CONFIG_TARGET_ipq52xx),)
ECM_MAKE_OPTS+=ECM_FRONT_END_SFE_ENABLE=y
ECM_MAKE_OPTS+=ECM_FRONT_END_PPE_ENABLE=y
ECM_MAKE_OPTS+=ECM_MHT_ENABLE=y
FILES_EXAMPLES+=$(PKG_BUILD_DIR)/examples/ecm_sfe_l2.ko
endif

ifneq ($(findstring 6.1, $(LINUX_VERSION)),)
ECM_MAKE_OPTS+=ECM_CLASSIFIER_WIFI_ENABLE=y
endif

ifneq ($(findstring 6.6, $(LINUX_VERSION)),)
ECM_MAKE_OPTS+=ECM_CLASSIFIER_WIFI_ENABLE=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-ath),)
ECM_MAKE_OPTS+=ECM_CLASSIFIER_WIFI_ENABLE=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-ppe-qdisc),)
ECM_MAKE_OPTS+=ECM_FRONT_END_PPE_QOS_ENABLE=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-ovsmgr),)
ECM_MAKE_OPTS+=ECM_INTERFACE_OVS_BRIDGE_ENABLE=y \
		ECM_CLASSIFIER_OVS_ENABLE=y
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/qca-ovsmgr
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-drv-igs),)
ECM_MAKE_OPTS+=ECM_CLASSIFIER_DSCP_IGS=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-emesh-sp),)
ECM_MAKE_OPTS+=ECM_CLASSIFIER_EMESH_ENABLE=y
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/emesh-sp
endif

ifneq ($(CONFIG_PACKAGE_kmod-qca-nss-drv-mscs),)
ifneq ($(CONFIG_PACKAGE_kmod-emesh-sp),)
ECM_MAKE_OPTS+=ECM_CLASSIFIER_MSCS_SCS_ENABLE=y
EXTRA_CFLAGS+= -I$(STAGING_DIR)/usr/include/emesh-sp
endif
ECM_MAKE_OPTS+=ECM_CLASSIFIER_MSCS_ENABLE=y
endif

ifneq ($(CONFIG_PACKAGE_kmod-macvlan),)
ECM_MAKE_OPTS+=ECM_INTERFACE_MACVLAN_ENABLE=y
endif

ifeq ($(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq54xx), y)
ifneq ($(CONFIG_KERNEL_IPQ_MEM_PROFILE),256)
ECM_MAKE_OPTS+=ECM_INTERFACE_L2TPV3_ENABLE=y
endif
endif

ifeq ($(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq54xx), y)
ECM_MAKE_OPTS+=ECM_FRONT_END_BRIDGE_ISOLATION_ENABLE=y
endif

# Disable ECM IPv6 support when global IPv6 support is disabled.
ifneq ($(CONFIG_IPV6),)
ECM_MAKE_OPTS+=ECM_IPV6_ENABLE=y
endif

# Disable ECM Bridge VLAN Filtering support when global kernel config is disabled
ifneq ($(CONFIG_BRIDGE_VLAN_FILTERING),)
ECM_MAKE_OPTS+=ECM_BRIDGE_VLAN_FILTERING_ENABLE=y
endif

# Keeping default as ipq806x for branches that does not have subtarget framework
ifeq ($(CONFIG_TARGET_ipq),y)
subtarget:=$(SUBTARGET)
else
subtarget:=$(CONFIG_TARGET_BOARD)
endif

# Enable NSS frontend for all the platforms except ipq40xx, ipq95xx, ipq52xx, ipq53xx and ipq54xx
ifeq ($(CONFIG_TARGET_ipq_ipq40xx)$(CONFIG_TARGET_ipq40xx)$(CONFIG_TARGET_ipq95xx)$(CONFIG_TARGET_ipq53xx)$(CONFIG_TARGET_ipq54xx)$(CONFIG_TARGET_ipq96xx)$(CONFIG_TARGET_ipq52xx),)
ifneq ($(BUILD_VARIANT), nonss)
ECM_MAKE_OPTS+=ECM_FRONT_END_NSS_ENABLE=y
endif
endif

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/qca-nss-ecm
	$(CP) $(PKG_BUILD_DIR)/exports/* $(1)/usr/include/qca-nss-ecm
endef

define Build/Compile
	$(KERNEL_MAKE) $(strip $(ECM_MAKE_OPTS)) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)" SoC="$(subtarget)" \
		EXAMPLES_BUILD_PCC="$(CONFIG_QCA_NSS_ECM_EXAMPLES_PCC)" \
		EXAMPLES_BUILD_MARK="$(CONFIG_QCA_NSS_ECM_EXAMPLES_MARK)" \
		EXAMPLES_BUILD_OVS="$(CONFIG_QCA_NSS_ECM_OVS)" \
		BUILD_ECM_WIFI_PLUGIN="$(CONFIG_QCA_NSS_ECM_WIFI_PLUGIN)" \
		EXAMPLES_BUILD_SDX="$(CONFIG_QCA_NSS_ECM_EXAMPLES_SDX)" \
		modules
endef

define KernelPackage/qca-nss-ecm-premium/config
menu "ECM Configuration"
	config QCA_NSS_ECM_EXAMPLES_PCC
		bool "Build PCC usage example"
		help
			Selecting this will build the PCC classifier usage example module.
		default y if TARGET_ipq95xx || TARGET_ipq53xx || TARGET_ipq54xx

	config QCA_NSS_ECM_EXAMPLES_MARK
		bool "Build Mark classifier usage example"
		help
			Selecting this will build the Mark classifier usage example module.
		default n

	config QCA_NSS_ECM_OVS
		bool "Build OVS classifier external module"
		help
			Selecting this will build the OVS classifier external module.
		default n

	config QCA_NSS_ECM_EXAMPLES_SDX
		bool "Build PCC SDX module"
		help
			Selecting this will build the SDX PCC classifier usage module.
		default n
endmenu
endef

$(eval $(call KernelPackage,qca-nss-ecm-noload))
$(eval $(call KernelPackage,qca-nss-ecm-standard))
$(eval $(call KernelPackage,qca-nss-ecm-premium-noload))
$(eval $(call KernelPackage,qca-nss-ecm-premium))
$(eval $(call KernelPackage,qca-nss-ecm-nonss))
$(eval $(call KernelPackage,qca-nss-ecm-wifi-plugin))
