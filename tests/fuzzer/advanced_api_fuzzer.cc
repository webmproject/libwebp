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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "./fuzz_utils.h"
#include "gtest/gtest.h"
#include "src/dec/webpi_dec.h"
#include "src/utils/rescaler_utils.h"
#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/types.h"

namespace {

void AdvancedApiTest(std::string_view blob, uint8_t factor_u8, int colorspace,
                     bool incremental,
                     const fuzz_utils::WebPDecoderOptionsCpp& decoder_options) {
  WebPDecoderConfig config;
  if (!WebPInitDecoderConfig(&config)) return;
  const uint8_t* const data = reinterpret_cast<const uint8_t*>(blob.data());
  const size_t size = blob.size();
  if (WebPGetFeatures(data, size, &config.input) != VP8_STATUS_OK) return;
  if ((size_t)config.input.width * config.input.height >
      fuzz_utils::kFuzzPxLimit) {
    return;
  }

  // Using two independent criteria ensures that all combinations of options
  // can reach each path at the decoding stage, with meaningful differences.

  const uint8_t value = fuzz_utils::FuzzHash(data, size);
  const float factor = factor_u8 / 255.f;  // 0-1

  std::memcpy(&config.options, &decoder_options, sizeof(decoder_options));
  if (config.options.use_cropping) {
    config.options.crop_width = (int)(config.input.width * (1 - factor));
    config.options.crop_height = (int)(config.input.height * (1 - factor));
    config.options.crop_left = config.input.width - config.options.crop_width;
    config.options.crop_top = config.input.height - config.options.crop_height;
  }
  if (config.options.use_scaling) {
    config.options.scaled_width = (int)(config.input.width * factor * 2);
    config.options.scaled_height = (int)(config.input.height * factor * 2);
  }
  config.output.colorspace = static_cast<WEBP_CSP_MODE>(colorspace);

  for (int i = 0; i < 2; ++i) {
    if (i == 1) {
      // Use the bitstream data to generate extreme ranges for the options. An
      // alternative approach would be to use a custom corpus containing webp
      // files prepended with sizeof(config.options) zeroes to allow the fuzzer
      // to modify these independently.
      const int data_offset = 50;
      if (data_offset + sizeof(config.options) >= size) break;
      memcpy(&config.options, data + data_offset, sizeof(config.options));

      // Skip easily avoidable out-of-memory fuzzing errors.
      if (config.options.use_scaling) {
        int input_width = config.input.width;
        int input_height = config.input.height;
        if (config.options.use_cropping) {
          const int cw = config.options.crop_width;
          const int ch = config.options.crop_height;
          const int x = config.options.crop_left & ~1;
          const int y = config.options.crop_top & ~1;
          if (WebPCheckCropDimensions(input_width, input_height, x, y, cw,
                                      ch)) {
            input_width = cw;
            input_height = ch;
          }
        }

        int scaled_width = config.options.scaled_width;
        int scaled_height = config.options.scaled_height;
        if (WebPRescalerGetScaledDimensions(input_width, input_height,
                                            &scaled_width, &scaled_height)) {
          size_t fuzz_px_limit = fuzz_utils::kFuzzPxLimit;
          if (scaled_width != config.input.width ||
              scaled_height != config.input.height) {
            // Using the WebPRescalerImport internally can significantly slow
            // down the execution. Avoid timeouts due to that.
            fuzz_px_limit /= 2;
          }
          // A big output canvas can lead to out-of-memory and timeout issues,
          // but a big internal working buffer can too. Also, rescaling from a
          // very wide input image to a very tall canvas can be as slow as
          // decoding a huge number of pixels. Avoid timeouts due to these.
          const uint64_t max_num_operations =
              (uint64_t)std::max(scaled_width, config.input.width) *
              std::max(scaled_height, config.input.height);
          if (max_num_operations > fuzz_px_limit) {
            break;
          }
        }
      }
    }
    if (incremental) {
      // Decodes incrementally in chunks of increasing size.
      WebPIDecoder* idec = WebPIDecode(NULL, 0, &config);
      if (!idec) return;
      VP8StatusCode status;
      if (size & 8) {
        size_t available_size = value + 1;
        while (1) {
          if (available_size > size) available_size = size;
          status = WebPIUpdate(idec, data, available_size);
          if (status != VP8_STATUS_SUSPENDED || available_size == size) break;
          available_size *= 2;
        }
      } else {
        // WebPIAppend expects new data and its size with each call.
        // Implemented here by simply advancing the pointer into data.
        const uint8_t* new_data = data;
        size_t new_size = value + 1;
        while (1) {
          if (new_data + new_size > data + size) {
            new_size = data + size - new_data;
          }
          status = WebPIAppend(idec, new_data, new_size);
          if (status != VP8_STATUS_SUSPENDED || new_size == 0) break;
          new_data += new_size;
          new_size *= 2;
        }
      }
      WebPIDelete(idec);
    } else {
      (void)WebPDecode(data, size, &config);
    }

    WebPFreeDecBuffer(&config.output);
  }
}

}  // namespace

FUZZ_TEST(AdvancedApi, AdvancedApiTest)
    .WithDomains(fuzztest::String().WithMaxSize(fuzz_utils::kMaxWebPFileSize +
                                                1),
                 /*factor_u8=*/fuzztest::Arbitrary<uint8_t>(),
#if defined(WEBP_REDUCE_CSP)
                 fuzztest::ElementOf<int>({static_cast<int>(MODE_RGBA),
                                           static_cast<int>(MODE_BGRA),
                                           static_cast<int>(MODE_rgbA),
                                           static_cast<int>(MODE_bgrA)}),
#else
                 fuzztest::InRange<int>(0, static_cast<int>(MODE_LAST) - 1),
#endif
                 /*incremental=*/fuzztest::Arbitrary<bool>(),
                 fuzz_utils::ArbitraryValidWebPDecoderOptions());

TEST(AdvancedApi, IncrementalExternalYUVABufferReplacementWithScaling) {
  constexpr int kWidth = 256;
  constexpr int kHeight = 256;
  constexpr int kOutWidth = 128;
  constexpr int kOutHeight = 128;
  constexpr int kUVWidth = (kOutWidth + 1) / 2;
  constexpr int kUVHeight = (kOutHeight + 1) / 2;

  // Build a deterministic RGBA image with non-trivial alpha.
  std::vector<uint8_t> rgba(kWidth * kHeight * 4);
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      const size_t i = 4u * (y * kWidth + x);
      rgba[i + 0] = static_cast<uint8_t>(x * 13 + y * 7 + (x ^ y));
      rgba[i + 1] = static_cast<uint8_t>(x * 3 + y * 17);
      rgba[i + 2] = static_cast<uint8_t>((x ^ (y * 5)) + x * 11);
      rgba[i + 3] = static_cast<uint8_t>(x * 5 + y * 9);
    }
  }

  uint8_t* encoded = nullptr;
  const size_t encoded_size =
      WebPEncodeRGBA(rgba.data(), kWidth, kHeight, kWidth * 4, 75.f, &encoded);
  ASSERT_NE(encoded, nullptr);
  ASSERT_GT(encoded_size, 0u);

  WebPDecoderConfig config;
  ASSERT_TRUE(WebPInitDecoderConfig(&config));
  ASSERT_EQ(WebPGetFeatures(encoded, encoded_size, &config.input),
            VP8_STATUS_OK);

  config.options.use_scaling = 1;
  config.options.scaled_width = kOutWidth;
  config.options.scaled_height = kOutHeight;
  config.output.colorspace = MODE_YUVA;
  config.output.is_external_memory = 1;

  // A uses tight strides.
  constexpr int kAYStride = kOutWidth;
  constexpr int kAUVStride = kUVWidth;
  constexpr int kAAStride = kOutWidth;

  std::vector<uint8_t> ay(kAYStride * kOutHeight, 0x11);
  std::vector<uint8_t> au(kAUVStride * kUVHeight, 0x22);
  std::vector<uint8_t> av(kAUVStride * kUVHeight, 0x33);
  std::vector<uint8_t> aa(kAAStride * kOutHeight, 0x44);

  // B deliberately uses different, padded strides. Pointer and stride changes
  // are both permitted for external buffers between incremental decode calls.
  constexpr int kBYStride = kOutWidth + 7;
  constexpr int kBUStride = kUVWidth + 5;
  constexpr int kBVStride = kUVWidth + 9;
  constexpr int kBAStride = kOutWidth + 11;

  std::vector<uint8_t> by(kBYStride * kOutHeight, 0xa1);
  std::vector<uint8_t> bu(kBUStride * kUVHeight, 0xa2);
  std::vector<uint8_t> bv(kBVStride * kUVHeight, 0xa3);
  std::vector<uint8_t> ba(kBAStride * kOutHeight, 0xa4);

  const auto bind = [&config](std::vector<uint8_t>& y, int y_stride,
                              std::vector<uint8_t>& u, int u_stride,
                              std::vector<uint8_t>& v, int v_stride,
                              std::vector<uint8_t>& a, int a_stride) {
    WebPYUVABuffer* const out = &config.output.u.YUVA;

    out->y = y.data();
    out->y_stride = y_stride;
    out->y_size = y.size();

    out->u = u.data();
    out->u_stride = u_stride;
    out->u_size = u.size();

    out->v = v.data();
    out->v_stride = v_stride;
    out->v_size = v.size();

    out->a = a.data();
    out->a_stride = a_stride;
    out->a_size = a.size();
  };

  bind(ay, kAYStride, au, kAUVStride, av, kAUVStride, aa, kAAStride);
  ASSERT_TRUE(WebPValidateDecoderConfig(&config));

  WebPIDecoder* const idec = WebPIDecode(nullptr, 0, &config);
  ASSERT_NE(idec, nullptr);

  bool switched = false;
  size_t offset = 0;
  VP8StatusCode status = VP8_STATUS_SUSPENDED;

  std::vector<uint8_t> ay_at_switch;
  std::vector<uint8_t> au_at_switch;
  std::vector<uint8_t> av_at_switch;
  std::vector<uint8_t> aa_at_switch;

  while (offset < encoded_size && status == VP8_STATUS_SUSPENDED) {
    const size_t chunk = std::min<size_t>(64, encoded_size - offset);

    status = WebPIAppend(idec, encoded + offset, chunk);
    offset += chunk;

    ASSERT_TRUE(status == VP8_STATUS_OK || status == VP8_STATUS_SUSPENDED);

    if (!switched && status == VP8_STATUS_SUSPENDED) {
      int last_y = -1;
      int width = 0, height = 0;
      int y_stride = 0, uv_stride = 0, a_stride = 0;
      uint8_t *u = nullptr, *v = nullptr, *a = nullptr;

      uint8_t* const y =
          WebPIDecGetYUVA(idec, &last_y, &u, &v, &a, &width, &height, &y_stride,
                          &uv_stride, &a_stride);

      // Wait until rescaled output has actually started, so the rescalers
      // already contain their initially configured destinations.
      if (y != nullptr && last_y > 0) {
        ay_at_switch = ay;
        au_at_switch = au;
        av_at_switch = av;
        aa_at_switch = aa;

        bind(by, kBYStride, bu, kBUStride, bv, kBVStride, ba, kBAStride);
        switched = true;
      }
    }
  }

  EXPECT_TRUE(switched);
  EXPECT_EQ(status, VP8_STATUS_OK);

  if (switched) {
    // Once config.output points at B, no later decoder output may be written
    // through the stale destinations originally captured from A.
    EXPECT_EQ(ay, ay_at_switch);
    EXPECT_EQ(au, au_at_switch);
    EXPECT_EQ(av, av_at_switch);
    EXPECT_EQ(aa, aa_at_switch);

    // Confirm that decoding actually continued into the replacement buffer.
    EXPECT_TRUE(
        std::any_of(by.begin(), by.end(), [](uint8_t v) { return v != 0xa1; }));
    EXPECT_TRUE(
        std::any_of(ba.begin(), ba.end(), [](uint8_t v) { return v != 0xa4; }));
  }

  WebPIDelete(idec);
  WebPFree(encoded);
}

TEST(AdvancedApi, Buganizer498966235) {
  AdvancedApiTest(
      std::string(
          "RIFF\014|"
          "\000\000WEBPVP8X\n\000\000\000\020\000\000D\002\000\000\017\000\000A"
          "LPH5\000\000\000\004\327\000\000\000\000\000\000c8\345S\000\243\000"
          "\253c\311\000\027\000\000\000\200\000\000\000\000\240\"AE\001\000"
          "\000\0008<"
          "ALP\010\000s\002\000\000\000\000\000\000\000\000\000ALPH\000\000\000"
          "\000VP8 "
          "(\000\000\000\224\001\000\235\001*\003\000\020\000\003,\000~"
          "\342\000\000se\002ionR\265Vq\302M}\"webp\"r\010\003\000\020#"
          "\366\356\002\323\220\000 "
          "\212N@\000\026\327A\367\266\201\201\"IFF@\"RIFF\"&\226!"
          "VP\n8Rg\000\0001\"\335\"I\"XEBP\"\002\002\"\367\\x0\203\203\203\341"
          "\341l,\203\\sectiqncJUN=\"sectistre\\x9D\\x01\\x2A\"JUKQ\"",
          257),
      68, 3, true,
      fuzz_utils::WebPDecoderOptionsCpp{
          0, 0, 1, 5, 10, 5, 9, 0, 1, 3, 0, 72, 0, 83, {0, 0, 0, 0, 0}});
}
