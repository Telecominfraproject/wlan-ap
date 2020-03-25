OS_TARGETS += ARMVIRT

ifeq ($(TARGET),ARMVIRT)
PLATFORM=openwrt
VENDOR=armvirt
PLATFORM_DIR := platform/$(PLATFORM)
KCONFIG_TARGET ?= $(PLATFORM_DIR)/kconfig/openwrt_generic
ARCH_MK := $(PLATFORM_DIR)/build/$(PLATFORM).mk
endif
