ifeq ($(call is-vendor-board-platform,QCOM),true)
ifeq ($(findstring true, $(BOARD_HAVE_QCOM_FM) $(BOARD_HAVE_BLUETOOTH)),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

BDROID_DIR:= system/bt
ifeq ($(TARGET_SUPPORTS_WEARABLES),true)
QTI_DIR  := hardware/qcom/bt/msm8909/libbt-vendor
else
QTI_DIR := hardware/qcom/bt/libbt-vendor
endif

LOCAL_C_INCLUDES := $(TARGET_OUT_HEADERS)/common/inc
LOCAL_C_INCLUDES += $(BDROID_DIR)/hci/include
LOCAL_C_INCLUDES += $(QTI_DIR)/include
ifeq ($(TARGET_SUPPORTS_WEARABLES),true)
LOCAL_C_INCLUDES += device/qcom/msm8909w/opensource/bluetooth/tools/hidl_client/inc
else
LOCAL_C_INCLUDES += vendor/qcom/opensource/bluetooth/tools/hidl_client/inc
endif

LOCAL_CFLAGS := -DANDROID

ifneq ($(BOARD_ANT_WIRELESS_DEVICE), )
LOCAL_CFLAGS += -DCONFIG_ANT
endif
LOCAL_SRC_FILES := wds_main.c
LOCAL_SRC_FILES += wds_hci_pfal_linux.c

LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_EXECUTABLES)
LOCAL_MODULE := wdsdaemon
ifeq ($(PRODUCT_VENDOR_MOVE_ENABLED),true)
LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := libdiag
LOCAL_SHARED_LIBRARIES += libcutils \
                          libdl \
                          libbt-hidlclient

include $(BUILD_EXECUTABLE)
endif  # filter
endif  # is-vendor-board-platform
