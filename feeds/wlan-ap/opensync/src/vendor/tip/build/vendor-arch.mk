OS_TARGETS +=tip

ifeq ($(TARGET),tip)
PLATFORM=openwrt
VENDOR=tip
PLATFORM_DIR := platform/$(PLATFORM)
KCONFIG_TARGET ?= $(PLATFORM_DIR)/kconfig/openwrt_generic
ARCH_MK := $(PLATFORM_DIR)/build/$(PLATFORM).mk
endif
