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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string_view>

#include "./fuzz_utils.h"
#include "./nalloc.h"
#include "examples/anim_util.h"
#include "webp/decode.h"
#include "webp/demux.h"
#include "webp/mux_types.h"

namespace {

void ReadAnimatedImageTest(std::string_view blob) {
  const uint8_t* const data = reinterpret_cast<const uint8_t*>(blob.data());
  const size_t size = blob.size();
  if (fuzz_utils::IsImageTooBig(data, size)) return;

  // Check GIF canvas and sub-image dimensions. ReadAnimatedGIF calls DGifSlurp
  // (which allocates every sub-image's RasterBits) and AllocateFrames (which
  // allocates all frame canvases simultaneously).
  if (size >= 13 && (std::memcmp(data, "GIF87a", 6) == 0 ||
                     std::memcmp(data, "GIF89a", 6) == 0)) {
    uint32_t canvas_width = data[6] | (data[7] << 8);
    uint32_t canvas_height = data[8] | (data[9] << 8);
    if ((size_t)canvas_width * canvas_height > fuzz_utils::kFuzzPxLimit) {
      return;
    }
    size_t offset = 13;
    if (data[10] & 0x80) {
      offset += 3 * (1u << ((data[10] & 0x07) + 1));
    }
    size_t frame_count = 0;
    size_t total_raster_pixels = 0;
    while (offset < size && data[offset] != 0x3b) {
      if (data[offset] == 0x21) {  // Extension block.
        if (offset + 2 > size) break;
        offset += 2;
        while (offset < size && data[offset] != 0) {
          offset += 1 + data[offset];
        }
        offset += 1;
      } else if (data[offset] == 0x2c) {  // Image descriptor.
        if (offset + 10 > size) break;
        const uint32_t w = data[offset + 5] | (data[offset + 6] << 8);
        const uint32_t h = data[offset + 7] | (data[offset + 8] << 8);
        const uint8_t packed = data[offset + 9];
        ++frame_count;
        if (frame_count == 1 && (canvas_width == 0 || canvas_height == 0)) {
          canvas_width = w;
          canvas_height = h;
        }
        total_raster_pixels += (size_t)w * h;
        if ((size_t)w * h > fuzz_utils::kFuzzPxLimit ||
            total_raster_pixels > fuzz_utils::kFuzzPxLimit ||
            (size_t)canvas_width * canvas_height * frame_count >
                fuzz_utils::kFuzzPxLimit) {
          return;
        }
        offset += 10;
        if (packed & 0x80) {
          offset += 3 * (1u << ((packed & 0x07) + 1));
        }
        if (offset >= size) break;
        offset += 1;  // LZW minimum code size.
        while (offset < size && data[offset] != 0) {
          offset += 1 + data[offset];
        }
        offset += 1;
      } else {
        break;
      }
    }
  }

  // Check WebP animation canvas * frame_count and individual frame payloads.
  const WebPData webp_data = {data, size};
  std::unique_ptr<WebPDemuxer, fuzz_utils::UniquePtrDeleter> demux(
      WebPDemux(&webp_data));
  if (demux != nullptr) {
    const uint32_t cw = WebPDemuxGetI(demux.get(), WEBP_FF_CANVAS_WIDTH);
    const uint32_t ch = WebPDemuxGetI(demux.get(), WEBP_FF_CANVAS_HEIGHT);
    const uint32_t frame_count =
        WebPDemuxGetI(demux.get(), WEBP_FF_FRAME_COUNT);
    if ((size_t)cw * ch * frame_count > fuzz_utils::kFuzzPxLimit) {
      return;
    }
    WebPIterator iter;
    memset(&iter, 0, sizeof(iter));
    std::unique_ptr<WebPIterator, fuzz_utils::UniquePtrDeleter> iter_deleter(
        &iter);
    for (uint32_t i = 0; i < frame_count; ++i) {
      if (!WebPDemuxGetFrame(demux.get(), i + 1, &iter)) break;
      int w, h;
      if (WebPGetInfo(iter.fragment.bytes, iter.fragment.size, &w, &h) &&
          (size_t)w * h > fuzz_utils::kFuzzPxLimit) {
        return;
      }
    }
  }

  nalloc_init(nullptr);
  nalloc_start(data, size);

  AnimatedImage image;
  if (ReadAnimatedImageFromMemory("random_file", data, size, &image,
                                  /*dump_frames=*/0, /*dump_folder=*/nullptr)) {
    ClearAnimatedImage(&image);
  }

  nalloc_end();
}

}  // namespace

FUZZ_TEST(ReadAnimatedImage, ReadAnimatedImageTest)
    .WithDomains(fuzztest::String().WithMaxSize(fuzz_utils::kMaxWebPFileSize +
                                                1));
