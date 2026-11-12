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
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>  // NOLINT
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
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
  if (feature == kSSE4_1 || feature == kAVX || feature == kAVX2) return 0;
  return GetCPUInfo(feature);
}

static WEBP_INLINE int GetCPUInfoNoAVX(CPUFeature feature) {
  if (feature == kAVX || feature == kAVX2) return 0;
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

std::vector<std::string> GetDictionaryFromFiles(
    const std::vector<std::string_view>& file_paths) {
  std::vector<std::string> lines;
  for (std::string_view file_path : file_paths) {
    std::ifstream dictfile{std::string(file_path)};
    if (!dictfile.is_open()) {
      std::cerr << file_path << " cannot be opened\n";
      std::abort();
    }
    for (std::string line; std::getline(dictfile, line);) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      lines.push_back(line);
    }
  }
  return lines;
}

namespace {

bool ShouldIncludeSeedFile(std::string_view path, FuzzSeedFormat format) {
  const size_t slash = path.find_last_of("/\\");
  const std::string_view filename =
      (slash == std::string_view::npos) ? path : path.substr(slash + 1);
  const size_t dot = filename.rfind('.');
  const std::string_view ext = (dot == std::string_view::npos || dot == 0)
                                   ? std::string_view()
                                   : filename.substr(dot);
  if (ext.empty()) return true;
  std::string lower_ext(ext);
  for (char& c : lower_ext) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  switch (format) {
    case FuzzSeedFormat::kWebPOnly:
      return lower_ext == ".webp";
    case FuzzSeedFormat::kWebPAndGif:
      return lower_ext == ".webp" || lower_ext == ".gif";
    case FuzzSeedFormat::kImageIo:
      for (std::string_view allowed :
           {".webp", ".png", ".jpg", ".jpeg", ".ppm", ".pgm", ".pbm", ".pam",
            ".pnm", ".tif", ".tiff", ".bin"}) {
        if (lower_ext == allowed) return true;
      }
      return false;
  }
  return false;
}

}  // namespace

std::vector<std::string> ReadFilesFromDirectory(std::string_view dir,
                                                FuzzSeedFormat format) {
  if (!std::filesystem::is_directory(dir)) return {};
  std::vector<std::tuple<std::string>> tuples =
      fuzztest::ReadFilesFromDirectory(dir, [format](std::string_view path) {
        return ShouldIncludeSeedFile(path, format);
      });
  std::vector<std::string> strings(tuples.size());
  for (size_t i = 0; i < tuples.size(); ++i) {
    using std::swap;
    swap(std::get<0>(tuples[i]), strings[i]);
  }
  return strings;
}

#if !defined(WEBP_HAS_CUSTOM_FUZZ_SEEDS)
std::vector<std::string> DefaultDictionary(FuzzSeedFormat /*format*/) {
  for (std::string_view path : {"tests/fuzzer/fuzz.dict", "fuzz.dict"}) {
    if (std::filesystem::is_regular_file(path)) {
      return GetDictionaryFromFiles({path});
    }
  }
  return {};
}

std::vector<std::string> DefaultSeeds(FuzzSeedFormat format) {
  std::vector<std::string> seeds;
  if (const char* const dir = std::getenv("TEST_DATA_DIRS");
      dir != nullptr && *dir != '\0') {
    seeds = ReadFilesFromDirectory(dir, format);
  }
  if (seeds.empty()) seeds.push_back("");
  return seeds;
}
#endif  // !defined(WEBP_HAS_CUSTOM_FUZZ_SEEDS)

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

bool CompareDecBuffers(const WebPDecBuffer& a, const WebPDecBuffer& b) {
  if (a.colorspace != b.colorspace || a.width != b.width ||
      a.height != b.height) {
    return false;
  }
  if (WebPIsRGBMode(a.colorspace)) {
    return a.u.RGBA.size == b.u.RGBA.size &&
           std::memcmp(a.u.RGBA.rgba, b.u.RGBA.rgba, a.u.RGBA.size) == 0;
  }
  if (a.u.YUVA.y_size != b.u.YUVA.y_size ||
      a.u.YUVA.u_size != b.u.YUVA.u_size ||
      a.u.YUVA.v_size != b.u.YUVA.v_size ||
      std::memcmp(a.u.YUVA.y, b.u.YUVA.y, a.u.YUVA.y_size) != 0 ||
      std::memcmp(a.u.YUVA.u, b.u.YUVA.u, a.u.YUVA.u_size) != 0 ||
      std::memcmp(a.u.YUVA.v, b.u.YUVA.v, a.u.YUVA.v_size) != 0) {
    return false;
  }
  if (a.u.YUVA.a != nullptr || b.u.YUVA.a != nullptr) {
    return a.u.YUVA.a != nullptr && b.u.YUVA.a != nullptr &&
           a.u.YUVA.a_size == b.u.YUVA.a_size &&
           std::memcmp(a.u.YUVA.a, b.u.YUVA.a, a.u.YUVA.a_size) == 0;
  }
  return true;
}

namespace {

bool CropOrScaleAndEncodeOne(const CropOrScaleParams& crop_or_scale_params,
                             WebPConfig& config, WebPPicture& pic,
                             WebPMemoryWriter& memory_writer) {
  // Crop and scale.
  if (!CropOrScale(&pic, crop_or_scale_params)) {
    const WebPEncodingError error_code = pic.error_code;
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return false;
    std::cerr << "CropOrScale failed. Error code: " << error_code << "\n";
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
    std::cerr << "WebPEncode failed. Error code: " << error_code << "\n";
    std::abort();
  }
  return true;
}

}  // namespace

bool CropOrScaleAndEncode(const CropOrScaleParams& crop_or_scale_params,
                          WebPConfig& config, WebPPicture& pic,
                          VP8CPUInfo default_VP8GetCPUInfo,
                          uint32_t optimization_index,
                          WebPMemoryWriter& memory_writer) {
  WebPPicture pic_ref;
  if (!WebPPictureInit(&pic_ref)) {
    std::cerr << "WebPPictureInit failed.\n";
    std::abort();
  }
  std::unique_ptr<WebPPicture, UniquePtrDeleter> pic_ref_owner(&pic_ref);
  if (!WebPPictureCopy(&pic, &pic_ref)) return false;
  WebPConfig config_ref = config;

  SetOptimization(default_VP8GetCPUInfo, optimization_index);
  if (!CropOrScaleAndEncodeOne(crop_or_scale_params, config, pic,
                               memory_writer)) {
    return false;
  }

  // Verify that encoding produces the exact same bitstream independently of the
  // optimization level.
  const uint32_t ref_optimization_index =
      (optimization_index == kMaxOptimizationIndex) ? 0 : kMaxOptimizationIndex;
  SetOptimization(default_VP8GetCPUInfo, ref_optimization_index);
  WebPMemoryWriter ref_memory_writer;
  WebPMemoryWriterInit(&ref_memory_writer);
  std::unique_ptr<WebPMemoryWriter, UniquePtrDeleter> ref_memory_writer_owner(
      &ref_memory_writer);
  if (!CropOrScaleAndEncodeOne(crop_or_scale_params, config_ref, pic_ref,
                               ref_memory_writer)) {
    return false;
  }
  if (memory_writer.size != ref_memory_writer.size ||
      std::memcmp(memory_writer.mem, ref_memory_writer.mem,
                  memory_writer.size) != 0) {
    std::cerr << "Encoded bitstream mismatch between optimizations "
              << optimization_index << " and " << ref_optimization_index
              << ".\n";
    std::abort();
  }
  SetOptimization(default_VP8GetCPUInfo, optimization_index);
  return true;
}

void CompareDecodeOptimizations(VP8CPUInfo default_VP8GetCPUInfo,
                                uint32_t optimization_index,
                                const uint8_t* data, size_t size,
                                const WebPDecoderConfig& config) {
  const uint32_t ref_optimization_index =
      (optimization_index == kMaxOptimizationIndex) ? 0 : kMaxOptimizationIndex;
  SetOptimization(default_VP8GetCPUInfo, ref_optimization_index);
  WebPDecoderConfig ref_config;
  std::unique_ptr<WebPDecoderConfig, UniquePtrDeleter> ref_config_owner(
      &ref_config);
  if (!WebPInitDecoderConfig(&ref_config)) {
    std::cerr << "WebPInitDecoderConfig failed.\n";
    std::abort();
  }
  ref_config.output.colorspace = config.output.colorspace;
  ref_config.options = config.options;
  const VP8StatusCode ref_status = WebPDecode(data, size, &ref_config);
  SetOptimization(default_VP8GetCPUInfo, optimization_index);
  if (ref_status != VP8_STATUS_OK && ref_status != VP8_STATUS_OUT_OF_MEMORY &&
      ref_status != VP8_STATUS_USER_ABORT) {
    std::cerr << "WebPDecode failed. status: " << ref_status << ".\n";
    std::abort();
  }
  if (ref_status == VP8_STATUS_OK &&
      !CompareDecBuffers(config.output, ref_config.output)) {
    std::cerr << "Decoded output mismatch between optimizations "
              << optimization_index << " and " << ref_optimization_index
              << ".\n";
    std::abort();
  }
}

}  // namespace fuzz_utils
