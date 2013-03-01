// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Paginated token buffer
//
//  A 'token' is a bit value associated with a probability, either fixed
// or a later-to-be-determined after statistics have been collected.
// For dynamic probability, we just record the slot id (idx) for the probability
// value in the final probability array (uint8_t* probas in VP8EmitTokens).
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "./vp8enci.h"


#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define MAX_NUM_TOKEN 2048          // max number of token per page

struct VP8Tokens {
  uint16_t tokens_[MAX_NUM_TOKEN];  // bit#15: bit
                                    // bit #14: constant proba or idx
                                    // bits 0..13: slot or constant proba
  VP8Tokens* next_;
};

//------------------------------------------------------------------------------

#ifdef USE_TOKEN_BUFFER

void VP8TBufferInit(VP8TBuffer* const b) {
  b->tokens_ = NULL;
  b->pages_ = NULL;
  b->last_page_ = &b->pages_;
  b->left_ = 0;
  b->error_ = 0;
}

void VP8TBufferClear(VP8TBuffer* const b) {
  if (b != NULL) {
    const VP8Tokens* p = b->pages_;
    while (p != NULL) {
      const VP8Tokens* const next = p->next_;
      free((void*)p);
      p = next;
    }
    VP8TBufferInit(b);
  }
}

static int TBufferNewPage(VP8TBuffer* const b) {
  VP8Tokens* const page = b->error_ ? NULL : (VP8Tokens*)malloc(sizeof(*page));
  if (page == NULL) {
    b->error_ = 1;
    return 0;
  }
  *b->last_page_ = page;
  b->last_page_ = &page->next_;
  b->left_ = MAX_NUM_TOKEN;
  b->tokens_ = page->tokens_;
  page->next_ = NULL;
  return 1;
}

//------------------------------------------------------------------------------

#define TOKEN_ID(b, ctx, p) ((p) + NUM_PROBAS * ((ctx) + (b) * NUM_CTX))

static WEBP_INLINE int VP8AddToken(VP8TBuffer* const b,
                                   int bit, int proba_idx) {
  assert(proba_idx < (1 << 14));
  if (b->left_ > 0 || TBufferNewPage(b)) {
    const int slot = --b->left_;
    b->tokens_[slot] = ((!bit) << 15) | proba_idx;
  }
  return bit;
}

static WEBP_INLINE void VP8AddConstantToken(VP8TBuffer* const b,
                                            int bit, int proba) {
  assert(proba < 256);
  if (b->left_ > 0 || TBufferNewPage(b)) {
    const int slot = --b->left_;
    b->tokens_[slot] = (bit << 15) | (1 << 14) | proba;
  }
}

int VP8RecordCoeffTokens(int ctx, int first, int last,
                         const int16_t* const coeffs, VP8TBuffer* tokens) {
  int n = first;
  int b = VP8EncBands[n];
  if (!VP8AddToken(tokens, last >= 0, TOKEN_ID(b, ctx, 0))) {
    return 0;
  }

  while (n < 16) {
    const int c = coeffs[n++];
    const int sign = c < 0;
    int v = sign ? -c : c;
    const int base_id = TOKEN_ID(b, ctx, 0);
    if (!VP8AddToken(tokens, v != 0, base_id + 1)) {
      b = VP8EncBands[n];
      ctx = 0;
      continue;
    }
    if (!VP8AddToken(tokens, v > 1, base_id + 2)) {
      b = VP8EncBands[n];
      ctx = 1;
    } else {
      if (!VP8AddToken(tokens, v > 4, base_id + 3)) {
        if (VP8AddToken(tokens, v != 2, base_id + 4))
          VP8AddToken(tokens, v == 4, base_id + 5);
      } else if (!VP8AddToken(tokens, v > 10, base_id + 6)) {
        if (!VP8AddToken(tokens, v > 6, base_id + 7)) {
          VP8AddConstantToken(tokens, v == 6, 159);
        } else {
          VP8AddConstantToken(tokens, v >= 9, 165);
          VP8AddConstantToken(tokens, !(v & 1), 145);
        }
      } else {
        int mask;
        const uint8_t* tab;
        if (v < 3 + (8 << 1)) {          // VP8Cat3  (3b)
          VP8AddToken(tokens, 0, base_id + 8);
          VP8AddToken(tokens, 0, base_id + 9);
          v -= 3 + (8 << 0);
          mask = 1 << 2;
          tab = VP8Cat3;
        } else if (v < 3 + (8 << 2)) {   // VP8Cat4  (4b)
          VP8AddToken(tokens, 0, base_id + 8);
          VP8AddToken(tokens, 1, base_id + 9);
          v -= 3 + (8 << 1);
          mask = 1 << 3;
          tab = VP8Cat4;
        } else if (v < 3 + (8 << 3)) {   // VP8Cat5  (5b)
          VP8AddToken(tokens, 1, base_id + 8);
          VP8AddToken(tokens, 0, base_id + 10);
          v -= 3 + (8 << 2);
          mask = 1 << 4;
          tab = VP8Cat5;
        } else {                         // VP8Cat6 (11b)
          VP8AddToken(tokens, 1, base_id + 8);
          VP8AddToken(tokens, 1, base_id + 10);
          v -= 3 + (8 << 3);
          mask = 1 << 10;
          tab = VP8Cat6;
        }
        while (mask) {
          VP8AddConstantToken(tokens, !!(v & mask), *tab++);
          mask >>= 1;
        }
      }
      ctx = 2;
    }
    b = VP8EncBands[n];
    VP8AddConstantToken(tokens, sign, 128);
    if (n == 16 || !VP8AddToken(tokens, n <= last, TOKEN_ID(b, ctx, 0))) {
      return 1;   // EOB
    }
  }
  return 1;
}

#undef TOKEN_ID

//------------------------------------------------------------------------------

static void Record(int bit, proba_t* const stats) {
  proba_t p = *stats;
  if (p >= 0xffff0000u) {               // an overflow is inbound.
    p = ((p + 1u) >> 1) & 0x7fff7fffu;  // -> divide the stats by 2.
  }
  // record bit count (lower 16 bits) and increment total count (upper 16 bits).
  p += 0x00010000u + bit;
  *stats = p;
}

void VP8TokenToStats(const VP8TBuffer* const b, proba_t* const stats) {
  const VP8Tokens* p = b->pages_;
  while (p != NULL) {
    const int N = (p->next_ == NULL) ? b->left_ : 0;
    int n = MAX_NUM_TOKEN;
    while (n-- > N) {
      const uint16_t token = p->tokens_[n];
      if (!(token & (1 << 14))) {
        Record((token >> 15) & 1, stats + (token & 0x3fffu));
      }
    }
    p = p->next_;
  }
}

int VP8EmitTokens(const VP8TBuffer* const b, VP8BitWriter* const bw,
                  const uint8_t* const probas, int final_pass) {
  const VP8Tokens* p = b->pages_;
  (void)final_pass;
  if (b->error_) return 0;
  while (p != NULL) {
    const VP8Tokens* const next = p->next_;
    const int N = (next == NULL) ? b->left_ : 0;
    int n = MAX_NUM_TOKEN;
    while (n-- > N) {
      const uint16_t token = p->tokens_[n];
      if (token & (1 << 14)) {
        VP8PutBit(bw, (token >> 15) & 1, token & 0x3fffu);  // constant proba
      } else {
        VP8PutBit(bw, (token >> 15) & 1, probas[token & 0x3fffu]);
      }
    }
    p = next;
  }
  return 1;
}

//------------------------------------------------------------------------------
#else

void VP8TBufferInit(VP8TBuffer* const b) {
  (void)b;
}
void VP8TBufferClear(VP8TBuffer* const b) {
  (void)b;
}

#endif    // USE_TOKEN_BUFFER

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
