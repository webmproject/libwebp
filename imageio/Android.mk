LOCAL_PATH := $(call my-dir)

################################################################################
# libimageio_util

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    imageio_util.c \

LOCAL_CFLAGS := $(WEBP_CFLAGS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../src

LOCAL_MODULE := imageio_util

include $(BUILD_STATIC_LIBRARY)

################################################################################
# libimagedec

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    image_dec.c \
    jpegdec.c \
    metadata.c \
    pngdec.c \
    tiffdec.c \
    webpdec.c \

LOCAL_CFLAGS := $(WEBP_CFLAGS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../src

LOCAL_MODULE := imagedec

include $(BUILD_STATIC_LIBRARY)
