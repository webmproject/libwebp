LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    example_util.c \

LOCAL_CFLAGS := $(WEBP_CFLAGS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../src

LOCAL_MODULE := example_util

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    dwebp.c \

LOCAL_CFLAGS := $(WEBP_CFLAGS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../src
LOCAL_STATIC_LIBRARIES := example_util webp

LOCAL_MODULE := dwebp

include $(BUILD_EXECUTABLE)
