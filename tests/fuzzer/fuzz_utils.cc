// Copyright 2024 Google LLC
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

#include "./fuzz_utils.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "./img_alpha.h"
#include "./img_grid.h"
#include "./img_peak.h"
#include "imageio/jpegdec.h"
#include "imageio/pngdec.h"
#include "imageio/pnmdec.h"
#include "imageio/tiffdec.h"
#include "src/dsp/cpu.h"
#include "webp/decode.h"
#include "webp/encode.h"
#include "webp/types.h"

namespace fuzz_utils {

WebPPicture GetSourcePicture(int image_index, bool use_argb) {
  WebPPicture pic;
  if (!WebPPictureInit(&pic)) std::abort();
  pic.use_argb = use_argb;

  // Pick a source picture.
  const int kImagesWidth[] = {kImgAlphaWidth, kImgGridWidth, kImgPeakWidth};
  const int kImagesHeight[] = {kImgAlphaHeight, kImgGridHeight, kImgPeakHeight};
  const uint8_t* const image_data = kImagesData[image_index];
  pic.width = kImagesWidth[image_index];
  pic.height = kImagesHeight[image_index];
  pic.argb_stride = pic.width * 4 * sizeof(uint8_t);

  // Read the bytes.
  if (!WebPPictureImportRGBA(&pic, image_data, pic.argb_stride)) std::abort();
  return pic;
}

//------------------------------------------------------------------------------

int CropOrScale(WebPPicture* const pic, const CropOrScaleParams& params) {
  if (pic == NULL) return 0;
#if !defined(WEBP_REDUCE_SIZE)
  if (params.alter_input) {
    if (params.crop_or_scale) {
      const int cropped_width = std::max(1, pic->width / params.width_ratio);
      const int cropped_height = std::max(1, pic->height / params.height_ratio);
      const int cropped_left = (pic->width - cropped_width) / params.left_ratio;
      const int cropped_top = (pic->height - cropped_height) / params.top_ratio;
      return WebPPictureCrop(pic, cropped_left, cropped_top, cropped_width,
                             cropped_height);
    } else {
      const int scaled_width = 1 + (pic->width * params.width_ratio) / 8;
      const int scaled_height = 1 + (pic->height * params.height_ratio) / 8;
      return WebPPictureRescale(pic, scaled_width, scaled_height);
    }
  }
#else   // defined(WEBP_REDUCE_SIZE)
  (void)pic;
  (void)params;
#endif  // !defined(WEBP_REDUCE_SIZE)
  return 1;
}

extern "C" VP8CPUInfo VP8GetCPUInfo;
static VP8CPUInfo GetCPUInfo;

static WEBP_INLINE int GetCPUInfoNoSSE41(CPUFeature feature) {
  if (feature == kSSE4_1 || feature == kAVX) return 0;
  return GetCPUInfo(feature);
}

static WEBP_INLINE int GetCPUInfoNoAVX(CPUFeature feature) {
  if (feature == kAVX) return 0;
  return GetCPUInfo(feature);
}

static WEBP_INLINE int GetCPUInfoForceSlowSSSE3(CPUFeature feature) {
  if (feature == kSlowSSSE3 && GetCPUInfo(kSSE3)) {
    return 1;  // we have SSE3 -> force SlowSSSE3
  }
  return GetCPUInfo(feature);
}

static WEBP_INLINE int GetCPUInfoOnlyC(CPUFeature feature) {
  (void)feature;
  return 0;
}

void SetOptimization(VP8CPUInfo default_VP8GetCPUInfo, uint32_t index) {
  assert(index <= kMaxOptimizationIndex);
  GetCPUInfo = default_VP8GetCPUInfo;
  const VP8CPUInfo kVP8CPUInfos[kMaxOptimizationIndex + 1] = {
      GetCPUInfoOnlyC, GetCPUInfoForceSlowSSSE3, GetCPUInfoNoSSE41,
      GetCPUInfoNoAVX, GetCPUInfo};
  VP8GetCPUInfo = kVP8CPUInfos[index];
}

//------------------------------------------------------------------------------

std::vector<std::string> ReadFilesFromDirectory(std::string_view dir) {
  std::vector<std::tuple<std::string>> tuples =
      fuzztest::ReadFilesFromDirectory(dir);
  std::vector<std::string> strings(tuples.size());
  for (size_t i = 0; i < tuples.size(); ++i) {
    using std::swap;
    swap(std::get<0>(tuples[i]), strings[i]);
  }
  return strings;
}

//------------------------------------------------------------------------------

bool IsImageTooBig(const uint8_t* data, size_t size) {
  int width = 0, height = 0;
  if (ReadJpegDimensions(data, size, &width, &height) ||
      WebPGetInfo(data, size, &width, &height) ||
      ReadPngDimensions(data, size, &width, &height) ||
      ReadPnmDimensions(data, size, &width, &height) ||
      ReadTiffDimensions(data, size, &width, &height)) {
    if (width <= 0 || height <= 0) return false;
    // Look at the number of 8x8px blocks rather than the overall pixel count
    // when comparing to memory and duration thresholds.
    const uint64_t ceiled_width = ((uint64_t)width + 7) / 8 * 8;
    const uint64_t ceiled_height = ((uint64_t)height + 7) / 8 * 8;
    // Threshold to avoid out-of-memory and timeout issues.
    // The threshold is arbitrary but below the fuzzer limit of 2 GB.
    // The value cannot be 2 GB because of the added memory by MSAN.
    if (ceiled_width > kFuzzPxLimit || ceiled_height > kFuzzPxLimit ||
        ceiled_width * ceiled_height > kFuzzPxLimit) {
      return true;
    }
  }
  return false;
}

}  // namespace fuzz_utils
