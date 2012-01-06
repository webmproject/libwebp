// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Internal header for tree-coder
//
// Author: Skal (pascal.massimino@gmail.com)
//

#ifndef WEBP_UTILS_TCODERI_H_
#define WEBP_UTILS_TCODERI_H_

#include "./tcoder.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/bit_reader.h"
#include "../utils/bit_writer.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef int Symbol_t;
typedef uint32_t Count_t;  // TODO(skal): check overflow during coding.

#define INVALID_SYMBOL ((Symbol_t)(-1))
#define INVALID_POS    0

#define MAX_PROBA 255
#define HALF_PROBA 128

// Limit the number of tree updates above which we freeze the probabilities.
// Mainly for speed reason.
// TODO(skal): could be a bitstream parameter?
#define COUNTER_CUT_OFF  16383

typedef struct {        // ternary node.
  Symbol_t symbol_;
  // Note: theoretically, one of this three field is redundant and could be
  // omitted, but it'd make the code quite complicated (having to look-up the
  // parent's total count in order to deduce the missing field). Better not.
  Count_t countS_;    // count for symbol
  Count_t count_;     // count for non-symbol (derived from sub-tree)
  int probaL_;        // cached left proba = TotalCount(left) / count_
  int probaS_;        // cached approximate proba = countS_ / TotalCount
} Node;

struct TCoder {
  // dynamic fields:
  int num_symbols_;       // number of symbols actually used
  Count_t total_coded_;   // total number of coded symbols
  int frozen_;            // if true, frequencies are not updated
  int fixed_symbols_;     // if true, symbols are not updated

  // constants:
  int num_nodes_;            // max number of symbols or nodes. Constant, > 0.
  double symbol_bit_cost_;   // latest evaluation of the bit-cost per new symbol

  Node* nodes_;              // nodes (1-based indexed)
  int* symbols_;             // for each symbol, location of its node
};

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  // WEBP_UTILS_TCODERI_H_
