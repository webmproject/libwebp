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

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "./fuzz_utils.h"
#include "gtest/gtest.h"
#include "webp/demux.h"
#include "webp/mux.h"
#include "webp/mux_types.h"

namespace {

void MuxDemuxApiTest(std::string_view data_in, bool use_mux_api,
                     const std::array<int, 10>& chunk_flags) {
  const size_t size = data_in.size();
  WebPData webp_data;
  WebPDataInit(&webp_data);
  webp_data.size = size;
  webp_data.bytes = reinterpret_cast<const uint8_t*>(data_in.data());

  // Extracted chunks and frames are not processed or decoded,
  // which is already covered extensively by the other fuzz targets.

  if (use_mux_api) {
    // Mux API
    WebPMux* mux = WebPMuxCreate(&webp_data, size & 2);
    if (!mux) return;

    WebPData chunk;
    (void)WebPMuxGetChunk(mux, "EXIF", &chunk);
    (void)WebPMuxGetChunk(mux, "ICCP", &chunk);
    (void)WebPMuxGetChunk(mux, "FUZZ", &chunk);  // unknown

    uint32_t flags;
    (void)WebPMuxGetFeatures(mux, &flags);

    int width = 0, height = 0;
    (void)WebPMuxGetCanvasSize(mux, &width, &height);

    WebPMuxAnimParams params;
    (void)WebPMuxGetAnimationParams(mux, &params);

    WebPMuxError status;
    WebPMuxFrameInfo info;
    for (int i = 0; i < fuzz_utils::kFuzzFrameLimit; i++) {
      status = WebPMuxGetFrame(mux, i + 1, &info);
      if (status == WEBP_MUX_NOT_FOUND) {
        break;
      } else if (status == WEBP_MUX_OK) {
        WebPDataClear(&info.bitstream);
      }
    }

    // Try setting a custom chunk.
    if (size > 20) {
      WebPData custom_chunk;
      custom_chunk.bytes = reinterpret_cast<const uint8_t*>(data_in.data()) + 4;
      custom_chunk.size = size - 4;
      int chunk_idx = 0;
      for (std::string_view fourcc : {"VP8X", "ICCP", "ANIM", "EXIF", "XMP ",
                                      "ANMF", "ALPH", "VP8 ", "VP8L", "FUZZ"}) {
        // The last five image chunks should return WEBP_MUX_INVALID_ARGUMENT.
        for (int i = 0; i < chunk_flags[chunk_idx]; i++) {
          (void)WebPMuxSetChunk(mux, fourcc.data(), &custom_chunk, 0);
        }
        chunk_idx++;
      }
    }

    // Try assembling the mux
    WebPData assembled;
    WebPDataInit(&assembled);
    (void)WebPMuxAssemble(mux, &assembled);
    WebPDataClear(&assembled);

    // Get number of chunks of various types
    for (int chunk_type = WEBP_CHUNK_VP8X; chunk_type <= WEBP_CHUNK_NIL;
         ++chunk_type) {
      int num_chunks = 0;
      (void)WebPMuxNumChunks(mux, static_cast<WebPChunkId>(chunk_type),
                             &num_chunks);
    }

    WebPMuxDelete(mux);
  } else {
    // Demux API
    WebPDemuxer* demux;
    if (size & 2) {
      WebPDemuxState state;
      demux = WebPDemuxPartial(&webp_data, &state);
      if (state < WEBP_DEMUX_PARSED_HEADER) {
        WebPDemuxDelete(demux);
        return;
      }
    } else {
      demux = WebPDemux(&webp_data);
      if (!demux) return;
    }

    WebPChunkIterator chunk_iter;
    if (WebPDemuxGetChunk(demux, "EXIF", 1, &chunk_iter)) {
      (void)WebPDemuxNextChunk(&chunk_iter);
    }
    WebPDemuxReleaseChunkIterator(&chunk_iter);
    if (WebPDemuxGetChunk(demux, "ICCP", 0, &chunk_iter)) {  // 0 == last
      (void)WebPDemuxPrevChunk(&chunk_iter);
    }
    WebPDemuxReleaseChunkIterator(&chunk_iter);
    // Skips FUZZ because the Demux API has no concept of (un)known chunks.

    WebPIterator iter;
    if (WebPDemuxGetFrame(demux, 1, &iter)) {
      for (int i = 1; i < fuzz_utils::kFuzzFrameLimit; i++) {
        if (!WebPDemuxNextFrame(&iter)) break;
      }
    }

    WebPDemuxReleaseIterator(&iter);
    WebPDemuxDelete(demux);
  }
}

// Tests building and modifying a WebPMux object from scratch (canvas size,
// animation params, image/frame insertion, chunk setting/deletion, frame
// deletion, and final assembly).
void MuxEditApiTest(std::string_view data_in,
                    const std::array<int, 10>& chunk_flags,
                    std::pair<int, int> canvas_size, uint32_t bgcolor,
                    int loop_count, int x_offset, int y_offset, int duration,
                    bool dispose_bg, bool no_blend, bool use_set_image,
                    bool copy_data, uint32_t delete_frame_nth) {
  const std::unique_ptr<WebPMux, fuzz_utils::UniquePtrDeleter> mux(
      WebPMuxNew());
  if (!mux) return;

  // Set canvas size and animation parameters.
  if (WebPMuxSetCanvasSize(mux.get(), canvas_size.first, canvas_size.second) !=
      WEBP_MUX_OK) {
    return;
  }

  const WebPMuxAnimParams params = {bgcolor, loop_count};
  if (WebPMuxSetAnimationParams(mux.get(), &params) != WEBP_MUX_OK) return;

  const size_t size = data_in.size();
  WebPData webp_data;
  WebPDataInit(&webp_data);
  webp_data.size = size;
  webp_data.bytes = reinterpret_cast<const uint8_t*>(data_in.data());

  // Set a single image or push an animation frame.
  if (use_set_image) {
    if (WebPMuxSetImage(mux.get(), &webp_data, copy_data) != WEBP_MUX_OK) {
      return;
    }
  } else {
    WebPMuxFrameInfo frame_info = {};
    frame_info.bitstream = webp_data;
    frame_info.x_offset = x_offset;
    frame_info.y_offset = y_offset;
    frame_info.duration = duration;
    frame_info.id = WEBP_CHUNK_ANMF;
    frame_info.dispose_method =
        dispose_bg ? WEBP_MUX_DISPOSE_BACKGROUND : WEBP_MUX_DISPOSE_NONE;
    frame_info.blend_method = no_blend ? WEBP_MUX_NO_BLEND : WEBP_MUX_BLEND;
    if (WebPMuxPushFrame(mux.get(), &frame_info, copy_data) != WEBP_MUX_OK) {
      return;
    }
  }

  // Try setting or deleting chunks of various known and unknown types.
  if (size > 20) {
    WebPData custom_chunk;
    custom_chunk.bytes = reinterpret_cast<const uint8_t*>(data_in.data()) + 4;
    custom_chunk.size = size - 4;
    int chunk_idx = 0;
    for (std::string_view fourcc : {"VP8X", "ICCP", "ANIM", "EXIF", "XMP ",
                                    "ANMF", "ALPH", "VP8 ", "VP8L", "FUZZ"}) {
      if (chunk_flags[chunk_idx] == 1) {
        if (WebPMuxSetChunk(mux.get(), fourcc.data(), &custom_chunk,
                            copy_data) != WEBP_MUX_OK) {
          return;
        }
      } else if (chunk_flags[chunk_idx] == 2) {
        if (WebPMuxDeleteChunk(mux.get(), fourcc.data()) != WEBP_MUX_OK) return;
      }
      chunk_idx++;
    }
  }

  // Optionally delete a frame (1-based index).
  if (delete_frame_nth > 0) {
    if (WebPMuxDeleteFrame(mux.get(), delete_frame_nth) != WEBP_MUX_OK) return;
  }

  // Try assembling the mux.
  WebPData assembled;
  WebPDataInit(&assembled);
  if (WebPMuxAssemble(mux.get(), &assembled) != WEBP_MUX_OK) return;
  WebPDataClear(&assembled);
}

auto ArbitraryWebPString() {
  return fuzztest::String().WithMaxSize(fuzz_utils::kMaxWebPFileSize + 1);
}

}  // namespace

FUZZ_TEST(MuxDemuxApi, MuxDemuxApiTest)
    .WithDomains(
        ArbitraryWebPString(),
        /*use_mux_api=*/fuzztest::Arbitrary<bool>(),
        /*chunk_flags=*/fuzztest::ArrayOf<10>(fuzztest::InRange(0, 2)));

FUZZ_TEST(MuxDemuxApi, MuxEditApiTest)
    .WithDomains(
        ArbitraryWebPString(),
        /*chunk_flags=*/fuzztest::ArrayOf<10>(fuzztest::InRange(0, 2)),
        /*canvas_size=*/
        fuzztest::PairOf(fuzztest::OneOf(fuzztest::Arbitrary<int>(),
                                         fuzztest::InRange(0, (1 << 16) - 1),
                                         fuzztest::InRange(0, 1 << 24)),
                         fuzztest::OneOf(fuzztest::Arbitrary<int>(),
                                         fuzztest::InRange(0, (1 << 16) - 1),
                                         fuzztest::InRange(0, 1 << 24))),
        /*bgcolor=*/fuzztest::Arbitrary<uint32_t>(),
        /*loop_count=*/fuzztest::Arbitrary<int>(),
        /*x_offset=*/fuzztest::Arbitrary<int>(),
        /*y_offset=*/fuzztest::Arbitrary<int>(),
        /*duration=*/fuzztest::Arbitrary<int>(),
        /*dispose_bg=*/fuzztest::Arbitrary<bool>(),
        /*no_blend=*/fuzztest::Arbitrary<bool>(),
        /*use_set_image=*/fuzztest::Arbitrary<bool>(),
        /*copy_data=*/fuzztest::Arbitrary<bool>(),
        /*delete_frame_nth=*/fuzztest::InRange<uint32_t>(0, 2));

// WebPMuxSetCanvasSize() multiplied 'width * height' as ints, which overflows
// for valid canvases whose area is in [2^31, 2^32).
TEST(MuxDemuxApi, SetCanvasSizeLargeArea) {
  WebPMux* const mux = WebPMuxNew();
  ASSERT_NE(mux, nullptr);
  EXPECT_EQ(WebPMuxSetCanvasSize(mux, 50000, 50000), WEBP_MUX_OK);
  EXPECT_EQ(WebPMuxSetCanvasSize(mux, 65535, 65537), WEBP_MUX_OK);
  EXPECT_EQ(WebPMuxSetCanvasSize(mux, 65536, 65536), WEBP_MUX_INVALID_ARGUMENT);
  EXPECT_EQ(WebPMuxSetCanvasSize(mux, 0, 5), WEBP_MUX_INVALID_ARGUMENT);
  EXPECT_EQ(WebPMuxSetCanvasSize(mux, 5, 0), WEBP_MUX_INVALID_ARGUMENT);
  EXPECT_EQ(WebPMuxSetCanvasSize(mux, 0, 0), WEBP_MUX_OK);
  WebPMuxDelete(mux);
}
