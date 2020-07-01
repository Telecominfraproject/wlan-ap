OS_TARGETS +=IPQ40XX

ifeq ($(TARGET),IPQ40XX)
PLATFORM=openwrt
VENDOR=ipq40xx
PLATFORM_DIR := platform/$(PLATFORM)
KCONFIG_TARGET ?= $(PLATFORM_DIR)/kconfig/openwrt_generic
ARCH_MK := $(PLATFORM_DIR)/build/$(PLATFORM).mk
endif
