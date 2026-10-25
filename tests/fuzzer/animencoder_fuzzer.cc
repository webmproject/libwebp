// Copyright 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "./fuzz_utils.h"
#include "gtest/gtest.h"
#include "src/dsp/cpu.h"
#include "src/mux/animi.h"
#include "webp/encode.h"
#include "webp/mux.h"
#include "webp/mux_types.h"

namespace {

const VP8CPUInfo default_VP8GetCPUInfo = fuzz_utils::VP8GetCPUInfo;

struct FrameConfig {
  int use_argb;
  int timestamp;
  WebPConfig webp_config;
  fuzz_utils::CropOrScaleParams crop_or_scale_params;
  fuzz_utils::WebPPictureCpp pic_cpp;
};

auto ArbitraryKMinKMax() {
  return fuzztest::PairOf(
      fuzztest::OneOf(fuzztest::InRange(std::numeric_limits<int>::min(),
                                        std::numeric_limits<int>::max()),
                      fuzztest::InRange(0, 15)),
      fuzztest::OneOf(fuzztest::InRange(std::numeric_limits<int>::min(),
                                        std::numeric_limits<int>::max()),
                      fuzztest::InRange(2, 15)));
}

int AddFrame(WebPAnimEncoder** const enc,
             const WebPAnimEncoderOptions& anim_config, int* const width,
             int* const height, int timestamp_ms, FrameConfig& frame_config,
             uint32_t* const bit_pos) {
  if (enc == nullptr || width == nullptr || height == nullptr) {
    fprintf(stderr, "NULL parameters.\n");
    if (enc != nullptr) WebPAnimEncoderDelete(*enc);
    std::abort();
  }

  // Init the source picture.
  WebPPicture& pic = frame_config.pic_cpp.ref();

  // Crop and scale.
  if (*enc == nullptr) {  // First frame will set canvas width and height.
    if (!fuzz_utils::CropOrScale(&pic, frame_config.crop_or_scale_params)) {
      const WebPEncodingError error_code = pic.error_code;
      if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return 0;
      fprintf(stderr, "ExtractAndCropOrScale failed. Error code: %d\n",
              error_code);
      std::abort();
    }
  } else {  // Other frames will be resized to the first frame's dimensions.
    if (!WebPPictureRescale(&pic, *width, *height)) {
      const WebPEncodingError error_code = pic.error_code;
      WebPAnimEncoderDelete(*enc);
      if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return 0;
      fprintf(stderr,
              "WebPPictureRescale failed. Size: %d,%d. Error code: %d\n",
              *width, *height, error_code);
      std::abort();
    }
  }

  // Create encoder if it doesn't exist.
  if (*enc == nullptr) {
    *width = pic.width;
    *height = pic.height;
    *enc = WebPAnimEncoderNew(*width, *height, &anim_config);
    if (*enc == nullptr) {
      if (getenv("NALLOC_FUZZ_VERSION") == nullptr) {
        fprintf(stderr, "WebPAnimEncoderNew failed.\n");
        std::abort();
      }
      return 0;
    }
  }

  // Create frame encoding config.
  WebPConfig config = frame_config.webp_config;
  // Skip slow settings on big images, it's likely to timeout.
  if (pic.width * pic.height > 32 * 32) {
    config.method = (config.method > 4) ? 4 : config.method;
    config.quality = (config.quality > 99.0f) ? 99.0f : config.quality;
    config.alpha_quality =
        (config.alpha_quality > 99) ? 99 : config.alpha_quality;
  }

  // Encode.
  if (!WebPAnimEncoderAdd(*enc, &pic, timestamp_ms, &config)) {
    const WebPEncodingError error_code = pic.error_code;
    WebPAnimEncoderDelete(*enc);
    // Tolerate failures when running under the nallocfuzz engine as
    // WebPAnimEncoderAdd() may fail due to memory allocation errors outside of
    // the encoder; in muxer functions that return booleans for instance.
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY ||
        error_code == VP8_ENC_ERROR_BAD_WRITE ||
        getenv("NALLOC_FUZZ_VERSION") != nullptr) {
      return 0;
    }
    fprintf(stderr, "WebPEncode failed. Error code: %d\n", error_code);
    std::abort();
  }

  return 1;
}

void AnimEncoderTest(bool minimize_size, std::pair<int, int> kmin_kmax,
                     bool allow_mixed, std::vector<FrameConfig> frame_configs,
                     int optimization_index) {
  WebPAnimEncoder* enc = nullptr;
  int width = 0, height = 0, timestamp_ms = 0;
  uint32_t bit_pos = 0;

  fuzz_utils::SetOptimization(default_VP8GetCPUInfo, optimization_index);

  // Extract a configuration from the packed bits.
  WebPAnimEncoderOptions anim_config;
  if (!WebPAnimEncoderOptionsInit(&anim_config)) {
    fprintf(stderr, "WebPAnimEncoderOptionsInit failed.\n");
    std::abort();
  }
  anim_config.minimize_size = minimize_size;
  anim_config.kmin = kmin_kmax.first;
  anim_config.kmax = kmin_kmax.second;
  anim_config.allow_mixed = allow_mixed;
  anim_config.verbose = 0;

  // For each frame.
  for (FrameConfig& frame_config : frame_configs) {
    if (!AddFrame(&enc, anim_config, &width, &height, timestamp_ms,
                  frame_config, &bit_pos)) {
      return;
    }

    timestamp_ms += frame_config.timestamp;
  }

  // Assemble.
  if (!WebPAnimEncoderAdd(enc, nullptr, timestamp_ms, nullptr)) {
    fprintf(stderr, "Last WebPAnimEncoderAdd failed: %s.\n",
            WebPAnimEncoderGetError(enc));
    WebPAnimEncoderDelete(enc);
    std::abort();
  }
  WebPData webp_data;
  WebPDataInit(&webp_data);
  // Tolerate failures when running under the nallocfuzz engine as allocations
  // during assembly may fail.
  if (!WebPAnimEncoderAssemble(enc, &webp_data) &&
      getenv("NALLOC_FUZZ_VERSION") == nullptr) {
    fprintf(stderr, "WebPAnimEncoderAssemble failed: %s.\n",
            WebPAnimEncoderGetError(enc));
    WebPAnimEncoderDelete(enc);
    WebPDataClear(&webp_data);
    std::abort();
  }

  WebPAnimEncoderDelete(enc);
  WebPDataClear(&webp_data);
}

}  // namespace

FUZZ_TEST(AnimIndexEncoder, AnimEncoderTest)
    .WithDomains(
        /*minimize_size=*/fuzztest::Arbitrary<bool>(), ArbitraryKMinKMax(),
        /*allow_mixed=*/fuzztest::Arbitrary<bool>(),
        fuzztest::VectorOf(fuzztest::StructOf<FrameConfig>(
                               fuzztest::InRange<int>(0, 1),
                               fuzztest::InRange<int>(0, 131073),
                               fuzz_utils::ArbitraryWebPConfig(),
                               fuzz_utils::ArbitraryCropOrScaleParams(),
                               fuzz_utils::ArbitraryWebPPictureFromIndex()))
            .WithMinSize(1)
            .WithMaxSize(15),
        /*optimization_index=*/
        fuzztest::InRange<uint32_t>(0, fuzz_utils::kMaxOptimizationIndex));

FUZZ_TEST(AnimArbitraryEncoder, AnimEncoderTest)
    .WithDomains(
        /*minimize_size=*/fuzztest::Arbitrary<bool>(), ArbitraryKMinKMax(),
        /*allow_mixed=*/fuzztest::Arbitrary<bool>(),
        fuzztest::VectorOf(fuzztest::StructOf<FrameConfig>(
                               fuzztest::InRange<int>(0, 1),
                               fuzztest::InRange<int>(0, 131073),
                               fuzz_utils::ArbitraryWebPConfig(),
                               fuzz_utils::ArbitraryCropOrScaleParams(),
                               fuzz_utils::ArbitraryWebPPicture()))
            .WithMinSize(1)
            .WithMaxSize(15),
        /*optimization_index=*/
        fuzztest::InRange<uint32_t>(0, fuzz_utils::kMaxOptimizationIndex));

TEST(AnimIndexEncoder, Buganizer498967191) {
  auto GetWebPPicture = [](int index,
                           bool use_argb) -> fuzz_utils::WebPPictureCpp {
    WebPPicture pic = fuzz_utils::GetSourcePicture(index, use_argb);
    return fuzz_utils::WebPPictureCpp(
        use_argb, pic.colorspace, pic.width, pic.height, pic.y, pic.u, pic.v,
        pic.y_stride, pic.uv_stride, pic.a, pic.a_stride, pic.argb,
        pic.argb_stride, pic.memory_, pic.memory_argb_);
  };
  AnimEncoderTest(
      false, {0, 1}, true,
      {FrameConfig{1, 0, WebPConfig{0,  0.f, 6, static_cast<WebPImageHint>(3),
                                    0,  0.f, 4, 0,
                                    38, 7,   1, 0,
                                    0,  0,   1, 10,
                                    1,  1,   1, 10,
                                    0,  0,   0, 20,
                                    1,  0,   0, 0,
                                    100},
                   fuzz_utils::CropOrScaleParams{true, true, 6, 1, 2, 1},
                   GetWebPPicture(0, true)},
       FrameConfig{0, 7248,
                   WebPConfig{1,   0.f, 1, static_cast<WebPImageHint>(3),
                              0,   0.f, 1, 0,
                              100, 0,   1, 0,
                              1,   0,   0, 10,
                              1,   1,   0, 10,
                              0,   1,   0, 0,
                              1,   0,   0, 0,
                              100},
                   fuzz_utils::CropOrScaleParams{true, true, 6, 8, 2, 1},
                   GetWebPPicture(0, true)}},
      1);
}

// 'kmax - kmin' used to overflow when kmax == 2 and kmin was close to INT_MIN,
// and WebPAnimEncoderNew() could then fail.
TEST(AnimIndexEncoder, ExtremeNegativeKmin) {
  constexpr int kSize = 16;
  constexpr int kNumFrames = 3;
  for (const int kmin : {INT_MIN, INT_MIN + 1, INT_MIN + 2, -100, 0}) {
    WebPAnimEncoderOptions options;
    ASSERT_TRUE(WebPAnimEncoderOptionsInit(&options));
    options.kmax = 2;
    options.kmin = kmin;
    const std::unique_ptr<WebPAnimEncoder, decltype(&WebPAnimEncoderDelete)>
        enc(WebPAnimEncoderNew(kSize, kSize, &options), WebPAnimEncoderDelete);
    ASSERT_NE(enc.get(), nullptr) << "kmin=" << kmin;
    WebPConfig config;
    ASSERT_TRUE(WebPConfigInit(&config));
    config.lossless = 1;
    config.method = 0;
    for (int i = 0; i < kNumFrames; ++i) {
      WebPPicture pic;
      ASSERT_TRUE(WebPPictureInit(&pic));
      pic.use_argb = 1;
      pic.width = kSize;
      pic.height = kSize;
      ASSERT_TRUE(WebPPictureAlloc(&pic));
      for (int p = 0; p < kSize * kSize; ++p) {
        pic.argb[(p / kSize) * pic.argb_stride + (p % kSize)] =
            0xff000000u | ((p * 2654435761u + i * 40503u) & 0xffffffu);
      }
      EXPECT_TRUE(WebPAnimEncoderAdd(enc.get(), &pic, i * 40, &config));
      WebPPictureFree(&pic);
    }
    EXPECT_TRUE(
        WebPAnimEncoderAdd(enc.get(), nullptr, kNumFrames * 40, nullptr));
    WebPData data;
    WebPDataInit(&data);
    EXPECT_TRUE(WebPAnimEncoderAssemble(enc.get(), &data));
    WebPDataClear(&data);
  }
}

// WebPAnimEncoderAssemble() flushes all frames: adding frames afterwards must
// be rejected instead of accessing the (now empty) frame cache.
TEST(AnimIndexEncoder, AddAfterAssemble) {
  constexpr int kSize = 16;
  const std::unique_ptr<WebPAnimEncoder, decltype(&WebPAnimEncoderDelete)> enc(
      WebPAnimEncoderNew(kSize, kSize, nullptr), WebPAnimEncoderDelete);
  ASSERT_NE(enc.get(), nullptr);
  WebPConfig config;
  ASSERT_TRUE(WebPConfigInit(&config));
  config.lossless = 1;
  config.method = 0;
  WebPPicture pic;
  ASSERT_TRUE(WebPPictureInit(&pic));
  pic.use_argb = 1;
  pic.width = kSize;
  pic.height = kSize;
  ASSERT_TRUE(WebPPictureAlloc(&pic));
  for (int i = 0; i < 2; ++i) {
    for (int p = 0; p < kSize * kSize; ++p) {
      pic.argb[(p / kSize) * pic.argb_stride + (p % kSize)] =
          0xff000000u | ((p * 2654435761u + i * 40503u) & 0xffffffu);
    }
    EXPECT_TRUE(WebPAnimEncoderAdd(enc.get(), &pic, i * 40, &config));
  }
  WebPData data;
  WebPDataInit(&data);
  ASSERT_TRUE(WebPAnimEncoderAssemble(enc.get(), &data));
  const std::vector<uint8_t> assembled(data.bytes, data.bytes + data.size);
  WebPDataClear(&data);

  EXPECT_FALSE(WebPAnimEncoderAdd(enc.get(), &pic, 80, &config));
  EXPECT_EQ(pic.error_code, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  EXPECT_FALSE(WebPAnimEncoderAdd(enc.get(), nullptr, 120, nullptr));
  EXPECT_STRNE(WebPAnimEncoderGetError(enc.get()), "");
  WebPPictureFree(&pic);

  // The already assembled animation is still available.
  ASSERT_TRUE(WebPAnimEncoderAssemble(enc.get(), &data));
  EXPECT_EQ(std::vector<uint8_t>(data.bytes, data.bytes + data.size),
            assembled);
  WebPDataClear(&data);
}

// A frame whose ARGB rows do not fit in 32-bit byte addressing must be
// rejected up front instead of being copied with an overflowing
// '4 * argb_stride' byte stride.
TEST(AnimIndexEncoder, AddRejectsUnboundedArgbStride) {
  constexpr int kSize = 64;
  const std::unique_ptr<WebPAnimEncoder, decltype(&WebPAnimEncoderDelete)> enc(
      WebPAnimEncoderNew(kSize, kSize, nullptr), WebPAnimEncoderDelete);
  ASSERT_NE(enc.get(), nullptr);
  WebPConfig config;
  ASSERT_TRUE(WebPConfigInit(&config));
  config.lossless = 1;
  config.method = 0;
  WebPPicture pic;
  ASSERT_TRUE(WebPPictureInit(&pic));
  pic.use_argb = 1;
  pic.width = kSize;
  pic.height = kSize;
  ASSERT_TRUE(WebPPictureAlloc(&pic));
  pic.argb_stride = 1 << 30;  // 4 * argb_stride overflows 'int'.
  EXPECT_FALSE(WebPAnimEncoderAdd(enc.get(), &pic, 0, &config));
  EXPECT_EQ(pic.error_code, VP8_ENC_ERROR_BAD_DIMENSION);
  EXPECT_STRNE(WebPAnimEncoderGetError(enc.get()), "");
  WebPPictureFree(&pic);
}

// RefineRect() must reject pictures whose geometry the encoder cannot
// address, before any pixel is touched. The buffers below are a few pixels
// wide on purpose: a case that passed validation would make the rectangle
// search read far past them.
TEST(AnimEncoderRefineRect, RejectsInvalidGeometry) {
  struct Case {
    int width;
    int height;
    int stride;
    bool null_buffer;
  };
  const Case cases[] = {
      {64, 64, (1 << 29) + 16, false},  // 4 * stride overflows 'int'.
      {64, 64, 63, false},              // Rows are closer than the width.
      {INT_MAX / 4 + 1, 1, 1, false},   // 4 * width overflows 'int'.
      {64, 64, 64, true},               // No buffer at all.
  };
  std::vector<uint32_t> pixels(16);
  for (const Case& c : cases) {
    WebPPicture prev, curr;
    ASSERT_TRUE(WebPPictureInit(&prev));
    ASSERT_TRUE(WebPPictureInit(&curr));
    prev.use_argb = curr.use_argb = 1;
    prev.width = curr.width = c.width;
    prev.height = curr.height = c.height;
    prev.argb_stride = curr.argb_stride = c.stride;
    prev.argb = curr.argb = c.null_buffer ? nullptr : pixels.data();
    int x = 0, y = 0, w = c.width, h = c.height;
    EXPECT_EQ(WebPAnimEncoderRefineRect(&prev, &curr, /*is_lossless=*/1, 70.f,
                                        &x, &y, &w, &h),
              0)
        << "width=" << c.width << " height=" << c.height
        << " stride=" << c.stride << " null_buffer=" << c.null_buffer;
  }
}

// Row padding is allowed: identical pixels must produce identical rectangles
// with and without padding.
TEST(AnimEncoderRefineRect, PaddedStride) {
  constexpr int kSize = 64;
  constexpr int kPaddedStride = 96;
  std::vector<uint32_t> prev_tight(kSize * kSize, 0xff102030u);
  std::vector<uint32_t> curr_tight = prev_tight;
  std::vector<uint32_t> prev_padded(kPaddedStride * kSize, 0u);
  std::vector<uint32_t> curr_padded(kPaddedStride * kSize, 0u);
  for (int y = 24; y < 32; ++y) {
    for (int x = 20; x < 28; ++x) {
      curr_tight[y * kSize + x] = 0xff405060u;
    }
  }
  for (int y = 0; y < kSize; ++y) {
    memcpy(&prev_padded[y * kPaddedStride], &prev_tight[y * kSize],
           kSize * sizeof(uint32_t));
    memcpy(&curr_padded[y * kPaddedStride], &curr_tight[y * kSize],
           kSize * sizeof(uint32_t));
  }
  WebPPicture prev_a, curr_a, prev_b, curr_b;
  ASSERT_TRUE(WebPPictureInit(&prev_a));
  ASSERT_TRUE(WebPPictureInit(&curr_a));
  ASSERT_TRUE(WebPPictureInit(&prev_b));
  ASSERT_TRUE(WebPPictureInit(&curr_b));
  prev_a.use_argb = curr_a.use_argb = 1;
  prev_b.use_argb = curr_b.use_argb = 1;
  prev_a.width = curr_a.width = prev_b.width = curr_b.width = kSize;
  prev_a.height = curr_a.height = prev_b.height = curr_b.height = kSize;
  prev_a.argb_stride = curr_a.argb_stride = kSize;
  prev_b.argb_stride = curr_b.argb_stride = kPaddedStride;
  prev_a.argb = prev_tight.data();
  curr_a.argb = curr_tight.data();
  prev_b.argb = prev_padded.data();
  curr_b.argb = curr_padded.data();
  int x_a = 0, y_a = 0, w_a = kSize, h_a = kSize;
  int x_b = 0, y_b = 0, w_b = kSize, h_b = kSize;
  ASSERT_EQ(WebPAnimEncoderRefineRect(&prev_a, &curr_a, /*is_lossless=*/1, 70.f,
                                      &x_a, &y_a, &w_a, &h_a),
            1);
  ASSERT_EQ(WebPAnimEncoderRefineRect(&prev_b, &curr_b, /*is_lossless=*/1, 70.f,
                                      &x_b, &y_b, &w_b, &h_b),
            1);
  EXPECT_EQ(x_a, x_b);
  EXPECT_EQ(y_a, y_b);
  EXPECT_EQ(w_a, w_b);
  EXPECT_EQ(h_a, h_b);
}
