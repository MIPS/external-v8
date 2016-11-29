LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/Android.v8common.mk

# Set up the target identity
LOCAL_MODULE := libv8sampler
LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_SRC_FILES := \
	src/libsampler/sampler.cc

LOCAL_C_INCLUDES := $(LOCAL_PATH)/src

include $(BUILD_STATIC_LIBRARY)
