include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=telemetry-agent
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=NHSS.QSDK.12.4.5
PKG_RELEASE:=1
PKG_SOURCE_URL:=https://git.codelinaro.org/clo/qsdk/wifi/qca-telemetry-agent-oss
PKG_MIRROR_HASH:=f6d5dd8fe2c5fb79b7c8bf8f041a11366237307d7ee1f0172efac5b40e27e0f7
PKG_VERSION:=662277a5f9f9f3be1a9cd4c97c475a10f42f6637

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  TITLE:=Telemetry Agent
  DEPENDS+= kmod-ath12k-qca
  FILES:=$(PKG_BUILD_DIR)/telemetry_agent.ko
endef

define KernelPackage/$(PKG_NAME)/description
	This package contains telemetry agent support for open source profile.
endef

MAKE_OPTS:= -C "$(PKG_BUILD_DIR)" \
	CROSS_COMPILE="$(KERNEL_CROSS)" \
	ARCH="$(LINUX_KARCH)" \
	EXTRA_CFLAGS="-Wall -Wno-unused-variable" \
	KERNEL_PATH="${LINUX_DIR}" \
	BUILD_PROFILE="Open" \
	PWD="$(PKG_BUILD_DIR)"

define Build/Compile
	$(KERNEL_MAKE) $(MAKE_OPTS) all
endef

$(eval $(call KernelPackage,$(PKG_NAME)))
