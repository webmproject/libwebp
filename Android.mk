LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
	src/bits.c \
	src/dsp.c \
	src/frame.c \
	src/quant.c \
	src/tree.c \
	src/vp8.c \
	src/webp.c \
	src/yuv.c

LOCAL_CFLAGS := -Wall -DANDROID -DHAVE_MALLOC_H -DHAVE_PTHREAD \
                -finline-functions -frename-registers -ffast-math \
                -s -fomit-frame-pointer

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src

LOCAL_MODULE:= webp-decode

include $(BUILD_STATIC_LIBRARY)
