# Emscripten makefile, to create libwebpdecoder.js

#### Customizable part ####

EMSCRIPTEN=/opt/emscripten

EXTRA_FLAGS =

# Extra flags to enable byte swap for 16 bit colorspaces.
# EXTRA_FLAGS += -DWEBP_SWAP_16BIT_CSP

# Force 8-bit operations for JS compatibility
EXTRA_FLAGS += -DWEBP_REFERENCE_IMPLEMENTATION

# Extra Flags for sse2
EXTRA_FLAGS += -DWEBP_USE_SSE2 -DWEBP_EMSCRIPTEN
EXTRA_FLAGS += -msse -msse2

# Extra flags to emulate C89 strictness with the full ANSI
EXTRA_FLAGS += -DNDEBUG -Wextra -Wold-style-definition
EXTRA_FLAGS += -Wmissing-prototypes
EXTRA_FLAGS += -Wmissing-declarations
EXTRA_FLAGS += -Wdeclaration-after-statement
EXTRA_FLAGS += -Wshadow
EXTRA_FLAGS += -s EXPORTED_FUNCTIONS='["_WebpToSDL"]'
# EXTRA_FLAGS += -Wvla

# Emscripten specfic flags
EXTRA_FLAGS += -s ASSERTIONS=0

#### Nothing should normally be changed below this line ####

AR = $(EMSCRIPTEN)/emar
ARFLAGS = r
CC = $(EMSCRIPTEN)/emcc
CPPFLAGS = -Isrc/ -Wall
CFLAGS = $(EXTRA_FLAGS)
OPT = -O3  # emcc will warn if used when building a library.
LDFLAGS = $(EXTRA_LIBS) -lm

DEC_OBJS = \
    src/dec/alpha.bc \
    src/dec/buffer.bc \
    src/dec/frame.bc \
    src/dec/idec.bc \
    src/dec/io.bc \
    src/dec/layer.bc \
    src/dec/quant.bc \
    src/dec/tree.bc \
    src/dec/vp8.bc \
    src/dec/vp8l.bc \
    src/dec/webp.bc \

DSP_OBJS = \
    src/dsp/cpu.bc \
    src/dsp/dec.bc \
    src/dsp/yuv.bc \
    src/dsp/cpu.bc \
    src/dsp/dec_sse2.bc \
    src/dsp/enc.bc \
    src/dsp/enc_sse2.bc \
    src/dsp/lossless.bc \
    src/dsp/upsampling.bc \
    src/dsp/upsampling_sse2.bc \
    src/dsp/yuv.bc \

EX_UTIL_OBJS = \
    examples/example_util.bc \

UTILS_OBJS = \
    src/utils/bit_reader.bc \
    src/utils/bit_writer.bc \
    src/utils/color_cache.bc \
    src/utils/filters.bc \
    src/utils/huffman.bc \
    src/utils/quant_levels.bc \
    src/utils/rescaler.bc \
    src/utils/thread.bc \
    src/utils/utils.bc \

LIBWEBPDECODER_OBJS = $(DEC_OBJS) $(DSP_OBJS) $(UTILS_OBJS)

HDRS_INSTALLED = \
    src/webp/decode.h \
    src/webp/types.h \

HDRS = \
    $(HDRS_INSTALLED) \
    src/dec/decode_vp8.h \
    src/dec/vp8i.h \
    src/dec/vp8li.h \
    src/dec/webpi.h \
    src/dsp/dsp.h \
    src/dsp/lossless.h \
    src/dsp/yuv.h \
    src/utils/bit_reader.h \
    src/utils/bit_writer.h \
    src/utils/color_cache.h \
    src/utils/filters.h \
    src/utils/huffman.h \
    src/utils/quant_levels.h \
    src/utils/rescaler.h \

OUT_LIBS = examples/libexample_util.bc.a src/libwebpdecoder.bc.a
JS_LIB = emscripten/libwebpdecoder.js

OUTPUT = $(OUT_LIBS)
OUTPUT += $(JS_LIB)

all: $(OUTPUT)

%.bc: %.c $(HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

examples/libexample_util.bc.a: $(EX_UTIL_OBJS)
src/libwebpdecoder.bc.a: $(LIBWEBPDECODER_OBJS)

%.bc.a:
	$(AR) $(ARFLAGS) $@ $^

$(JS_LIB): EXTRA_FLAGS += -s INVOKE_RUN=0
$(JS_LIB): $(OUT_LIBS)
$(JS_LIB): emscripten/vwebp-sdl.c
	$(CC) $(OPT) $(CFLAGS) $(CPPFLAGS) $< $(OUT_LIBS) -o $@

clean:
	$(RM) $(OUTPUT) *~ \
              examples/*.bc examples/*~ \
              src/dec/*.bc src/dec/*~ \
              src/demux/*.bc src/demux/*~ \
              src/dsp/*.bc src/dsp/*~ \
              src/utils/*.bc src/utils/*~ \
              src/webp/*~ man/*~ doc/*~ swig/*~ \

.PHONY: all clean
.SUFFIXES:
