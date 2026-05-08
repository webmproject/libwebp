// Copyright 2026 Google Inc.
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

// Targets API surfaces beyond the basic decode-and-iterate path covered by
// the existing fuzzers in this directory:
//   - WebPPictureView / Crop / Rescale / Import* (picture manipulation)
//   - WebPDecode with is_external_memory=1 (caller-supplied output buffer)
//   - SharpYuvConvert (libsharpyuv standalone, all bit-depth combinations)

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

#include "fuzztest/fuzztest.h"
#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"
#include "webp/decode.h"
#include "webp/encode.h"

namespace {

constexpr int kMaxDim = 256;
constexpr int kMinDim = 1;
constexpr size_t kMaxOutputBytes = 16 * 1024 * 1024;

int ClampDim(uint32_t v) {
  return static_cast<int>(v % (kMaxDim - kMinDim + 1)) + kMinDim;
}

uint32_t ReadU32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 0) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

// Build a WebPPicture from raw bytes via Import. Picks dimensions, fills
// channels by tiling the supplied bytes.
int InitPictureFromBytes(WebPPicture* pic, const uint8_t* data, size_t size,
                         int channels, int width, int height) {
  pic->width = width;
  pic->height = height;
  pic->use_argb = 1;
  if (!WebPPictureAlloc(pic)) return 0;

  size_t row_bytes = static_cast<size_t>(width) * channels;
  std::vector<uint8_t> src(static_cast<size_t>(height) * row_bytes);
  if (size == 0) {
    std::memset(src.data(), 0, src.size());
  } else {
    for (size_t i = 0; i < src.size(); i++) {
      src[i] = data[i % size];
    }
  }

  int ok = (channels == 3) ? WebPPictureImportRGB(pic, src.data(),
                                                  static_cast<int>(row_bytes))
                           : WebPPictureImportRGBA(pic, src.data(),
                                                   static_cast<int>(row_bytes));
  if (!ok) {
    WebPPictureFree(pic);
    return 0;
  }
  return 1;
}

// --- WebPPictureView: sub-rectangle view + encode ---
void PictureViewTest(std::string_view blob) {
  if (blob.size() < 12) return;
  const auto* d = reinterpret_cast<const uint8_t*>(blob.data());
  const size_t size = blob.size();

  int w = ClampDim(ReadU32(d));
  int h = ClampDim(ReadU32(d + 4));
  int left = static_cast<int>(ReadU32(d + 8) % static_cast<uint32_t>(w));
  int top = (size > 8) ? d[8] % h : 0;
  int vw = (size > 11) ? ClampDim((d[9] << 24) | (d[10] << 16) | d[11]) : 1;
  int vh = (size > 11) ? ClampDim(d[11] * 7 + 3) : 1;
  if (left + vw > w) vw = w - left;
  if (top + vh > h) vh = h - top;
  if (vw <= 0 || vh <= 0) return;

  WebPPicture src;
  if (!WebPPictureInit(&src)) return;
  if (!InitPictureFromBytes(&src, d + 12, size - 12, 4, w, h)) return;

  WebPPicture view;
  if (!WebPPictureInit(&view)) {
    WebPPictureFree(&src);
    return;
  }
  if (WebPPictureView(&src, left, top, vw, vh, &view)) {
    WebPConfig config;
    if (WebPConfigInit(&config)) {
      config.quality = 50;
      config.method = 0;
      WebPMemoryWriter writer;
      WebPMemoryWriterInit(&writer);
      view.writer = WebPMemoryWrite;
      view.custom_ptr = &writer;
      (void)WebPEncode(&config, &view);
      WebPMemoryWriterClear(&writer);
    }
  }
  WebPPictureFree(&view);
  WebPPictureFree(&src);
}
FUZZ_TEST(PictureGaps, PictureViewTest).WithDomains(fuzztest::String());

// --- WebPPictureCrop: in-place crop ---
void PictureCropTest(std::string_view blob) {
  if (blob.size() < 16) return;
  const auto* d = reinterpret_cast<const uint8_t*>(blob.data());
  const size_t size = blob.size();

  int w = ClampDim(ReadU32(d));
  int h = ClampDim(ReadU32(d + 4));
  int left = ClampDim(d[8]) - 1;
  int top = ClampDim(d[9]) - 1;
  int cw = ClampDim(ReadU32(d + 10));
  int ch = ClampDim((d[14] << 8) | d[15]);

  WebPPicture pic;
  if (!WebPPictureInit(&pic)) return;
  if (!InitPictureFromBytes(&pic, d + 16, size - 16, 4, w, h)) return;
  (void)WebPPictureCrop(&pic, left, top, cw, ch);
  WebPPictureFree(&pic);
}
FUZZ_TEST(PictureGaps, PictureCropTest).WithDomains(fuzztest::String());

// --- WebPPictureRescale: arbitrary target dimensions ---
void PictureRescaleTest(std::string_view blob) {
  if (blob.size() < 16) return;
  const auto* d = reinterpret_cast<const uint8_t*>(blob.data());
  const size_t size = blob.size();

  int w = ClampDim(ReadU32(d));
  int h = ClampDim(ReadU32(d + 4));
  int dst_w = ClampDim(ReadU32(d + 8));
  int dst_h = ClampDim(ReadU32(d + 12));

  WebPPicture pic;
  if (!WebPPictureInit(&pic)) return;
  if (!InitPictureFromBytes(&pic, d + 16, size - 16, 4, w, h)) return;
  (void)WebPPictureRescale(&pic, dst_w, dst_h);
  WebPPictureFree(&pic);
}
FUZZ_TEST(PictureGaps, PictureRescaleTest).WithDomains(fuzztest::String());

// --- WebPPictureImport*: caller-supplied stride ---
void PictureImportTest(std::string_view blob) {
  if (blob.size() < 12) return;
  const auto* d = reinterpret_cast<const uint8_t*>(blob.data());
  const size_t size = blob.size();

  int w = ClampDim(ReadU32(d));
  int h = ClampDim(ReadU32(d + 4));
  uint8_t variant = d[8] % 6;
  int channels = (variant == 0 || variant == 3) ? 3 : 4;
  int stride_bonus = d[9] % 65;
  int stride = w * channels + stride_bonus;

  size_t needed = static_cast<size_t>(h) * stride;
  std::vector<uint8_t> src(needed);
  if (size > 12) {
    for (size_t i = 0; i < needed; i++) {
      src[i] = d[12 + (i % (size - 12))];
    }
  }

  WebPPicture pic;
  if (!WebPPictureInit(&pic)) return;
  pic.width = w;
  pic.height = h;
  pic.use_argb = 1;

  switch (variant) {
    case 0: (void)WebPPictureImportRGB(&pic, src.data(), stride); break;
    case 1: (void)WebPPictureImportRGBA(&pic, src.data(), stride); break;
    case 2: (void)WebPPictureImportRGBX(&pic, src.data(), stride); break;
    case 3: (void)WebPPictureImportBGR(&pic, src.data(), stride); break;
    case 4: (void)WebPPictureImportBGRA(&pic, src.data(), stride); break;
    case 5: (void)WebPPictureImportBGRX(&pic, src.data(), stride); break;
  }
  WebPPictureFree(&pic);
}
FUZZ_TEST(PictureGaps, PictureImportTest).WithDomains(fuzztest::String());

// --- WebPDecode with is_external_memory=1: caller-supplied output buffer
//     and stride ---
void DecodeExternalMemoryTest(std::string_view blob) {
  if (blob.size() < 8) return;
  const auto* d = reinterpret_cast<const uint8_t*>(blob.data());
  const size_t size = blob.size();

  WebPDecoderConfig config;
  if (!WebPInitDecoderConfig(&config)) return;
  if (WebPGetFeatures(d, size, &config.input) != VP8_STATUS_OK) return;
  if (config.input.width <= 0 || config.input.height <= 0) return;
  if (static_cast<size_t>(config.input.width) * config.input.height >
      kMaxOutputBytes / 4) {
    return;
  }

  uint8_t alignment_byte = d[size - 1];
  int extra = (alignment_byte & 0x3F);
  int stride = config.input.width * 4 + extra;
  size_t buffer_size = static_cast<size_t>(config.input.height) * stride;
  if (buffer_size > kMaxOutputBytes) return;

  std::vector<uint8_t> out_buf(buffer_size);
  config.output.colorspace = MODE_RGBA;
  config.output.is_external_memory = 1;
  config.output.u.RGBA.rgba = out_buf.data();
  config.output.u.RGBA.stride = stride;
  config.output.u.RGBA.size = out_buf.size();

  (void)WebPDecode(d, size, &config);
  WebPFreeDecBuffer(&config.output);
}
FUZZ_TEST(PictureGaps, DecodeExternalMemoryTest).WithDomains(fuzztest::String());

// --- SharpYuvConvert: caller-controlled strides + bit depths ---
void SharpYuvTest(std::string_view blob) {
  if (blob.size() < 16) return;
  const auto* d = reinterpret_cast<const uint8_t*>(blob.data());
  const size_t size = blob.size();

  int w = ClampDim(ReadU32(d)) & ~1;
  int h = ClampDim(ReadU32(d + 4)) & ~1;
  if (w < 2 || h < 2) return;
  int rgb_bit_depth = (d[8] & 1) ? 8 : 10;
  int yuv_bit_depth = (d[9] & 1) ? 8 : 10;
  int rgb_step = (rgb_bit_depth == 8) ? 3 : 6;
  int rgb_stride = w * rgb_step;
  int y_stride = w * (yuv_bit_depth == 8 ? 1 : 2);
  int uv_stride = (w / 2) * (yuv_bit_depth == 8 ? 1 : 2);

  size_t rgb_bytes = static_cast<size_t>(h) * rgb_stride;
  size_t y_bytes = static_cast<size_t>(h) * y_stride;
  size_t uv_bytes = static_cast<size_t>(h / 2) * uv_stride;
  if (rgb_bytes + y_bytes + uv_bytes > kMaxOutputBytes) return;

  std::vector<uint8_t> rgb(rgb_bytes);
  std::vector<uint8_t> y(y_bytes);
  std::vector<uint8_t> u(uv_bytes);
  std::vector<uint8_t> v(uv_bytes);

  if (size > 16) {
    for (size_t i = 0; i < rgb.size(); i++) {
      rgb[i] = d[16 + (i % (size - 16))];
    }
  }

  const uint8_t* r_ptr = rgb.data();
  const uint8_t* g_ptr = rgb.data() + (rgb_step >= 2 ? 1 : 0);
  const uint8_t* b_ptr = rgb.data() + (rgb_step >= 3 ? 2 : 0);
  SharpYuvMatrixType matrix_id =
      static_cast<SharpYuvMatrixType>(d[10] % kSharpYuvMatrixNum);

  (void)SharpYuvConvert(r_ptr, g_ptr, b_ptr, rgb_step, rgb_stride,
                        rgb_bit_depth, y.data(), y_stride, u.data(), uv_stride,
                        v.data(), uv_stride, yuv_bit_depth, w, h,
                        SharpYuvGetConversionMatrix(matrix_id));
}
FUZZ_TEST(PictureGaps, SharpYuvTest).WithDomains(fuzztest::String());

}  // namespace
