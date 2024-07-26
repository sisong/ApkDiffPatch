LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := apkpatch

include $(LOCAL_PATH)/Android_i.mk

LOCAL_CFLAGS += -DNDEBUG

include $(BUILD_SHARED_LIBRARY)

