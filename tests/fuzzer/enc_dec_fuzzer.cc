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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "./fuzz_utils.h"
#include "gtest/gtest.h"
#include "src/dsp/cpu.h"
#include "src/utils/rescaler_utils.h"
#include "src/utils/utils.h"
#include "webp/decode.h"
#include "webp/encode.h"

namespace {

const VP8CPUInfo default_VP8GetCPUInfo = fuzz_utils::VP8GetCPUInfo;

bool Enc(const fuzz_utils::CropOrScaleParams& crop_or_scale_params,
         WebPConfig& config, WebPPicture& pic,
         WebPMemoryWriter& memory_writer) {
  // Crop and scale.
  if (!fuzz_utils::CropOrScale(&pic, crop_or_scale_params)) {
    const WebPEncodingError error_code = pic.error_code;
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return false;
    fprintf(stderr, "ExtractAndCropOrScale failed. Error code: %d\n",
            error_code);
    std::abort();
  }

  // Skip slow settings on big images, it's likely to timeout.
  if (pic.width * pic.height > 32 * 32) {
    if (config.lossless) {
      if (config.quality > 99.0f && config.method >= 5) {
        config.quality = 99.0f;
        config.method = 5;
      }
    } else {
      if (config.quality > 99.0f && config.method == 6) {
        config.quality = 99.0f;
      }
    }
    if (config.alpha_quality == 100 && config.method == 6) {
      config.alpha_quality = 99;
    }
  }

  // Encode.
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &memory_writer;
  if (!WebPEncode(&config, &pic)) {
    const WebPEncodingError error_code = pic.error_code;
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY ||
        error_code == VP8_ENC_ERROR_BAD_WRITE) {
      return false;
    }
    fprintf(stderr, "WebPEncode failed. Error code: %d\n", error_code);
    std::abort();
  }
  return true;
}

// Encodes a picture and decodes it using valid colorspace and decoder options,
// verifying that decoding succeeds and produces consistent output.
void EncDecValidTest(bool use_argb, fuzz_utils::WebPPictureCpp pic_cpp,
                     WebPConfig config, int optimization_index,
                     const fuzz_utils::CropOrScaleParams& crop_or_scale_params,
                     int colorspace,
                     const fuzz_utils::WebPDecoderOptionsCpp& decoder_options) {
  fuzz_utils::SetOptimization(default_VP8GetCPUInfo, optimization_index);

  // Init the source picture.
  WebPPicture& pic = pic_cpp.ref();

  WebPMemoryWriter memory_writer;
  WebPMemoryWriterInit(&memory_writer);
  std::unique_ptr<WebPMemoryWriter, fuzz_utils::UniquePtrDeleter>
      memory_writer_owner(&memory_writer);

  if (!Enc(crop_or_scale_params, config, pic, memory_writer)) return;

  // Try decoding the result without options into BGRA first.
  const uint8_t* const out_data = memory_writer.mem;
  const size_t out_size = memory_writer.size;
  WebPDecoderConfig dec_config;
  std::unique_ptr<WebPDecoderConfig, fuzz_utils::UniquePtrDeleter>
      dec_config_owner(&dec_config);
  if (!WebPInitDecoderConfig(&dec_config)) {
    fprintf(stderr, "WebPInitDecoderConfig failed.\n");
    std::abort();
  }

  dec_config.output.colorspace = MODE_BGRA;
  VP8StatusCode status = WebPDecode(out_data, out_size, &dec_config);
  if ((status != VP8_STATUS_OK && status != VP8_STATUS_OUT_OF_MEMORY &&
       status != VP8_STATUS_USER_ABORT) ||
      (status == VP8_STATUS_OK && (dec_config.output.width != pic.width ||
                                   dec_config.output.height != pic.height))) {
    fprintf(stderr, "WebPDecode failed. status: %d.\n", status);
    std::abort();
  }

  // Compare the results if exact encoding.
  if (status == VP8_STATUS_OK && pic.use_argb && config.lossless &&
      config.near_lossless == 100) {
    const uint8_t* const rgba = dec_config.output.u.RGBA.rgba;
    const int w = dec_config.output.width;
    const int h = dec_config.output.height;

    const uint32_t* src1 = (const uint32_t*)rgba;
    const uint32_t* src2 = pic.argb;
    for (int y = 0; y < h; ++y, src1 += w, src2 += pic.argb_stride) {
      for (int x = 0; x < w; ++x) {
        uint32_t v1 = src1[x], v2 = src2[x];
        if (!config.exact) {
          if ((v1 & 0xff000000u) == 0 || (v2 & 0xff000000u) == 0) {
            // Only keep alpha for comparison of fully transparent area.
            v1 &= 0xff000000u;
            v2 &= 0xff000000u;
          }
        }
        if (v1 != v2) {
          fprintf(stderr, "Lossless compression failed pixel-exactness.\n");
          std::abort();
        }
      }
    }
  }

  // Use given decoding options. Skip if the crop rectangle exceeds the image
  // dimensions (which would validly fail with VP8_STATUS_INVALID_PARAM).
  if (static_cast<int64_t>(decoder_options.crop_left) +
              decoder_options.crop_width >
          static_cast<int64_t>(pic.width) ||
      static_cast<int64_t>(decoder_options.crop_top) +
              decoder_options.crop_height >
          static_cast<int64_t>(pic.height)) {
    return;
  }
  // Decode into a separate config so the uncropped BGRA output in dec_config
  // remains available for comparison below.
  WebPDecoderConfig cropped_dec_config;
  std::unique_ptr<WebPDecoderConfig, fuzz_utils::UniquePtrDeleter>
      cropped_dec_config_owner(&cropped_dec_config);
  if (!WebPInitDecoderConfig(&cropped_dec_config)) {
    fprintf(stderr, "WebPInitDecoderConfig failed.\n");
    abort();
  }

  cropped_dec_config.output.colorspace = static_cast<WEBP_CSP_MODE>(colorspace);
  std::memcpy(&cropped_dec_config.options, &decoder_options,
              sizeof(decoder_options));
  const VP8StatusCode cropped_status =
      WebPDecode(out_data, out_size, &cropped_dec_config);
  if (cropped_status != VP8_STATUS_OK &&
      cropped_status != VP8_STATUS_OUT_OF_MEMORY &&
      cropped_status != VP8_STATUS_USER_ABORT) {
    fprintf(stderr, "WebPDecode failed. status: %d.\n", cropped_status);
    abort();
  }

  // Verify that for all 8-bit alpha colorspaces, decoding with decoder_options
  // produces the exact same alpha values as the corresponding cropped (and
  // optionally flipped) sub-rect of the uncropped decode when scaling and alpha
  // dithering are disabled. RGB/YUV values are not compared because on lossy
  // images they can be affected by decoder_options (e.g. bypass_filtering,
  // no_fancy_upsampling, dithering_strength, or fancy YUV upsampling at crop
  // boundaries) and by premultiplication.
  if (status == VP8_STATUS_OK && cropped_status == VP8_STATUS_OK &&
      !decoder_options.use_scaling &&
      decoder_options.alpha_dithering_strength == 0 &&
      (colorspace == MODE_RGBA || colorspace == MODE_BGRA ||
       colorspace == MODE_ARGB || colorspace == MODE_rgbA ||
       colorspace == MODE_bgrA || colorspace == MODE_Argb ||
       colorspace == MODE_YUVA)) {
    // Lossy decoding snaps crop_left and crop_top to even coordinates.
    const int crop_left =
        decoder_options.use_cropping
            ? (config.lossless ? decoder_options.crop_left
                               : (decoder_options.crop_left & ~1))
            : 0;
    const int crop_top =
        decoder_options.use_cropping
            ? (config.lossless ? decoder_options.crop_top
                               : (decoder_options.crop_top & ~1))
            : 0;
    const int cw = cropped_dec_config.output.width;
    const int ch = cropped_dec_config.output.height;
    const uint8_t* const uncropped_bgra = dec_config.output.u.RGBA.rgba;
    const int uncropped_stride = dec_config.output.u.RGBA.stride;
    for (int y = 0; y < ch; ++y) {
      const int src_y = crop_top + (decoder_options.flip ? (ch - 1 - y) : y);
      const uint8_t* const src_row =
          uncropped_bgra + static_cast<ptrdiff_t>(src_y) * uncropped_stride;
      for (int x = 0; x < cw; ++x) {
        const uint8_t expected_a = src_row[4 * (crop_left + x) + 3];
        uint8_t actual_a = 0;
        if (colorspace == MODE_YUVA) {
          actual_a = cropped_dec_config.output.u.YUVA
                         .a[static_cast<ptrdiff_t>(y) *
                                cropped_dec_config.output.u.YUVA.a_stride +
                            x];
        } else {
          // Alpha is at byte offset 0 for MODE_ARGB/MODE_Argb and 3 for
          // MODE_RGBA/MODE_rgbA/MODE_BGRA/MODE_bgrA.
          const int alpha_offset =
              (colorspace == MODE_ARGB || colorspace == MODE_Argb) ? 0 : 3;
          actual_a = cropped_dec_config.output.u.RGBA
                         .rgba[static_cast<ptrdiff_t>(y) *
                                   cropped_dec_config.output.u.RGBA.stride +
                               4 * x + alpha_offset];
        }
        if (actual_a != expected_a) {
          fprintf(stderr,
                  "Cropped alpha mismatch at (%d, %d): got %u, expected %u.\n",
                  x, y, actual_a, expected_a);
          std::abort();
        }
      }
    }
  }

  // Verify that incremental decoding (via WebPIAppend or WebPIUpdate) of the
  // encoded bitstream with the same colorspace and decoder_options succeeds and
  // produces the exact same output as one-shot WebPDecode.
  if (cropped_status == VP8_STATUS_OK) {
    WebPDecoderConfig idec_config;
    std::unique_ptr<WebPDecoderConfig, fuzz_utils::UniquePtrDeleter>
        idec_config_owner(&idec_config);
    if (!WebPInitDecoderConfig(&idec_config)) {
      fprintf(stderr, "WebPInitDecoderConfig failed.\n");
      std::abort();
    }
    idec_config.output.colorspace = static_cast<WEBP_CSP_MODE>(colorspace);
    std::memcpy(&idec_config.options, &decoder_options,
                sizeof(decoder_options));

    // A simple single-chunk WebP file has:
    //   - bytes 0..11: 12-byte "RIFF" + file_size + "WEBP" container header,
    //   - bytes 12..15: 4-byte chunk tag ("VP8 " or "VP8L", i.e. no "VP8X"),
    //   - bytes 16..19: 32-bit little-endian chunk payload size (chunk_len),
    //   - bytes 20..20+chunk_len-1: raw VP8/VP8L compressed bitstream.
    // When the encoded output has this layout, use the low bits of fuzz inputs
    // (optimization_index and colorspace) to deterministically choose between
    // feeding the full RIFF container, a RIFF-less chunk, or a headerless raw
    // bitstream to the incremental decoder.
    const uint8_t* idec_data = out_data;
    size_t idec_size = out_size;
    if (out_size >= 32 && (std::memcmp(out_data + 12, "VP8 ", 4) == 0 ||
                           std::memcmp(out_data + 12, "VP8L", 4) == 0)) {
      const uint32_t chunk_len = out_data[16] | (uint32_t{out_data[17]} << 8) |
                                 (uint32_t{out_data[18]} << 16) |
                                 (uint32_t{out_data[19]} << 24);
      if (chunk_len >= 12 && 20u + chunk_len <= out_size) {
        if ((optimization_index & 1) != 0) {
          // Strip both the 12-byte RIFF header and 8-byte chunk header to test
          // headerless raw VP8/VP8L incremental decoding (where the total
          // bitstream size is unknown upfront).
          idec_data = out_data + 20;
          idec_size = chunk_len;
        } else if ((colorspace & 1) != 0) {
          // Strip only the 12-byte RIFF header, keeping the "VP8 "/"VP8L" chunk
          // header and its declared payload size.
          idec_data = out_data + 12;
          idec_size = out_size - 12;
        }
      }
    }

    WebPIDecoder* const idec = WebPIDecode(nullptr, 0, &idec_config);
    if (idec != nullptr) {
      // Start with a small chunk size in [1..64] so early calls exercise
      // VP8_STATUS_SUSPENDED across header and partition/entropy parsing, then
      // double the chunk size at each step so large images finish quickly.
      // Alternate between WebPIUpdate (passing the growing prefix [0..avail))
      // and WebPIAppend (passing each new slice [pos..pos+n)) based on
      // out_size.
      VP8StatusCode idec_status = VP8_STATUS_SUSPENDED;
      size_t step = 1 + (out_size % 64);
      if ((out_size & 2) != 0) {
        size_t avail = step;
        while (true) {
          if (avail > idec_size) avail = idec_size;
          idec_status = WebPIUpdate(idec, idec_data, avail);
          if (idec_status != VP8_STATUS_SUSPENDED || avail == idec_size) break;
          avail *= 2;
        }
      } else {
        size_t pos = 0;
        while (pos < idec_size && idec_status == VP8_STATUS_SUSPENDED) {
          const size_t n = std::min<size_t>(step, idec_size - pos);
          idec_status = WebPIAppend(idec, idec_data + pos, n);
          pos += n;
          step *= 2;
        }
      }
      WebPIDelete(idec);

      // Since the bitstream is valid and one-shot WebPDecode succeeded,
      // incremental decoding must also succeed (unless an allocation limit is
      // hit).
      if (idec_status != VP8_STATUS_OK &&
          idec_status != VP8_STATUS_OUT_OF_MEMORY &&
          idec_status != VP8_STATUS_USER_ABORT) {
        fprintf(stderr, "Incremental decode failed. status: %d.\n",
                idec_status);
        std::abort();
      }
      // Verify that the incrementally decoded pixels match the one-shot
      // decoded buffer (cropped_dec_config.output) byte-for-byte across RGB or
      // YUVA planes.
      if (idec_status == VP8_STATUS_OK) {
        const WebPDecBuffer& a = idec_config.output;
        const WebPDecBuffer& b = cropped_dec_config.output;
        bool match = (a.colorspace == b.colorspace && a.width == b.width &&
                      a.height == b.height);
        if (match) {
          if (WebPIsRGBMode(a.colorspace)) {
            match =
                (a.u.RGBA.size == b.u.RGBA.size &&
                 std::memcmp(a.u.RGBA.rgba, b.u.RGBA.rgba, a.u.RGBA.size) == 0);
          } else {
            match =
                (a.u.YUVA.y_size == b.u.YUVA.y_size &&
                 a.u.YUVA.u_size == b.u.YUVA.u_size &&
                 a.u.YUVA.v_size == b.u.YUVA.v_size &&
                 std::memcmp(a.u.YUVA.y, b.u.YUVA.y, a.u.YUVA.y_size) == 0 &&
                 std::memcmp(a.u.YUVA.u, b.u.YUVA.u, a.u.YUVA.u_size) == 0 &&
                 std::memcmp(a.u.YUVA.v, b.u.YUVA.v, a.u.YUVA.v_size) == 0);
            if (match && (a.u.YUVA.a != nullptr || b.u.YUVA.a != nullptr)) {
              match =
                  (a.u.YUVA.a != nullptr && b.u.YUVA.a != nullptr &&
                   a.u.YUVA.a_size == b.u.YUVA.a_size &&
                   std::memcmp(a.u.YUVA.a, b.u.YUVA.a, a.u.YUVA.a_size) == 0);
            }
          }
        }
        if (!match) {
          fprintf(stderr, "Incremental decode output mismatch.\n");
          std::abort();
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void EncDecTest(bool use_argb, fuzz_utils::WebPPictureCpp pic_cpp,
                WebPConfig config, int optimization_index,
                const fuzz_utils::CropOrScaleParams& crop_or_scale_params,
                int colorspace,
                const fuzz_utils::WebPDecoderOptionsCpp& decoder_options) {
  fuzz_utils::SetOptimization(default_VP8GetCPUInfo, optimization_index);

  // Init the source picture.
  WebPPicture& pic = pic_cpp.ref();
  WebPMemoryWriter memory_writer;
  WebPMemoryWriterInit(&memory_writer);
  std::unique_ptr<WebPMemoryWriter, fuzz_utils::UniquePtrDeleter>
      memory_writer_owner(&memory_writer);

  if (!Enc(crop_or_scale_params, config, pic, memory_writer)) return;

  // Try decoding the result.
  const uint8_t* const out_data = memory_writer.mem;
  const size_t out_size = memory_writer.size;
  WebPDecoderConfig dec_config;
  std::unique_ptr<WebPDecoderConfig, fuzz_utils::UniquePtrDeleter>
      dec_config_owner(&dec_config);
  if (!WebPInitDecoderConfig(&dec_config)) {
    fprintf(stderr, "WebPInitDecoderConfig failed.\n");
    abort();
  }
  if (decoder_options.use_scaling) {
    int scaled_width = decoder_options.scaled_width;
    int scaled_height = decoder_options.scaled_height;
    if (!WebPRescalerGetScaledDimensions(pic.width, pic.height, &scaled_width,
                                         &scaled_height)) {
      // Rescaled dimensions do not make sense.
      return;
    }
    if (static_cast<uint64_t>(scaled_width) * scaled_height > 1000u * 1000u) {
      // Skip huge scaling.
      return;
    }
  }

  dec_config.output.colorspace = static_cast<WEBP_CSP_MODE>(colorspace);
  std::memcpy(&dec_config.options, &decoder_options, sizeof(decoder_options));
  const VP8StatusCode status = WebPDecode(out_data, out_size, &dec_config);
  if (status != VP8_STATUS_OK && status != VP8_STATUS_OUT_OF_MEMORY &&
      status != VP8_STATUS_USER_ABORT && status != VP8_STATUS_INVALID_PARAM) {
    fprintf(stderr, "WebPDecode failed. status: %d.\n", status);
    abort();
  }
}

TEST(EncIndexDec, Buganizer498966814) {
  const bool use_argb = false;
  WebPPicture pic = fuzz_utils::GetSourcePicture(/*image_index=*/2, use_argb);
  EncDecTest(false,
             fuzz_utils::WebPPictureCpp{
                 use_argb, pic.colorspace, pic.width, pic.height, pic.y, pic.u,
                 pic.v, pic.y_stride, pic.uv_stride, pic.a, pic.a_stride,
                 pic.argb, pic.argb_stride, pic.memory_, pic.memory_argb_},
             WebPConfig{1,  78.f, 6,  static_cast<WebPImageHint>(3),
                        0,  0.f,  3,  93,
                        34, 3,    0,  1,
                        0,  0,    79, 3,
                        1,  0,    0,  30,
                        1,  0,    1,  80,
                        0,  0,    1,  0,
                        100},
             1, fuzz_utils::CropOrScaleParams{true, true, 2, 1, 6, 3}, 9,
             fuzz_utils::WebPDecoderOptionsCpp{1044690037,
                                               -814076429,
                                               0,
                                               503925603,
                                               -129135778,
                                               1416295626,
                                               0,
                                               488395839,
                                               524288,
                                               1,
                                               1573917569,
                                               487414467,
                                               1639447831,
                                               1696032481,
                                               {0, 0, 0, 0, 0}});
}

TEST(RescalerUtils, Needs64BitExtremeDownscale) {
  // 16383x16383 -> 1x1: a huge number of source rows (16383) accumulate into
  // a single output row, each contributing up to 255 * src_width. Overflows
  // a 32-bit accumulator; see WebPRescalerNeeds64Bit().
  EXPECT_TRUE(WebPRescalerNeeds64Bit(16383, 16383, 1, 1));
}

TEST(RescalerUtils, Needs64BitExtremeUpscale) {
  // 64x128 -> 524288x1: horizontal upscale makes each row contribute up to
  // 255 * (dst_width - 1), and 128 rows accumulate into the single output
  // row (dst_height == 1). Also overflows a 32-bit accumulator.
  EXPECT_TRUE(WebPRescalerNeeds64Bit(64, 128, 524288, 1));
}

}  // namespace

FUZZ_TEST(EncIndexDec, EncDecValidTest)
    .WithDomains(/*use_argb=*/fuzztest::Arbitrary<bool>(),
                 fuzz_utils::ArbitraryWebPPictureFromIndex(),
                 fuzz_utils::ArbitraryWebPConfig(),
                 /*optimization_index=*/
                 fuzztest::InRange<uint32_t>(0,
                                             fuzz_utils::kMaxOptimizationIndex),
                 fuzz_utils::ArbitraryCropOrScaleParams(),
                 /*colorspace=*/fuzztest::InRange<int>(0, MODE_LAST - 1),
                 fuzz_utils::ArbitraryValidWebPDecoderOptions());

FUZZ_TEST(EncArbitraryDec, EncDecValidTest)
    .WithDomains(/*use_argb=*/fuzztest::Arbitrary<bool>(),
                 fuzz_utils::ArbitraryWebPPicture(),
                 fuzz_utils::ArbitraryWebPConfig(),
                 /*optimization_index=*/
                 fuzztest::InRange<uint32_t>(0,
                                             fuzz_utils::kMaxOptimizationIndex),
                 fuzz_utils::ArbitraryCropOrScaleParams(),
                 /*colorspace=*/fuzztest::InRange<int>(0, MODE_LAST - 1),
                 fuzz_utils::ArbitraryValidWebPDecoderOptions());

FUZZ_TEST(EncIndexDec, EncDecTest)
    .WithDomains(/*use_argb=*/fuzztest::Arbitrary<bool>(),
                 fuzz_utils::ArbitraryWebPPictureFromIndex(),
                 fuzz_utils::ArbitraryWebPConfig(),
                 /*optimization_index=*/
                 fuzztest::InRange<uint32_t>(0,
                                             fuzz_utils::kMaxOptimizationIndex),
                 fuzz_utils::ArbitraryCropOrScaleParams(),
                 /*colorspace=*/fuzztest::Arbitrary<int>(),
                 fuzz_utils::ArbitraryWebPDecoderOptions());

FUZZ_TEST(EncArbitraryDec, EncDecTest)
    .WithDomains(/*use_argb=*/fuzztest::Arbitrary<bool>(),
                 fuzz_utils::ArbitraryWebPPicture(),
                 fuzz_utils::ArbitraryWebPConfig(),
                 /*optimization_index=*/
                 fuzztest::InRange<uint32_t>(0,
                                             fuzz_utils::kMaxOptimizationIndex),
                 fuzz_utils::ArbitraryCropOrScaleParams(),
                 /*colorspace=*/fuzztest::Arbitrary<int>(),
                 fuzz_utils::ArbitraryWebPDecoderOptions());

// WebPPictureImportRGBA() documents no alignment requirement for 'rgba' or
// 'rgba_stride', so the input must not be read through a uint32_t*.
TEST(Enc, ImportRGBAUnalignedInput) {
  constexpr int kWidth = 37;  // More than 16 pixels, to also hit SIMD loops.
  constexpr int kHeight = 4;
  for (int offset = 0; offset < 4; ++offset) {
    for (int pad = 0; pad < 4; ++pad) {
      const int stride = 4 * kWidth + pad;
      // Exactly-sized buffer, so that out-of-bounds reads are detected.
      std::vector<uint8_t> buf(offset + stride * (kHeight - 1) + 4 * kWidth);
      for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = static_cast<uint8_t>(i * 37 + 11);
      }
      const uint8_t* const rgba = buf.data() + offset;
      WebPPicture pic;
      ASSERT_TRUE(WebPPictureInit(&pic));
      pic.use_argb = 1;
      pic.width = kWidth;
      pic.height = kHeight;
      // On failure pic.argb is left null: free the picture and skip the
      // pixel comparison below.
      const bool imported = WebPPictureImportRGBA(&pic, rgba, stride);
      EXPECT_TRUE(imported);
      if (!imported) {
        WebPPictureFree(&pic);
        continue;
      }
      for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
          const uint8_t* const p = rgba + y * stride + 4 * x;
          const uint32_t expected = (uint32_t{p[3]} << 24) |
                                    (uint32_t{p[0]} << 16) |
                                    (uint32_t{p[1]} << 8) | uint32_t{p[2]};
          EXPECT_EQ(pic.argb[y * pic.argb_stride + x], expected)
              << "offset=" << offset << " pad=" << pad << " x=" << x
              << " y=" << y;
        }
      }
      WebPPictureFree(&pic);
    }
  }
}
