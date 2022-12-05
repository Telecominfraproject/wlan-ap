################################################################################
# @file pkgs/stringl/Android.mk
# @brief Makefile for building the string library on Android.
################################################################################

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

libdiag_includes:= \
        $(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../src \

LOCAL_C_INCLUDES:= $(libdiag_includes)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/common/inc

LOCAL_SRC_FILES:= \
	diag_klog.c \

commonSharedLibraries :=libdiag \
	
LOCAL_MODULE:= diag_klog
LOCAL_MODULE_TAGS := optional debug
LOCAL_SHARED_LIBRARIES := $(commonSharedLibraries)

LOCAL_MODULE_OWNER := qcom
include $(BUILD_EXECUTABLE)

