LOCAL_PATH:=$(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libtcmd_headers
LOCAL_CFLAGS := -Werror
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_HEADER_LIBRARY)

# Build libtcmd =========================
include $(CLEAR_VARS)

LOCAL_CLANG := true
LOCAL_MODULE := libtcmd
LOCAL_SRC_FILES:= \
    nl80211.c \
    libtcmd.c \
    os.c

ifeq ($(PRODUCT_VENDOR_MOVE_ENABLED), true)
LOCAL_PROPRIETARY_MODULE := true
endif

ifeq ($(BOARD_HAS_ATH_WLAN_AR6004),true)
	LOCAL_CFLAGS+= -DCONFIG_AR6002_REV6
endif

ifneq ($(wildcard external/libnl-headers),)
LOCAL_C_INCLUDES += external/libnl-headers
else
LOCAL_C_INCLUDES += external/libnl/include external/libnl/include/linux-private
endif

LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_CFLAGS += \
	-DWLAN_API_NL80211 \
	-DANDROID \
	-DLIBNL_2 \
	-DSYSCONFDIR="\"/etc/libnl\""\
	-Werror

ifneq ($(wildcard system/core/libnl_2),)
# ICS ships with libnl 2.0
LOCAL_SHARED_LIBRARIES := libnl_2
else
LOCAL_SHARED_LIBRARIES := libnl
endif

LOCAL_MODULE_OWNER := qcom
LOCAL_SANITIZE := signed-integer-overflow unsigned-integer-overflow

include $(BUILD_STATIC_LIBRARY)
