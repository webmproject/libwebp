// Copyright 2024 Google Inc.
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
#include <string_view>

#include "./nalloc.h"
#include "src/dsp/cpu.h"
#include "tests/fuzzer/fuzz_utils.h"
#include "webp/decode.h"

namespace {

const VP8CPUInfo default_VP8GetCPUInfo = fuzz_utils::VP8GetCPUInfo;

void DecodeWebP(std::string_view arbitrary_bytes,
                const fuzz_utils::WebPDecoderOptionsCpp& decoder_options,
                uint32_t optimization_index) {
  if (fuzz_utils::IsImageTooBig(
          reinterpret_cast<const uint8_t*>(arbitrary_bytes.data()),
          arbitrary_bytes.size())) {
    return;
  }
  fuzz_utils::SetOptimization(default_VP8GetCPUInfo, optimization_index);
  WebPDecoderConfig decoder_config;
  if (!WebPInitDecoderConfig(&decoder_config)) {
    fprintf(stderr, "WebPInitDecoderConfig failed.\n");
    std::abort();
  }
  const uint8_t* const data =
      reinterpret_cast<const uint8_t*>(arbitrary_bytes.data());
  const size_t size = arbitrary_bytes.size();
  nalloc_init(nullptr);
  nalloc_start(data, size);
  std::memcpy(&decoder_config.options, &decoder_options,
              sizeof(decoder_options));
  const VP8StatusCode status = WebPDecode(data, size, &decoder_config);
  nalloc_end();
  if (status == VP8_STATUS_OK) {
    fuzz_utils::CompareDecodeOptimizations(
        default_VP8GetCPUInfo, optimization_index, data, size, decoder_config);
  }
  WebPFreeDecBuffer(&decoder_config.output);
  // The decoding may fail (because the fuzzed input can be anything) but not
  // for these reasons.
  if (status == VP8_STATUS_SUSPENDED || status == VP8_STATUS_USER_ABORT) {
    std::abort();
  }
}

FUZZ_TEST(WebPSuite, DecodeWebP)
    .WithDomains(
        fuzz_utils::ArbitraryImageWithSeeds(
            fuzz_utils::FuzzSeedFormat::kWebPOnly),
        fuzz_utils::ArbitraryValidWebPDecoderOptions(),
        /*optimization_index=*/
        fuzztest::InRange<uint32_t>(0, fuzz_utils::kMaxOptimizationIndex));

}  // namespace
