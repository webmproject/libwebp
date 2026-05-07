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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sharpyuv/sharpyuv.h"
#include "sharpyuv/sharpyuv_csp.h"
#include "testing/base/public/gunit.h"
#include "testing/fuzzing/fuzztest.h"

namespace {

void SharpYuvConvertFuzz(int width, int height, int rgb_bit_depth,
                         int yuv_bit_depth,
                         const std::vector<uint16_t>& r_samples,
                         const std::vector<uint16_t>& g_samples,
                         const std::vector<uint16_t>& b_samples,
                         int matrix_type_int, int transfer_type_int) {
  const size_t num_pixels = static_cast<size_t>(width) * height;
  ASSERT_LE(num_pixels, r_samples.size());
  ASSERT_LE(num_pixels, g_samples.size());
  ASSERT_LE(num_pixels, b_samples.size());

  const SharpYuvMatrixType matrix_type =
      static_cast<SharpYuvMatrixType>(matrix_type_int);
  const SharpYuvTransferFunctionType transfer_type =
      static_cast<SharpYuvTransferFunctionType>(transfer_type_int);

  const SharpYuvConversionMatrix* matrix =
      SharpYuvGetConversionMatrix(matrix_type);
  ASSERT_NE(matrix, nullptr);

  SharpYuvOptions options;
  ASSERT_TRUE(SharpYuvOptionsInit(matrix, &options));
  options.transfer_type = transfer_type;

  const int yuv_bytes = (yuv_bit_depth > 8) ? 2 : 1;
  const int y_stride = width * yuv_bytes;
  const int uv_stride = ((width + 1) / 2) * yuv_bytes;

  std::vector<uint8_t> y_dst(y_stride * height);
  std::vector<uint8_t> u_dst(uv_stride * ((height + 1) / 2));
  std::vector<uint8_t> v_dst(uv_stride * ((height + 1) / 2));

  if (rgb_bit_depth == 8) {
    std::vector<uint8_t> r8(num_pixels), g8(num_pixels), b8(num_pixels);
    for (size_t i = 0; i < num_pixels; ++i) {
      r8[i] = static_cast<uint8_t>(r_samples[i]);
      g8[i] = static_cast<uint8_t>(g_samples[i]);
      b8[i] = static_cast<uint8_t>(b_samples[i]);
    }
    EXPECT_TRUE(SharpYuvConvertWithOptions(
        r8.data(), g8.data(), b8.data(), 1, width, rgb_bit_depth, y_dst.data(),
        y_stride, u_dst.data(), uv_stride, v_dst.data(), uv_stride,
        yuv_bit_depth, width, height, &options));
  } else {
    EXPECT_TRUE(SharpYuvConvertWithOptions(
        r_samples.data(), g_samples.data(), b_samples.data(), 2, width * 2,
        rgb_bit_depth, y_dst.data(), y_stride, u_dst.data(), uv_stride,
        v_dst.data(), uv_stride, yuv_bit_depth, width, height, &options));
  }
}

auto AnyTransferFunction() {
  return fuzztest::ElementOf<int>({
      kSharpYuvTransferFunctionBt709,
      kSharpYuvTransferFunctionBt470M,
      kSharpYuvTransferFunctionBt470Bg,
      kSharpYuvTransferFunctionBt601,
      kSharpYuvTransferFunctionSmpte240,
      kSharpYuvTransferFunctionLinear,
      kSharpYuvTransferFunctionLog100,
      kSharpYuvTransferFunctionLog100_Sqrt10,
      kSharpYuvTransferFunctionIec61966,
      kSharpYuvTransferFunctionBt1361,
      kSharpYuvTransferFunctionSrgb,
      kSharpYuvTransferFunctionBt2020_10Bit,
      kSharpYuvTransferFunctionBt2020_12Bit,
      kSharpYuvTransferFunctionSmpte2084,
      kSharpYuvTransferFunctionSmpte428,
      kSharpYuvTransferFunctionHlg,
  });
}

auto SharpYuvDomain() {
  return fuzztest::FlatMap(
      [](int w, int h, int rgb_bd, int yuv_bd) {
        // Create input/output buffers of the right size.
        const size_t n = static_cast<size_t>(w * h);
        return fuzztest::TupleOf(
            fuzztest::Just(w), fuzztest::Just(h), fuzztest::Just(rgb_bd),
            fuzztest::Just(yuv_bd),
            fuzztest::VectorOf(fuzztest::Arbitrary<uint16_t>()).WithSize(n),
            fuzztest::VectorOf(fuzztest::Arbitrary<uint16_t>()).WithSize(n),
            fuzztest::VectorOf(fuzztest::Arbitrary<uint16_t>()).WithSize(n),
            fuzztest::InRange(0, static_cast<int>(kSharpYuvMatrixNum) - 1),
            AnyTransferFunction());
      },
      /*width=*/fuzztest::InRange(1, 128),
      /*height=*/fuzztest::InRange(1, 128),
      /*rgb_bit_depth=*/fuzztest::ElementOf({8, 10, 12, 16}),
      /*yuv_bit_depth=*/fuzztest::ElementOf({8, 10, 12}));
}

void SharpYuvConvertFuzzWrapped(
    const std::tuple<int, int, int, int, std::vector<uint16_t>,
                     std::vector<uint16_t>, std::vector<uint16_t>, int, int>&
        args) {
  std::apply(SharpYuvConvertFuzz, args);
}

FUZZ_TEST(SharpYuvFuzz, SharpYuvConvertFuzzWrapped)
    .WithDomains(SharpYuvDomain());

}  // namespace
