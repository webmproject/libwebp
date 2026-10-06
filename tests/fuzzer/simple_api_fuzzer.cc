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
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "./fuzz_utils.h"
#include "gtest/gtest.h"
#include "webp/decode.h"
#include "webp/types.h"

namespace {

void SimpleApiTest(std::string_view data_in) {
  const uint8_t* const data = reinterpret_cast<const uint8_t*>(data_in.data());
  const size_t size = data_in.size();
  int w, h;
  if (!WebPGetInfo(data, size, &w, &h)) return;
  if ((size_t)w * h > fuzz_utils::kFuzzPxLimit) return;

  const uint8_t value = fuzz_utils::FuzzHash(data, size);
  uint8_t* buf = NULL;

  // For *Into functions, which decode into an external buffer, an
  // intentionally too small buffer can be given with low probability.
  if (value < 0x16) {
    buf = WebPDecodeRGBA(data, size, &w, &h);
  } else if (value < 0x2b) {
    buf = WebPDecodeBGRA(data, size, &w, &h);
#if !defined(WEBP_REDUCE_CSP)
  } else if (value < 0x40) {
    buf = WebPDecodeARGB(data, size, &w, &h);
  } else if (value < 0x55) {
    buf = WebPDecodeRGB(data, size, &w, &h);
  } else if (value < 0x6a) {
    buf = WebPDecodeBGR(data, size, &w, &h);
#endif  // !defined(WEBP_REDUCE_CSP)
  } else if (value < 0x7f) {
    uint8_t *u, *v;
    int stride, uv_stride;
    buf = WebPDecodeYUV(data, size, &w, &h, &u, &v, &stride, &uv_stride);
  } else if (value < 0xe8) {
    const int stride = (value < 0xbe ? 4 : 3) * w;
    size_t buf_size = stride * h;
    if (value % 0x10 == 0) buf_size--;
    uint8_t* const ext_buf = (uint8_t*)malloc(buf_size);
    if (value < 0x94) {
      (void)WebPDecodeRGBAInto(data, size, ext_buf, buf_size, stride);
#if !defined(WEBP_REDUCE_CSP)
    } else if (value < 0xa9) {
      (void)WebPDecodeARGBInto(data, size, ext_buf, buf_size, stride);
    } else if (value < 0xbe) {
      (void)WebPDecodeBGRInto(data, size, ext_buf, buf_size, stride);
    } else if (value < 0xd3) {
      (void)WebPDecodeRGBInto(data, size, ext_buf, buf_size, stride);
#endif  // !defined(WEBP_REDUCE_CSP)
    } else {
      (void)WebPDecodeBGRAInto(data, size, ext_buf, buf_size, stride);
    }
    free(ext_buf);
  } else {
    size_t luma_size = w * h;
    const int uv_stride = (w + 1) / 2;
    size_t u_size = uv_stride * (h + 1) / 2;
    size_t v_size = uv_stride * (h + 1) / 2;
    if (value % 0x10 == 0) {
      if (size & 1) luma_size--;
      if (size & 2) u_size--;
      if (size & 4) v_size--;
    }
    uint8_t* const luma_buf = (uint8_t*)malloc(luma_size);
    uint8_t* const u_buf = (uint8_t*)malloc(u_size);
    uint8_t* const v_buf = (uint8_t*)malloc(v_size);
    (void)WebPDecodeYUVInto(data, size, luma_buf, luma_size,
                            w /* luma_stride */, u_buf, u_size, uv_stride,
                            v_buf, v_size, uv_stride);
    free(luma_buf);
    free(u_buf);
    free(v_buf);
  }

  if (buf) WebPFree(buf);
}

}  // namespace

FUZZ_TEST(SimpleApi, SimpleApiTest)
    .WithDomains(fuzztest::String().WithMaxSize(fuzz_utils::kMaxWebPFileSize +
                                                1));

TEST(SimpleApi, Buganizer498966511) {
  SimpleApiTest(
      std::string("ALPH\004\000\000\000A\377\377\377\377LP\010\000\000\000\000"
                  "\000\000\311H\006\000\000\000\"E\356PW\"ALPH\000\000\000\000"
                  "ALpH\004\000\000\000\004\010\000\200VP8 "
                  "T\000\000\000\266\003\000\235\001*"
                  "\001\000\002\000y\336n\366\001O\363\374\243\000\003LPS\"\002"
                  "iF\000FjRsa\232vP\"EO\"K\217OM;rOect\275n\"Wsection_JUNQ="
                  "\"JUNQ\"\250YO,_I\362\021\"ANIM\"",
                  150));
}

// More than 200 prefix code groups are remapped to a dense numbering by the
// decoder. Group ids are numbered in order of first use, which differs from the
// numeric order of the ids here, so the codes read from the bitstream must be
// stored through the mapping.
TEST(SimpleApi, ManyHuffmanGroupsUsedOutOfOrder) {
  constexpr int kSize = 64;  // 16x16 blocks of 4x4 pixels.
  constexpr int kBlocksPerRow = 16;
  constexpr int kNumGroups = 201;

  std::vector<uint8_t> bits;  // LSB-first bit packing, as in VP8L.
  size_t num_bits = 0;
  auto put = [&](uint32_t value, int num) {
    for (int i = 0; i < num; ++i, ++num_bits) {
      if (num_bits % 8 == 0) bits.push_back(0);
      bits.back() |= ((value >> i) & 1) << (num_bits % 8);
    }
  };
  // Prefix code with a single symbol: its pixels cost no bit.
  auto single_symbol_code = [&](uint32_t symbol, bool is_8_bits) {
    put(1, 1);  // simple code
    put(0, 1);  // one symbol
    put(is_8_bits, 1);
    put(symbol, is_8_bits ? 8 : 1);
  };
  auto color = [](int group) {  // RGBA
    return std::array<uint8_t, 4>{static_cast<uint8_t>(group),
                                  static_cast<uint8_t>(255 - group),
                                  static_cast<uint8_t>(group * 7), 255};
  };

  // Meta image: groups 1 and 0 are used first, then 2, 3, ..., 200.
  std::vector<int> meta(kBlocksPerRow * kBlocksPerRow, 0);
  meta[0] = 1;
  meta[1] = 0;
  for (int g = 2; g < kNumGroups; ++g) meta[g] = g;

  put(0x2f, 8);  // VP8L signature
  put(kSize - 1, 14);
  put(kSize - 1, 14);
  put(1, 1);  // alpha is used
  put(0, 3);  // version
  put(0, 1);  // no transform
  put(0, 1);  // no color cache
  put(1, 1);  // meta prefix codes
  put(0, 3);  // 4x4 blocks
  // Entropy-coded meta image.
  put(0, 1);  // no color cache
  // Green: 256 literals, all with a code length of 8.
  put(0, 1);
  put(12 - 4, 4);
  for (int i = 0; i < 12; ++i) put(i == 11 ? 1 : 0, 3);  // only length 8 used
  put(1, 1);
  put(3, 3);
  put(254, 8);  // 256 code lengths follow (all 8, in zero bits).
  for (int i = 0; i < 4; ++i) single_symbol_code(0, false);  // R, B, A, dist
  for (const int group : meta) {
    uint32_t reversed = 0;  // Codes are stored bit-reversed.
    for (int i = 0; i < 8; ++i) reversed |= ((group >> i) & 1) << (7 - i);
    put(reversed, 8);
  }
  // One constant color per group.
  for (int g = 0; g < kNumGroups; ++g) {
    const std::array<uint8_t, 4> c = color(g);
    single_symbol_code(c[1], true);  // green
    single_symbol_code(c[0], true);  // red
    single_symbol_code(c[2], true);  // blue
    single_symbol_code(c[3], true);  // alpha
    single_symbol_code(0, false);    // distance
  }
  bits.resize(bits.size() + 8, 0);
  if (bits.size() & 1) bits.push_back(0);

  std::vector<uint8_t> webp = {'R', 'I', 'F', 'F', 0,   0,   0,   0,
                               'W', 'E', 'B', 'P', 'V', 'P', '8', 'L'};
  auto put_le32 = [&webp](size_t pos, uint32_t v) {
    for (int i = 0; i < 4; ++i) webp[pos + i] = (v >> (8 * i)) & 0xff;
  };
  webp.resize(webp.size() + 4);
  put_le32(16, static_cast<uint32_t>(bits.size()));
  webp.insert(webp.end(), bits.begin(), bits.end());
  put_le32(4, static_cast<uint32_t>(webp.size() - 8));

  int width, height;
  const std::unique_ptr<uint8_t, decltype(&WebPFree)> rgba(
      WebPDecodeRGBA(webp.data(), webp.size(), &width, &height), WebPFree);
  ASSERT_NE(rgba.get(), nullptr);
  ASSERT_EQ(width, kSize);
  ASSERT_EQ(height, kSize);
  int num_wrong_pixels = 0;
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      const std::array<uint8_t, 4> expected =
          color(meta[(y / 4) * kBlocksPerRow + (x / 4)]);
      const uint8_t* const pixel = rgba.get() + 4 * (y * kSize + x);
      if (!std::equal(expected.begin(), expected.end(), pixel)) {
        ++num_wrong_pixels;
      }
    }
  }
  EXPECT_EQ(num_wrong_pixels, 0);
}
