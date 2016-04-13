// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Assymetric Numeral System coder
//
// Author: Skal (pascal.massimino@gmail.com)

#ifdef HAVE_CONFIG_H
#include "../webp/config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "./ans.h"
#include "./utils.h"

#if defined(WEBP_EXPERIMENTAL_FEATURES)

//------------------------------------------------------------------------------
// Decoder

static void ReadNextWord(ANSDec* const ans) {
  ans->state_ <<= IO_BITS;
  if (ans->pos_ + IO_BYTES <= ans->max_pos_) {
#if IO_BITS == 8
    ans->state_ |= ans->buf_[ans->pos_];
#else
    ans->state_ |= (ans->buf_[ans->pos_] << 8) | ans->buf_[ans->pos_ + 1];
#endif
    ans->pos_ += IO_BYTES;
  } else {
    ans->error_ = 1;
  }
}

uint32_t ANSDecReadBit(ANSDec* const ans, uint32_t p0) {
  const int q0 = PROBA_MAX - p0;
  if (p0 == 0) return 0;
  if (q0 == 0) return 1;
  {
    const uint32_t xfrac = ans->state_ & PROBA_MASK;
    const uint32_t bit = (xfrac >= p0);
    if (!bit) {
      ans->state_ = p0 * (ans->state_ >> PROBA_BITS) + xfrac;
    } else {
      ans->state_ = q0 * (ans->state_ >> PROBA_BITS) + xfrac - p0;
    }
    if (ans->state_ < IO_LIMIT_LO) ReadNextWord(ans);
    return bit;
  }
}

void ANSBinSymbolInit(ANSBinSymbol* const stats) {
  assert(stats != NULL);
  stats->p0 = stats->p1 = 1;
}

uint32_t ANSBinSymbolUpdate(ANSBinSymbol* const stats, uint32_t bit) {
  if (bit) {
    ++stats->p1;
  } else {
    ++stats->p0;
  }
  // TODO(skal): overflow detection / renormalization + alternate update schemes
  return bit;
}

uint32_t ANSBinSymbolProba(const ANSBinSymbol* const stats) {
  const uint32_t sum = stats->p0 + stats->p1;
  const uint32_t proba = (uint32_t)(((uint64_t)stats->p0 << PROBA_BITS) / sum);
  return proba;
}

uint32_t ANSDecReadABit(ANSDec* const ans, ANSBinSymbol* const stats) {
  const uint32_t bit = ANSDecReadBit(ans, ANSBinSymbolProba(stats));
  return ANSBinSymbolUpdate(stats, bit);
}

uint32_t ANSDecReadSymbol(ANSDec* const ans,
                          const ANSSymbolInfo codes[],
                          uint32_t log2_tab_size) {
  const uint32_t res = ans->state_ & ((1u << log2_tab_size) - 1);
  const ANSSymbolInfo s = codes[res];
  ans->state_ = s.freq_ * (ans->state_ >> log2_tab_size) + s.offset_;
  if (ans->state_ < IO_LIMIT_LO) ReadNextWord(ans);
  return s.symbol_;
}

uint32_t ANSDecReadUValue(ANSDec* const ans, int bits) {
  const uint32_t value = ans->state_ & ((1u << bits) - 1);
  ans->state_ >>= bits;
  if (ans->state_ < IO_LIMIT_LO) ReadNextWord(ans);
  return value;
}

int ANSDecInit(ANSDec* const ans, const uint8_t* buf, size_t len) {
  if (ans == NULL || buf == NULL || len < 2) return 0;
  ans->buf_ = buf;
  ans->max_pos_ = len;
  ans->pos_ = 0;
  ans->error_ = 0;
  ans->state_ = 0;
  ReadNextWord(ans);
  ReadNextWord(ans);
#if IO_BITS == 8
  ReadNextWord(ans);
#endif
  return !ans->error_;
}

void ANSDecClear(ANSDec* const ans) {
  // do nothing for now, but this could change
  (void)ans;
}

//------------------------------------------------------------------------------
// Dictionary management

int ANSEncDictionaryInit(ANSEncDictionary* const dict, int max_symbol) {
  assert(max_symbol <= ANS_MAX_SYMBOLS);
  assert(dict != NULL);
  ANSEncDictionaryClear(dict);
  if (max_symbol > 0) {
    dict->counts_ =
        (uint32_t*)WebPSafeCalloc(max_symbol, sizeof(*dict->counts_));
    dict->infos_ =
        (ANSSymbolInfo*)WebPSafeCalloc(max_symbol, sizeof(*dict->infos_));
    if (dict->counts_ == NULL || dict->infos_ == NULL) return 0;
    dict->max_symbol_ = max_symbol;
  }
  return 1;
}

static void DictionaryResetCounts(ANSEncDictionary* const dict) {
  dict->nb_symbols_ = -1;
  dict->total_ = 0;
  dict->stats_ok_ = 0;
  memset(dict->counts_, 0, dict->max_symbol_ * sizeof(*dict->counts_));
}

void ANSEncDictionaryClear(ANSEncDictionary* const dict) {
  if (dict != NULL) {
    WebPSafeFree(dict->counts_);
    WebPSafeFree(dict->infos_);
    dict->counts_ = NULL;
    dict->infos_ = NULL;
    dict->max_symbol_ = 0;
    dict->nb_symbols_ = 0;
    dict->total_ = 0;
    dict->stats_ok_ = 0;
    // don't reset max_freq_, log2_tab_size_, id_.
  }
}

static void ClearDictionaries(ANSEnc* const ans) {
  uint32_t i;
  for (i = 0; i < ans->nb_dict_; ++i) {
    ANSEncDictionaryClear(&ans->dict_[i]);
  }
  ans->nb_dict_ = 0;
}

ANSEncDictionary* ANSEncAddDictionary(ANSEnc* const ans, int max_symbol) {
  ANSEncDictionary* dict;
  assert(ans != NULL);
  assert(max_symbol <= ANS_MAX_SYMBOLS);
  if (ans->nb_dict_ >= ANS_MAX_DICTIONARIES) return NULL;   // full!
  dict = &ans->dict_[ans->nb_dict_];
  if (!ANSEncDictionaryInit(dict, max_symbol)) return NULL;
  dict->id_ = ans->nb_dict_++;
  return dict;
}

ANSEncDictionary* ANSEncGetDictionary(ANSEnc* const ans, uint32_t id) {
  return (id >= ans->nb_dict_) ? NULL : &ans->dict_[id];
}

static void ResetDictionaries(ANSEnc* const ans) {
  uint32_t i;
  for (i = 0; i < ans->nb_dict_; ++i) {
    DictionaryResetCounts(&ans->dict_[i]);
  }
}

void ANSDictionaryRecordSymbol(ANSEncDictionary* const dict, uint32_t symbol) {
  assert(dict != NULL);
  assert(symbol < dict->max_symbol_);
  dict->nb_symbols_ += (dict->counts_[symbol] == 0);
  ++dict->counts_[symbol];
  ++dict->total_;   // TODO(skal): only update total_ at the end instead?
  dict->stats_ok_ = 0;
}

//------------------------------------------------------------------------------
// Encoder

// Reset the bitstream buffer.
void ANSEncResetBuffer(ANSEnc* const ans) {
  WebPSafeFree(ans->buffer_);
  ans->buffer_ = NULL;
  ans->buffer_pos_ = 0;
  ans->max_buffer_size_ = 0;
  ans->state_ = ANS_SIGNATURE;
}

void ANSEncInit(ANSEnc* const ans) {
  memset(ans, 0, sizeof(*ans));
  ans->buffer_ = NULL;
  ans->tokens_ = NULL;
  ans->list_ = NULL;
  ans->error_ = 0;
  ans->nb_dict_ = 0;
  ANSEncResetBuffer(ans);
  ANSEncReset(ans, 1);
}

//------------------------------------------------------------------------------
// emit symbol/bit/uniform-value

static int ReallocBuffer(ANSEnc* const ans) {
  const size_t cur_size = ans->max_buffer_size_ - ans->buffer_pos_;
  const size_t new_size = (ans->max_buffer_size_ == 0) ? 4096
                        : 2 * ans->max_buffer_size_;
  uint8_t* const new_buffer =
      (uint8_t*)WebPSafeMalloc(new_size, sizeof(*new_buffer));
  if (new_buffer == NULL) {
    ans->error_ = 1;
    return 0;
  }
  if (ans->buffer_ != NULL) {
    memcpy(new_buffer + new_size - cur_size,
           ans->buffer_ + ans->buffer_pos_,
           cur_size);
    WebPSafeFree(ans->buffer_);
  }
  ans->buffer_ = new_buffer;
  ans->max_buffer_size_ = new_size;
  ans->buffer_pos_ = new_size - cur_size;
  return 1;
}

static void EmitWord(ANSEnc* const ans) {
  const uint32_t s = ans->state_;
  if (ans->buffer_pos_ < IO_BYTES) {
    if (!ReallocBuffer(ans)) return;
  }
  ans->buffer_[--ans->buffer_pos_] = (s >> 0) & 0xff;
#if IO_BITS == 16
  ans->buffer_[--ans->buffer_pos_] = (s >> 8) & 0xff;
#endif
  ans->state_ = s >> IO_BITS;
}

static void EmitBit(ANSEnc* const ans, uint32_t bit, uint32_t p0) {
  const uint32_t q0 = PROBA_MAX - p0;
  if (p0 == 0) {
    assert(bit == 0);
  } else if (q0 == 0) {
    assert(bit != 0);
  } else {
    uint32_t s = ans->state_;
    if ((s >> (IO_LIMIT_BITS + IO_BITS - PROBA_BITS)) >= (bit ? q0 : p0)) {
      EmitWord(ans);
      s = ans->state_;
      assert(s < IO_LIMIT_LO);
    }
    if (bit) {
      s = ((s / q0) << PROBA_BITS) + (s % q0) + p0;
    } else {
      s = ((s / p0) << PROBA_BITS) + (s % p0);
    }
    ans->state_ = s;
    assert(ans->state_ >= IO_LIMIT_LO);
  }
}

static void EmitSymbol(ANSEnc* const ans, const ANSSymbolInfo t,
                       uint32_t log2_tab_size) {
  uint32_t s = ans->state_;
  assert(log2_tab_size > 0 && log2_tab_size <= IO_LIMIT_BITS + IO_BITS);
  if ((s >> (IO_LIMIT_BITS + IO_BITS - log2_tab_size)) >= t.freq_) {
    EmitWord(ans);
    s = ans->state_;
  }
  ans->state_ = ((s / t.freq_) << log2_tab_size) + (s % t.freq_) + t.offset_;
  assert(ans->state_ >= IO_LIMIT_LO);
}

static void EmitUValue(ANSEnc* const ans, uint32_t value, int bits) {
  if (ans->state_ >> (IO_LIMIT_BITS + IO_BITS - bits) > 0) {
    EmitWord(ans);
  }
  ans->state_ = (ans->state_ << bits) + value;
  assert(ans->state_ >= IO_LIMIT_LO);
}

//------------------------------------------------------------------------------

static int FinishStream(ANSEnc* const ans) {
  EmitWord(ans);
  EmitWord(ans);
#if IO_BITS == 8
  EmitWord(ans);
#endif
  return !ans->error_;
}

static int EmitOnlyBits(ANSEnc* const ans) {
  const ANSTokenPage* page = ans->tokens_;
  while (page != NULL) {
    int n = page->nb_tokens_;
    while (--n >= 0) {
      const ANSToken tok = page->tokens_[n];
      const uint32_t p0 = tok & ANS_TOK_MASK;
      const int bit = !!(tok & ANS_TOK_BIT_MASK);
      assert((tok & ANS_TOK_TYPE) == ANS_TOK_IS_BIT);
      EmitBit(ans, bit, p0);
    }
    page = page->prev_;
  }
  return FinishStream(ans);
}

static int EmitNonSymbols(ANSEnc* const ans) {
  const ANSTokenPage* page = ans->tokens_;
  while (page != NULL) {
    int n = page->nb_tokens_;
    while (--n >= 0) {
      const ANSToken tok = page->tokens_[n];
      if ((tok & ANS_TOK_TYPE) == ANS_TOK_IS_BIT) {
        const uint32_t p0 = tok & ANS_TOK_MASK;
        const int bit = !!(tok & ANS_TOK_BIT_MASK);
        EmitBit(ans, bit, p0);
      } else {
        assert((tok & ANS_TOK_TYPE) == ANS_TOK_IS_UVAL);
        EmitUValue(ans, (tok >> 8) & ANS_TOK_MASK, tok & 0xff);
      }
    }
    page = page->prev_;
  }
  return FinishStream(ans);
}

static int EmitAll(ANSEnc* const ans) {
  const ANSTokenPage* page = ans->tokens_;
  while (page != NULL) {
    int n = page->nb_tokens_;
    while (--n >= 0) {
      const ANSToken tok = page->tokens_[n];
      if ((tok & ANS_TOK_TYPE) == ANS_TOK_IS_BIT) {
        const uint32_t p0 = tok & ANS_TOK_MASK;
        const int bit = !!(tok & ANS_TOK_BIT_MASK);
        EmitBit(ans, bit, p0);
      } else if ((tok & ANS_TOK_TYPE) == ANS_TOK_IS_UVAL) {
        EmitUValue(ans, (tok >> 8) & ANS_TOK_MASK, tok & 0xff);
      } else {  // symbol
        const uint32_t id = (tok >> 16) & 0xff;
        const uint32_t symbol = tok & 0xffff;
        assert(id < ANS_MAX_DICTIONARIES);
        assert(symbol < ans->dict_[id].max_symbol_);
        assert(ans->dict_[id].log2_tab_size_ <= 16);
        EmitSymbol(ans,
                   ans->dict_[id].infos_[symbol],
                   ans->dict_[id].log2_tab_size_);
      }
    }
    page = page->prev_;
  }
  return FinishStream(ans);
}

//------------------------------------------------------------------------------
// Selection helper function

static void SwapU32(uint32_t* const A, uint32_t* const B) {
  const uint32_t tmp = *A;
  *A = *B;
  *B = tmp;
}
static void CheckSwapU32(uint32_t* const A, uint32_t* const B) {
  assert(A <= B);
  if (A != B && *A < *B) SwapU32(A, B);
}

// select the Mth largest keys amongst N
static void Select(uint32_t* const keys, int M, int N) {
  int low = 0, hi = N - 1;
  if (M == N || N <= 1) return;  // done
  while (1) {
    const int mid = (low + hi) >> 1;
    if (low + 1 >= hi) {   // only 1 or 2 left
      if (low + 1 == hi) CheckSwapU32(keys + low, keys + low + 1);
      return;  // done!
    }
    // sort low | mid | hi triplet of entries
    CheckSwapU32(keys + low, keys + hi);
    CheckSwapU32(keys + mid, keys + hi);
    CheckSwapU32(keys + low, keys + mid);
    // move mid in position low + 1 (will serve as pivot)
    SwapU32(keys + low + 1, keys + mid);
    {
      const uint32_t pivot = keys[low + 1];
      // and start loop over [low + 2, hi - 1] sub-range
      int i = low + 2;
      int j = hi - 1;
      while (1) {
        while (keys[i] > pivot) ++i;
        while (keys[j] < pivot) --j;
        if (j < i) break;  // they crossed the streams!
        SwapU32(keys + i, keys + j);
      }
      keys[low + 1] = keys[j];    // move pivot back to position
      keys[j] = pivot;
      // recurse down (only one branch)
      if (j >= M) {
        hi = j - 1;
      } else {
        low = j + 1;
      }
    }
  }
}

// Analyze counts[] and renormalize with Squeaky Wheel fix, so that
// the total is rescaled to be equal to 'tab_size' exactly.
// Returns 0 in case of failure. Otherwise, returns the final size.
static int ANSNormalizeCounts(uint32_t counts[ANS_MAX_SYMBOLS],
                              uint32_t max_symbol, uint32_t tab_size) {
  uint32_t keys[ANS_MAX_SYMBOLS];
  uint32_t s, non_zero;
  int n, miss;
  float norm;
  const float key_norm = (float)((1u << 24) / ANS_MAX_SYMBOLS);
  uint32_t total = 0;

  if (max_symbol == 0) return 1;   // corner case, but ok.
  for (s = 0; s < max_symbol; ++s) {
    if (total > 0xffffffffu - counts[s]) return 0;  // overflow
    total += counts[s];
  }
  if (total == 0) {   // force a sane state
    counts[0] = tab_size;
    return tab_size;
  }
  if (total == tab_size) return total;   // already ok.

  norm = 1.f * tab_size / total;
  for (non_zero = 0, s = 0, miss = tab_size; s < max_symbol; ++s) {
    if (counts[s] > 0) {
      uint32_t error;
      const float target = norm * counts[s];
      counts[s] = (uint32_t)(target + .5);  // round
      if (counts[s] == 0) counts[s] = 1;
      miss -= counts[s];
      error = (uint32_t)fabsf(key_norm * (target - counts[s]));
      keys[non_zero++] = (error * ANS_MAX_SYMBOLS) + s;
    }
  }
  if (miss == 0) return tab_size;

  if (miss > 0) {
    Select(keys, miss, non_zero);
    for (n = 0; n < miss; ++n) {
      ++counts[keys[n] % ANS_MAX_SYMBOLS];
    }
  } else if (non_zero <= tab_size) {
    const uint32_t cap_count = (1u << 23) - 1;  // to avoid overflow
    // Overflow case. We need to decrease some counts, but need extra care
    // to not make any counts[] go to zero. So we just loop and shave off
    // the largest elements greater than 2 until we're good. It's guaranteed
    // to terminate.
    non_zero = 0;
    for (s = 0; s < max_symbol; ++s) {
      if (counts[s] > 1) {
        const uint32_t c = (counts[s] > cap_count) ? cap_count : counts[s];
        keys[non_zero++] = (c * ANS_MAX_SYMBOLS) + s;
      }
    }
    assert(non_zero > 0);
    miss = -miss;
    Select(keys, miss, non_zero);
    {
      int to_fix = miss;
      while (to_fix > 0) {
        for (n = 0; n < miss && to_fix > 0; ++n) {
          const uint32_t idx = keys[n] % ANS_MAX_SYMBOLS;
          if (counts[idx] > 1) {
            --counts[idx];
            --to_fix;
          }
        }
      }
    }
  } else {
    assert(0);  // impossible: more symbols than final table size expected.
    return 0;
  }
  return tab_size;
}

static void CountsToInfos(const uint32_t counts[], uint32_t max_symbol,
                          ANSSymbolInfo* infos) {
  uint32_t s;
  uint32_t total = 0;
  for (s = 0; s < max_symbol; ++s) {
    const uint32_t freq = counts[s];
    infos[s].freq_ = freq;
    infos[s].offset_ = total;
    infos[s].symbol_ = s;   // not really needed
    total += freq;
  }
}

int ANSCountsToSpreadTable(uint32_t counts[], int max_symbol,
                           uint32_t log2_tab_size, ANSSymbolInfo codes[]) {
  int s;
  uint32_t pos = 0;
  const int have_tab_size = (log2_tab_size > 0);
  const uint32_t tab_size = 1u << log2_tab_size;
  if (have_tab_size && !ANSNormalizeCounts(counts, max_symbol, tab_size)) {
    return 0;
  }
  for (s = 0; s < max_symbol; ++s) {
    const uint32_t freq = counts[s];
    uint32_t k;
    for (k = 0; (!have_tab_size || pos < tab_size) && k < freq; ++k, ++pos) {
      codes[pos].freq_ = freq;
      codes[pos].offset_ = k;
      codes[pos].symbol_ = s;
    }
  }
  return (!have_tab_size || pos == tab_size);
}

int ANSDictionaryToSpreadTable(const ANSEncDictionary* const dict,
                               ANSSymbolInfo codes[]) {
  uint32_t counts[ANS_MAX_SYMBOLS];
  if (dict == NULL || dict->max_symbol_ > ANS_MAX_SYMBOLS) {
    return 0;
  }
  memcpy(counts, dict->counts_, dict->max_symbol_ * sizeof(*counts));
  return ANSCountsToSpreadTable(counts, dict->max_symbol_,
                                dict->log2_tab_size_, codes);
}

//------------------------------------------------------------------------------
// Main call, that will gather stats and code the buffer out.

int ANSEncCollectStatistics(ANSEnc* const ans) {
  if (ans->nb_symbols_ < 0) {
    const ANSTokenPage* page = ans->tokens_;
    ans->nb_symbols_ = 0;
    while (page != NULL) {
      int n = page->nb_tokens_;
      while (--n >= 0) {
        const ANSToken tok = page->tokens_[n];
        if ((tok & ANS_TOK_TYPE) == ANS_TOK_IS_SYM) {
          const uint32_t symbol = tok & 0xffff;
          const int id = (tok >> 16) & 0xff;
          ANSDictionaryRecordSymbol(&ans->dict_[id], symbol);
          ++ans->nb_symbols_;
        }
      }
      page = page->prev_;
    }
  }
  if (ans->nb_symbols_ > 0) {
    uint32_t d;
    for (d = 0; d < ans->nb_dict_; ++d) {
      ANSEncDictionary* const dict = &ans->dict_[d];
      if (dict != NULL && !dict->stats_ok_) {
        if (dict->nb_symbols_ > 0) {
          assert(dict->total_ > 0);
          if (dict->log2_tab_size_ == 0) {            // size is unspecified...
            dict->log2_tab_size_ = ANS_LOG_TAB_SIZE;  // ... use default.
          } else if (dict->log2_tab_size_ > 16) {
 Error:
            ans->error_ = 1;
            return 0;
          }
          if (ANSDictionaryQuantize(dict) <= 0) {
            goto Error;
          }
        }
        dict->stats_ok_ = 1;
      }
    }
  }
  return !ans->error_;
}

int ANSDictionaryToCodingTable(const ANSEncDictionary* const dict) {
  if (dict->stats_ok_) {
    if (dict->nb_symbols_ == 0) return 1;
    if (dict->total_ != (1u << dict->log2_tab_size_)) {
      uint32_t counts[ANS_MAX_SYMBOLS];
      uint32_t total;
      assert(dict->max_symbol_ <= ANS_MAX_SYMBOLS);
      memcpy(counts, dict->counts_, dict->max_symbol_ * sizeof(*counts));
      total = ANSNormalizeCounts(counts, dict->max_symbol_,
                                 1u << dict->log2_tab_size_);
      if (total == 0) return 0;
      CountsToInfos(counts, dict->max_symbol_, dict->infos_);
    } else {
      CountsToInfos(dict->counts_, dict->max_symbol_, dict->infos_);
    }
    return 1;
  }
  return 0;
}

int ANSEncAssemble(ANSEnc* const ans) {
  ANSEncResetBuffer(ans);
  if (!ANSEncCollectStatistics(ans)) {
    return 0;
  }
  if (ans->nb_symbols_ == 0) {
    if (ans->nb_uvalues_ == 0) {
      return EmitOnlyBits(ans);     // only bits
    } else {
      return EmitNonSymbols(ans);   // bits + uvalues
    }
  } else {
    uint32_t d;
    for (d = 0; d < ans->nb_dict_; ++d) {
      if (!ANSDictionaryToCodingTable(&ans->dict_[d])) {
        return 0;
      }
    }
    return EmitAll(ans);
  }
}

void ANSEncWipeOut(ANSEnc* const ans) {
  ANSEncResetBuffer(ans);
  ANSEncClear(ans);
}

//------------------------------------------------------------------------------

void ANSEncReset(ANSEnc* const ans, int delete_pages) {
  ANSTokenPage* page = ans->tokens_;
  while (page != NULL) {
    ANSTokenPage* const next = (ANSTokenPage*)page->prev_;
    if (delete_pages) {
      WebPSafeFree(page);
    } else {
      page->prev_ = ans->list_;
      ans->list_ = page;
    }
    page = next;
  }
  ans->tokens_ = NULL;
  ans->nb_tokens_ = 0;
  ans->nb_symbols_ = -1;
  ans->nb_uvalues_ = 0;
  ans->nb_bits_ = 0;
  ResetDictionaries(ans);
}

// Reset the bistream, the message and free old pages.
void ANSEncClear(ANSEnc* const ans) {
  ANSEncReset(ans, 1);
  ans->state_ = ANS_SIGNATURE;
  while (ans->list_ != NULL) {
    ANSTokenPage* const next = (ANSTokenPage*)ans->list_->prev_;
    WebPSafeFree((void*)ans->list_);
    ans->list_ = next;
  }
  ClearDictionaries(ans);
}

//------------------------------------------------------------------------------
// Token pages management

static int NewPage(ANSEnc* const ans) {
  ANSTokenPage* page = ans->list_;
  if (page != NULL) {
    ans->list_ = (ANSTokenPage*)page->prev_;
  } else {
    page = (ANSTokenPage*)WebPSafeMalloc(1ull, sizeof(*page));
    if (page == NULL) {
      ans->error_ = 1;
      return 0;
    }
  }
  page->prev_ = ans->tokens_;
  page->nb_tokens_ = 0;
  ans->tokens_ = page;
  return 1;
}

void ANSEnqueueToken(ANSEnc* const ans, ANSToken tok) {
  ANSTokenPage* page = ans->tokens_;
  if (page == NULL || page->nb_tokens_ == ANS_MAX_TOKEN_PER_PAGE) {
    if (!NewPage(ans)) return;
    page = ans->tokens_;
  }
  page->tokens_[page->nb_tokens_++] = tok;
  ++ans->nb_tokens_;
}

//------------------------------------------------------------------------------

int ANSDictionaryQuantize(ANSEncDictionary* const dict) {
  uint32_t i;
  uint32_t max_value = 0;
  if (dict == NULL || dict->total_ < 1) {
    return -1;
  }
  if (dict->max_freq_ < 1) {
    return dict->max_symbol_;   // nothing to do.
  }
  for (i = 0; i < dict->max_symbol_; ++i) {
    if (i == 0 || dict->counts_[i] > max_value) {
      max_value = dict->counts_[i];
    }
  }
  if (max_value > 0 && dict->max_freq_ != max_value) {
    const double norm = 1. * dict->max_freq_ / max_value;
    dict->total_ = 0;
    for (i = 0; i < dict->max_symbol_; ++i) {
      if (dict->counts_[i] > 0) {
        const int new_count = (int)(dict->counts_[i] * norm + .5);
        dict->counts_[i] = (new_count < 1) ? 1 : new_count;
        dict->total_ += dict->counts_[i];
      }
    }
  }
  return dict->max_symbol_;
}

#else

void ANSInit(void) {}

#endif   // WEBP_EXPERIMENTAL_FEATURES
