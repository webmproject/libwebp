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
/*
  Sample usage:

    //   ENCODING

    ANSEnc enc;
    ANSEncInit(&enc);
    // set-up some dictionaries (up to 4)
    for (n = 0; n < num_dicts; ++n) {
      dicts[n] = ANSEncAddDictionary(&enc, max_symbols[n]);
      dicts[n].max_freq_ = 256;       // quantization of frequencies
      dicts[n].log2_tab_size_ = 12;   // final coding table size
    }

    for (...) {
      ANSEncPutSymbol(&enc, dicts[d], symbols[i]);
      ..
      ANSEncPutBit(&enc, bit, probability);
      ..
      ANSEncPutABit(&enc, bit, &stats);
      ..
      ANSEncPutUValue(&enc, value, num_bits);
    }

    CHECK(ANSEncAssemble(&enc));
    const uint8_t* const buffer = ANSEncBuffer(&enc);
    const size_t size = ANSEncBufferSize(&enc);
    // serialize-out buffer[size] and each dictionaries' counts[]/max_symbol
    ...

    ANSEncWipeOut(&enc);   // <- frees buffer[] if needed
    CHECK(!enc.error_);


    //   DECODING

    ANSDec dec;
    CHECK(ANSDecInit(&dec, buffer, size));

    ANSSymbolInfo codes[num_dictionaries][tab_size];
    // serialize-in buffer[], size and each counts[]/max_symbol
    for (n = 0; n < num_dicts; ++n) {
      ANSCountsToSpreadTable(counts[n], max_symbol[n], 0, codes[n]);
    }

    for (...) {
      dict = ... // contextual dictionary
      symbol = ANSDecReadSymbol(&dec, codes[dict], log2_tab_size);
      ...
      bit = ANSDecReadBit(&dec, probability);
      ...
      bit = ANSDecReadABit(&dec, &stats);
      ...
      value = ANSDecReadUValue(&dec, num_bits);
    }
    CHECK(ANSDecCheckCRC(&dec));   // optional
    CHECK(!dec.error_);
    ANSDecClear(&dec);
*/

#ifndef WEBP_UTILS_ANS_H_
#define WEBP_UTILS_ANS_H_

#include <assert.h>
#include "../webp/types.h"

#if defined(WEBP_EXPERIMENTAL_FEATURES)

#ifdef __cplusplus
extern "C" {
#endif

#define ANS_LOG_TAB_SIZE 14
#define ANS_TAB_SIZE (1u << ANS_LOG_TAB_SIZE)
#define ANS_TAB_MASK (ANS_TAB_SIZE - 1)
#define ANS_MAX_SYMBOLS 256

#define PROBA_BITS 16
#define PROBA_MAX (1u << PROBA_BITS)
#define PROBA_MASK (PROBA_MAX - 1)

#define IO_BITS 16    // 8 or 16 only
#define IO_BYTES 2    // IO_BITS = 8 << IO_BYTES
#define IO_MASK ((1u << IO_BITS) - 1u)

#define IO_LIMIT_BITS 16
#define IO_LIMIT_LO (1u << IO_LIMIT_BITS)
#define ANS_SIGNATURE (0xf3 * IO_LIMIT_LO)    // Initial state, used as CRC.

//------------------------------------------------------------------------------
// Decoding

typedef struct {   // Symbol information.
  uint16_t offset_;
  uint16_t freq_;
  uint8_t symbol_;
} ANSSymbolInfo;

// Struct for recording binary event and updating probability.
typedef struct {
  uint32_t p0;   // event counter for bit=0
  uint32_t p1;   // for bit=1
} ANSBinSymbol;
// initial state
void ANSBinSymbolInit(ANSBinSymbol* const stats);
// get probability for symbol '1' out of the current state
uint32_t ANSBinSymbolProba(const ANSBinSymbol* const stats);
// update 'stats' observation and returns 'bit'
uint32_t ANSBinSymbolUpdate(ANSBinSymbol* const stats, uint32_t bit);

// Generate a spread table from a set of frequencies. If log2_tab_size is
// non zero, the counts[] distributions is re-normalized such that its sum is
// equal to 1<<log2_tab_size. codes[] is an array of size '1<<log2_tab_size'.
// Return false in case of error (ill-defined counts).
int ANSCountsToSpreadTable(uint32_t counts[], int max_symbol,
                           uint32_t log2_tab_size, ANSSymbolInfo codes[]);

// Structure for holding the decoding state.
typedef struct {
  const uint8_t* buf_;
  size_t pos_;
  size_t max_pos_;
  uint32_t state_;
  int error_;
} ANSDec;

// Creates and initializes a new ANSDec object.
// Returns false if an error occurred.
int ANSDecInit(ANSDec* const ans, const uint8_t* buf, size_t len);

// Decodes a symbol, according to the spread table 'codes'.
uint32_t ANSDecReadSymbol(ANSDec* const ans,
                          const ANSSymbolInfo codes[],
                          uint32_t log2_tab_size);

// Decodes a binary symbol with probability 'proba' in range [0, PROBA_MAX].
uint32_t ANSDecReadBit(ANSDec* const ans, uint32_t proba);

// Decodes an adaptive binary symbol with statistics 'stats'.
// 'stats' is updated upon return.
uint32_t ANSDecReadABit(ANSDec* const ans, ANSBinSymbol* const stats);

// Decodes a uniform value known to be in range [0..1 << bits), where 'bits'
// is in [1, IO_BITS] range.
uint32_t ANSDecReadUValue(ANSDec* const ans, int bits);

// If the bitstream has been correctly decoded, this function should
// return true.
static WEBP_INLINE int ANSDecCheckCRC(const ANSDec* const ans) {
  return !ans->error_ && (ans->state_ == ANS_SIGNATURE);
}

// Reclaim all memory, reset the decoder.
void ANSDecClear(ANSDec* const ans);

//------------------------------------------------------------------------------
// Encoding

// Internal token marking. There are 3 variants of token, for binary symbols,
// uniformly drawn values and arbitrary symbols.
typedef uint32_t ANSToken;
#define ANS_TOK_IS_BIT  (1u << 30)
#define ANS_TOK_IS_SYM  (2u << 30)
#define ANS_TOK_IS_UVAL (3u << 30)
#define ANS_TOK_TYPE    (3u << 30)    // 2bits

#define ANS_TOK_BIT_MASK (1u << 29)
#define ANS_TOK_MASK     (0x1ffff)   // 17 bits for side-info (proba, uval ...)
#define ANS_TOK_BIT(b, p)   (ANS_TOK_IS_BIT | (b) << 29 | (p))
#define ANS_TOK_SYM(s, D)   (ANS_TOK_IS_SYM | ((D) << 16) | ((s) << 0))
#define ANS_TOK_UVAL(v, B)  (ANS_TOK_IS_UVAL | ((v) << 8) | ((B) << 0))

// internal structure for storing tokens in linked pages.
#define ANS_MAX_TOKEN_PER_PAGE 4096
typedef struct ANSTokenPage ANSTokenPage;
struct ANSTokenPage {
  const ANSTokenPage* prev_;   // previous -finished- pages
  ANSToken tokens_[ANS_MAX_TOKEN_PER_PAGE];
  int nb_tokens_;
};

#define ANS_MAX_DICTIONARIES 16
typedef struct {
  uint32_t id_;           // slot in the embedding ANSEnc object
  int stats_ok_;          // true if counts[] is finalized
  uint32_t max_symbol_;
  int nb_symbols_;
  uint32_t total_;        // = sum_i{counts_[i]}
  uint32_t* counts_;
  // the following params must be set before calling ANSAssemble()
  uint32_t max_freq_;       // if > 0, quantization limit for counts[]
  uint32_t log2_tab_size_;  // final expect total of counts[], used for coding
                            // (0="default to ANS_LOG_TAB_SIZE")
                            // Must be in [0,16] range.
  ANSSymbolInfo* infos_;    // used during final coding
} ANSEncDictionary;

// Encoding structure for storing message and generating bitstream.
typedef struct {
  int error_;              // if non-zero, an error occurred.
  uint32_t state_;         // state for the coder
  // output coded stream:
  uint8_t* buffer_;
  size_t   buffer_pos_;
  size_t   max_buffer_size_;
  // stored message, possibly composed of bits, uvalues and symbols:
  ANSTokenPage* tokens_;
  ANSTokenPage* list_;     // page list
  // misc stats:
  int nb_tokens_;          // total number of tokens in the message
  int nb_symbols_;         // if < 0, symbols needs some recomputation.
  int nb_uvalues_;         // number of uniform values stored
  int nb_bits_;            // number of binary symbols stored
  // dictionaries:
  uint32_t nb_dict_;
  ANSEncDictionary dict_[ANS_MAX_DICTIONARIES];
} ANSEnc;

// Initializes a new ANSEnc object.
void ANSEncInit(ANSEnc* const ans);

// Create a new dictionary capable of holding 'max_symbol' symbols.
// Returns NULL in case of error.
ANSEncDictionary* ANSEncAddDictionary(ANSEnc* const ans, int max_symbol);

// Returns a dictionary from id. Returns NULL if non-existing.
ANSEncDictionary* ANSEncGetDictionary(ANSEnc* const ans, uint32_t id);

// Initialize dictionary and allocate memory of holding records of 'max_symbol'.
// The previous memory (if any) is released.
int ANSEncDictionaryInit(ANSEncDictionary* const dict, int max_symbol);

// Deallocate memory and reset the dictionary structure.
void ANSEncDictionaryClear(ANSEncDictionary* const dict);

// Record use of a given symbol.
void ANSDictionaryRecordSymbol(ANSEncDictionary* const dict, uint32_t symbol);

// Re-generate a quantized PDF in counts[], according to dict->max_freq_.
// Returns the max number of symbols, or 0 in case of error.
int ANSDictionaryQuantize(ANSEncDictionary* const dict);

// Collect all the statistics (not necessary, but can be done before
// calling ANSEncAssemble(), in order to access final stats)
int ANSEncCollectStatistics(ANSEnc* const ans);

// Generate a bitstream from the current state (calls ANSEncCollectStatistics()
// if needed).
int ANSEncAssemble(ANSEnc* const ans);

// Deallocate the coded buffer generated by ANSEncAssemble().
// Will not deallocate the token pages / stats / dictionaries.
void ANSEncResetBuffer(ANSEnc* const ans);

// Prepare the dictionary's distribution and retrieve the symbol's info
// needed for decoding in codes[].
int ANSDictionaryToSpreadTable(const ANSEncDictionary* const dict,
                               ANSSymbolInfo codes[]);

// Process a closed dictionary to populate infos_[] prior to coding.
int ANSDictionaryToCodingTable(const ANSEncDictionary* const dict);

// Reclaim all memory, reset the encoder.
// (= reset the bistream, the message and free old pages).
void ANSEncClear(ANSEnc* const ans);

// Reset the stored message (but not the bitstream, if any).
// If delete_pages is true, past message pages are free'd. Otherwise, they are
// marked for re-use in a future message.
void ANSEncReset(ANSEnc* const ans, int delete_pages);

// Store a message token (internal).
void ANSEnqueueToken(ANSEnc* const ans, ANSToken tok);

// Append a binary symbol 'bit' with probability 'proba' to the message.
// The probability is in range [0, PROBA_MAX]. Returns the value of 'bit'.
static WEBP_INLINE uint32_t ANSEncPutBit(ANSEnc* const ans,
                                         uint32_t bit, uint32_t proba) {
  ANSEnqueueToken(ans, ANS_TOK_BIT(bit, proba));
  ++ans->nb_bits_;
  return bit;
}

// Append an adaptive binary symbol 'bit', updating 'stats' afterward.
static WEBP_INLINE uint32_t ANSEncPutABit(ANSEnc* const ans, uint32_t bit,
                                          ANSBinSymbol* const stats) {
  ANSEncPutBit(ans, bit, ANSBinSymbolProba(stats));
  return ANSBinSymbolUpdate(stats, bit);
}

// Append a symbol 'symbol' to the message. Probabilities are optimally
// evaluated at the end of the message, when calling ANSEncAssemble().
// 'symbol' should be in range [0, ANS_MAX_SYMBOLS).
// Returns the symbol.
static WEBP_INLINE uint32_t ANSEncPutSymbol(ANSEnc* const ans,
                                            ANSEncDictionary* const dict,
                                            uint32_t symbol) {
  assert(symbol < dict->max_symbol_);
  ANSEnqueueToken(ans, ANS_TOK_SYM(symbol, dict->id_));
  ans->nb_symbols_ = -1;  // mark the stats as 'need-a-recompute'
  return symbol;
}

// Append a 'value' uniformly distributed in the range [0..1 << bits).
// 'bits' should be in range [1, IO_BITS].
// Returns the value.
static WEBP_INLINE uint32_t ANSEncPutUValue(ANSEnc* const ans,
                                            uint32_t value, int bits) {
  ANSEnqueueToken(ans, ANS_TOK_UVAL(value, bits));
  assert(bits >= 1 && bits <= IO_BITS);
  assert(value < (1u << bits));
  ++ans->nb_uvalues_;
  return value;
}

// Retrieve the coded buffer (after the last call to ANSEncAssemble()).
// This buffer is only valid until the next call to ANSEncAssemble or
// ANSEncWipeOut().
static WEBP_INLINE const uint8_t* ANSEncBuffer(const ANSEnc* const ans) {
  return ans->buffer_ + ans->buffer_pos_;
}

// Retrieve the size of the coded buffer (after the last call to
// ANSEncAssemble()). This size is only valid until the next call to
// ANSEncAssemble() or ANSEncWipeOut().
static WEBP_INLINE size_t ANSEncBufferSize(const ANSEnc* const ans) {
  return ans->max_buffer_size_ - ans->buffer_pos_;
}

// Frees write-buffer memory (in case of error, for instance)
void ANSEncWipeOut(ANSEnc* const ans);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}    // extern "C"
#endif

#else

void ANSInit(void);    // this is just a stub

#endif   // WEBP_EXPERIMENTAL_FEATURES
#endif   // WEBP_UTILS_ANS_H_
