LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := apkpatch
LOCAL_ARM_MODE := arm

include $(LOCAL_PATH)/Android_i.mk

LOCAL_CFLAGS     += -DDEBUG=1
LOCAL_SANITIZE   := address
include $(BUILD_SHARED_LIBRARY)

