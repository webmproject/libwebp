// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Alpha plane encoding and decoding library.
//
// Author: vikasa@google.com (Vikas Arora)

#include <string.h>  // for memcpy()
#include "./alpha.h"

#include "./bit_reader.h"
#include "./bit_writer.h"
#include "./tcoder.h"

#define MAX_SYMBOLS      255
#define ALPHA_HEADER_LEN 2

// -----------------------------------------------------------------------------
// Alpha Encode.

static int EncodeIdent(const uint8_t* data, int width, int height,
                       uint8_t** output, size_t* output_size) {
  const size_t data_size = height * width;
  uint8_t* alpha = NULL;
  assert((output != NULL) && (output_size != NULL));

  if (data == NULL) {
    return 0;
  }

  alpha = (uint8_t*)malloc(data_size);
  if (alpha == NULL) {
    return 0;
  }
  memcpy(alpha, data, data_size);
  *output_size = data_size;
  *output = alpha;
  return 1;
}

// -----------------------------------------------------------------------------
// Zlib-like encoding using TCoder

typedef struct {
  int dist;        // backward distance (=0 means: literal)
  int literal;     // literal value (if dist = 0)
  size_t len;      // length of matched string for non-literal
} Token;

#define MIN_LEN 2
#define DEFER_SKIP 1      // for deferred evaluation (0 = off)

#define CACHED_COST(coder, c) ((cost_cache[(c)] == 0.) ?             \
  (cost_cache[(c)] = lit_mode_cost + TCoderSymbolCost((coder), (c))) \
  : cost_cache[(c)])

// Record symbol
#define RECORD(TOKEN) {                                       \
  TCoderEncode(coderd, (TOKEN)->dist, NULL);                  \
  if ((TOKEN)->dist == 0) {                                   \
    TCoderEncode(coder, (TOKEN)->literal, NULL);              \
  } else {                                                    \
    TCoderEncode(coderl, (TOKEN)->len - MIN_LEN, NULL);       \
  }                                                           \
}

static size_t GetLongestMatch(const uint8_t* const data,
                              const uint8_t* const ref, size_t max_len) {
  size_t n;
  for (n = 0; n < max_len && (data[n] == ref[n]); ++n) { /* do nothing */ }
  return n;
}

static int EncodeZlibTCoder(const uint8_t* data, int width, int height,
                            uint8_t** output, size_t* output_size) {
  int ok = 0;
  const size_t data_size = width * height;
  const size_t MAX_DIST = 3 * width;
  const size_t MAX_LEN = 2 * width;
  Token* const msg = (Token*)malloc(data_size * sizeof(*msg));
  int num_tokens;
  TCoder* const coder = TCoderNew(MAX_SYMBOLS);
  TCoder* const coderd = TCoderNew(MAX_DIST);
  TCoder* const coderl = TCoderNew(MAX_LEN - MIN_LEN);

  if (coder == NULL || coderd == NULL || coderl == NULL) {
    goto End;
  }
  if (msg == NULL) {
    goto End;
  }

  {
    int deferred_eval = 0;
    size_t n = 0;
    num_tokens = 0;
    while (n < data_size) {
      const double lit_mode_cost = TCoderSymbolCost(coderd, 0);
      double cost_cache[MAX_SYMBOLS + 1] = { 0. };
      Token best;
      size_t dist = 0;
      double best_cost = CACHED_COST(coder, data[n]);
      size_t max_len = MAX_LEN;
      if (max_len > data_size - n) {
        max_len = data_size - n;
      }
      best.dist = 0;
      best.literal = data[n];
      best.len = 1;
      for (dist = 1; dist <= MAX_DIST && dist <= n; ++dist) {
        const int pos = n - dist;
        const size_t min_len = best.len - 1;
        size_t len;

        // Early out: we probe at two locations for a quick match check
        if (data[pos] != data[n] ||
            data[pos + min_len] != data[n + min_len]) {
          continue;
        }

        len = GetLongestMatch(data + pos, data + n, max_len);
        if (len >= MIN_LEN && len >= best.len) {
          // This is the cost of the coding proposal
          const double cost = TCoderSymbolCost(coderl, len - MIN_LEN)
                            + TCoderSymbolCost(coderd, dist);
          // We're gaining an extra len-best.len coded message over the last
          // known best. Compute how this would have cost if coded all literal.
          // (TODO: we should fully re-evaluate at position best.len and not
          // assume all is going be coded as literals. But it's at least an
          // upper-bound (worst-case coding). Deferred evaluation used below
          // partially addresses this.
          double lit_cost = 0;
          size_t i;
          for (i = best.len; i < len; ++i) {
            lit_cost += CACHED_COST(coder, data[n + i]);
          }
          // So, is it worth ?
          if (best_cost + lit_cost >= cost) {
            best_cost = cost;
            best.len = len;
            best.dist = dist;
          }
        }
        if (len >= MAX_LEN) {
          break;  // No need to search further. We already got a max-long match
        }
      }
      // Deferred evaluation: before finalizing a choice we try to find
      // best cost at position n + 1 and see if we get a longer
      // match then current best. If so, we transform the current match
      // into a literal, go to position n + 1, and try again.
      {
        Token* cur = &msg[num_tokens];
        int forget = 0;
        if (deferred_eval) {
          --cur;
          // If the next match isn't longer, keep previous match
          if (best.len <= cur->len) {
            deferred_eval = 0;
            n += cur->len - DEFER_SKIP;
            forget = 1;   // forget the new match
            RECORD(cur)
          } else {   // else transform previous match into a shorter one
            cur->len = DEFER_SKIP;
            if (DEFER_SKIP == 1) {
              cur->dist = 0;    // literal
            }
            // TODO(later): RECORD() macro should be changed to take an extra
            // "is_final" param, so that we could write the bitstream at once.
            RECORD(cur)
            ++cur;
          }
        }
        if (!forget) {
          *cur = best;
          ++num_tokens;
          if (DEFER_SKIP > 0) {
            deferred_eval = (cur->len > 2) && (cur->len < MAX_LEN / 2);
          }
          if (deferred_eval) {
            // will probe at a later position before finalizing.
            n += DEFER_SKIP;
          } else {
            // Keep the current choice.
            n += cur->len;
            RECORD(cur)
          }
        }
      }
    }
  }

  // Final bitstream assembly.
  {
    int n;
    VP8BitWriter bw;
    VP8BitWriterInit(&bw, 0);
    TCoderInit(coder);
    TCoderInit(coderd);
    TCoderInit(coderl);
    for (n = 0; n < num_tokens; ++n) {
      const Token* const t = &msg[n];
      const int is_literal = (t->dist == 0);
      TCoderEncode(coderd, t->dist, &bw);
      if (is_literal) {  // literal
        TCoderEncode(coder, t->literal, &bw);
      } else {
        TCoderEncode(coderl, t->len - MIN_LEN, &bw);
      }
    }

    // clean up
    VP8BitWriterFinish(&bw);
    *output = VP8BitWriterBuf(&bw);
    *output_size = VP8BitWriterSize(&bw);
    ok = 1;
  }

 End:
  if (coder) TCoderDelete(coder);
  if (coderl) TCoderDelete(coderl);
  if (coderd) TCoderDelete(coderd);
  free(msg);
  return ok;
}

// -----------------------------------------------------------------------------

int EncodeAlpha(const uint8_t* data, int width, int height, int stride,
                int quality, int method,
                uint8_t** output, size_t* output_size) {
  const int kMaxImageDim = (1 << 14) - 1;
  uint8_t* compressed_alpha = NULL;
  uint8_t* quant_alpha = NULL;
  uint8_t* out = NULL;
  size_t compressed_size = 0;
  const size_t data_size = height * width;
  float mse = 0.0;
  int ok = 0;
  int h;

  if ((data == NULL) || (output == NULL) || (output_size == NULL)) {
    return 0;
  }

  if (width <= 0 || width > kMaxImageDim ||
      height <= 0 || height > kMaxImageDim || stride < width) {
    return 0;
  }

  if (quality < 0 || quality > 100) {
    return 0;
  }

  if (method < 0 || method > 1) {
    return 0;
  }

  quant_alpha = (uint8_t*)malloc(data_size);
  if (quant_alpha == NULL) {
    return 0;
  }

  // Extract the alpha data (WidthXHeight) from raw_data (StrideXHeight).
  for (h = 0; h < height; ++h) {
    memcpy(quant_alpha + h * width, data + h * stride, width);
  }

  if (quality < 100) {  // No Quantization required for 'quality = 100'.
    // 16 Alpha levels gives quite a low MSE w.r.t Original Alpha plane hence
    // mapped to moderate quality 70. Hence Quality:[0, 70] -> Levels:[2, 16]
    // and Quality:]70, 100] -> Levels:]16, 256].
    const int alpha_levels = (quality <= 70) ?
                             2 + quality / 5 :
                             16 + (quality - 70) * 8;

    ok = QuantizeLevels(quant_alpha, width, height, alpha_levels, &mse);
    if (!ok) {
      free(quant_alpha);
      return 0;
    }
  }

  if (method == 0) {
    ok = EncodeIdent(quant_alpha, width, height,
                     &compressed_alpha, &compressed_size);
  } else if (method == 1) {
    ok = EncodeZlibTCoder(quant_alpha, width, height,
                          &compressed_alpha, &compressed_size);
  }

  free(quant_alpha);
  if (!ok) {
    return 0;
  }

  out = (uint8_t*)malloc(compressed_size + ALPHA_HEADER_LEN);
  if (out == NULL) {
    free(compressed_alpha);
    return 0;
  } else {
    *output = out;
  }

  // Alpha bit-stream Header:
  // Byte0: Compression Method.
  // Byte1: Reserved for later extension.
  out[0] = method & 0xff;
  out[1] = 0;  // Reserved Byte.
  out += ALPHA_HEADER_LEN;
  memcpy(out, compressed_alpha, compressed_size);
  free(compressed_alpha);
  out += compressed_size;

  *output_size = out - *output;

  return 1;
}

// -----------------------------------------------------------------------------
// Alpha Decode.

static int DecodeIdent(const uint8_t* data, size_t data_size,
                       uint8_t* output) {
  assert((data != NULL) && (output != NULL));
  memcpy(output, data, data_size);
  return 1;
}

static int DecompressZlibTCoder(const uint8_t* data, size_t data_size,
                                int width, int height,
                                uint8_t* output, size_t output_size) {
  int ok = 1;
  const size_t MAX_DIST = 3 * width;
  const size_t MAX_LEN = 2 * width;
  TCoder* const coder = TCoderNew(MAX_SYMBOLS);
  TCoder* const coderd = TCoderNew(MAX_DIST);
  TCoder* const coderl = TCoderNew(MAX_LEN - MIN_LEN);

  if (coder == NULL || coderd == NULL || coderl == NULL) {
    goto End;
  }
  (void)height;     // unused parameter

  {
    size_t pos = 0;
    VP8BitReader br;
    VP8InitBitReader(&br, data, data + data_size);
    while (pos < output_size) {
      const int dist = TCoderDecode(coderd, &br);
      if (dist == 0) {
        const int literal = TCoderDecode(coder, &br);
        output[pos] = literal;
        ++pos;
      } else {
        const int len = MIN_LEN + TCoderDecode(coderl, &br);
        int k;
        if (pos + len > output_size) goto End;
        for (k = 0; k < len; ++k) {
          output[pos + k] = output[pos + k - dist];
        }
        pos += len;
      }
    }
  }
  ok = 1;

 End:
  if (coder) TCoderDelete(coder);
  if (coderl) TCoderDelete(coderl);
  if (coderd) TCoderDelete(coderd);
  return ok;
}

// -----------------------------------------------------------------------------

int DecodeAlpha(const uint8_t* data, size_t data_size,
                int width, int height, int stride,
                uint8_t* output) {
  uint8_t* decoded_data = NULL;
  int ok = 0;
  int method;
  size_t decoded_size = height * width;

  if (data == NULL || output == NULL) {
    return 0;
  }

  if (data_size <= ALPHA_HEADER_LEN) {
    return 0;
  }

  if (width <= 0 || height <= 0 || stride < width) {
    return 0;
  }

  method = data[0];
  if (method < 0 || method > 1) {
    return 0;
  }

  decoded_data = (uint8_t*)malloc(decoded_size);
  if (decoded_data == NULL) {
    return 0;
  }

  data_size -= ALPHA_HEADER_LEN;
  data += ALPHA_HEADER_LEN;

  if (method == 0) {
    ok = DecodeIdent(data, data_size, decoded_data);
  } else if (method == 1) {
    ok = DecompressZlibTCoder(data, data_size, width, height,
                              decoded_data, decoded_size);
  }

  if (ok) {
    // Construct raw_data (HeightXStride) from the alpha data (HeightXWidth).
    int h;
    for (h = 0; h < height; ++h) {
      memcpy(output + h * stride, decoded_data + h * width, width);
    }
  }
  free(decoded_data);

  return ok;
}
