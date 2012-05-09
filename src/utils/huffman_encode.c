// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Author: jyrki@google.com (Jyrki Alakuijala)
//
// Flate like entropy encoding (Huffman) for webp lossless.

#ifdef USE_LOSSLESS_ENCODER

#include "./huffman_encode.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int total_count_;
  int value_;
  int pool_index_left_;
  int pool_index_right_;
} HuffmanTree;

// A comparer function for two Huffman trees: sorts first by 'total count'
// (more comes first), and then by 'value' (more comes first).
static int CompareHuffmanTrees(const void* ptr1, const void* ptr2) {
  const HuffmanTree* const t1 = (const HuffmanTree*)ptr1;
  const HuffmanTree* const t2 = (const HuffmanTree*)ptr2;
  if (t1->total_count_ > t2->total_count_) {
    return -1;
  } else if (t1->total_count_ < t2->total_count_) {
    return 1;
  } else {
    if (t1->value_ < t2->value_) {
      return -1;
    }
    if (t1->value_ > t2->value_) {
      return 1;
    }
    return 0;
  }
}

static void SetBitDepths(const HuffmanTree* const tree,
                         const HuffmanTree* const pool,
                         uint8_t* const bit_depths, int level) {
  if (tree->pool_index_left_ >= 0) {
    SetBitDepths(&pool[tree->pool_index_left_], pool, bit_depths, level + 1);
    SetBitDepths(&pool[tree->pool_index_right_], pool, bit_depths, level + 1);
  } else {
    bit_depths[tree->value_] = level;
  }
}

// This function will create a Huffman tree.
//
// The catch here is that the tree cannot be arbitrarily deep.
// Deflate specifies a maximum depth of 15 bits for "code trees"
// and 7 bits for "code length code trees."
//
// count_limit is the value that is to be faked as the minimum value
// and this minimum value is raised until the tree matches the
// maximum length requirement.
//
// This algorithm is not of excellent performance for very long data blocks,
// especially when population counts are longer than 2**tree_limit, but
// we are not planning to use this with extremely long blocks.
//
// See http://en.wikipedia.org/wiki/Huffman_coding
int VP8LCreateHuffmanTree(const int* const histogram, int histogram_size,
                          int tree_depth_limit, uint8_t* const bit_depths) {
  int count_min;
  HuffmanTree* tree_pool;
  HuffmanTree* tree;
  int tree_size_orig = 0;
    int i;
    for (i = 0; i < histogram_size; ++i) {
    if (histogram[i] != 0) {
      ++tree_size_orig;
      }
    }
    // 3 * tree_size is enough to cover all the nodes representing a
    // population and all the inserted nodes combining two existing nodes.
  // The tree pool needs 2 * (tree_size_orig - 1) entities, and the
  // tree needs exactly tree_size_orig entities.
  tree = (HuffmanTree*)malloc(3 * tree_size_orig * sizeof(*tree));
  if (tree == NULL) return 0;
  tree_pool = tree + tree_size_orig;


  // For block sizes with less than 64k symbols we never need to do a
  // second iteration of this loop.
  // If we actually start running inside this loop a lot, we would perhaps
  // be better off with the Katajainen algorithm.
  assert(tree_size_orig <= (1 << (tree_depth_limit - 1)));
  for (count_min = 1; ; count_min *= 2) {
    int tree_size = tree_size_orig;
    {
      // We need to pack the Huffman tree in tree_depth_limit bits.
      // So, we try by faking histogram entries to be at least 'count_min'.
      int idx = 0;
      int j;
      for (j = 0; j < histogram_size; ++j) {
        if (histogram[j] != 0) {
          const int count =
              (histogram[j] < count_min) ? count_min : histogram[j];
          tree[idx].total_count_ = count;
          tree[idx].value_ = j;
          tree[idx].pool_index_left_ = -1;
          tree[idx].pool_index_right_ = -1;
          ++idx;
        }
      }
    }

    // Build the Huffman tree.
    qsort((void*)tree, tree_size, sizeof(*tree), CompareHuffmanTrees);

    if (tree_size > 1) {  // Normal case.
      int tree_pool_size = 0;
      while (tree_size > 1) {  // Finish when we have only one root.
        int count;
        tree_pool[tree_pool_size++] = tree[tree_size - 1];
        tree_pool[tree_pool_size++] = tree[tree_size - 2];
        count = tree_pool[tree_pool_size - 1].total_count_ +
            tree_pool[tree_pool_size - 2].total_count_;
        tree_size -= 2;
        {
          // Search for the insertion point.
          int k;
          for (k = 0; k < tree_size; ++k) {
            if (tree[k].total_count_ <= count) {
              break;
            }
          }
          memmove(tree + (k + 1), tree + k, (tree_size - k) * sizeof(*tree));
          tree[k].total_count_ = count;
          tree[k].value_ = -1;

          tree[k].pool_index_left_ = tree_pool_size - 1;
          tree[k].pool_index_right_ = tree_pool_size - 2;
          tree_size = tree_size + 1;
        }
      }
      SetBitDepths(&tree[0], tree_pool, bit_depths, 0);
    } else if (tree_size == 1) {  // Trivial case: only one element.
        bit_depths[tree[0].value_] = 1;
      }

    {
      // Test if this Huffman tree satisfies our 'tree_depth_limit' criteria.
      int max_depth = bit_depths[0];
      int j;
      for (j = 1; j < histogram_size; ++j) {
        if (max_depth < bit_depths[j]) {
          max_depth = bit_depths[j];
        }
      }
      if (max_depth <= tree_depth_limit) {
        break;
      }
    }
  }
  free(tree);
  return 1;
}

static void WriteHuffmanTreeRepetitions(
    const int value,
    const int prev_value,
    int repetitions,
    int* num_symbols,
    uint8_t* tree,
    uint8_t* extra_bits_data) {
  if (value != prev_value) {
    tree[*num_symbols] = value;
    extra_bits_data[*num_symbols] = 0;
    ++(*num_symbols);
    --repetitions;
  }
  while (repetitions >= 1) {
    if (repetitions < 3) {
      int i;
      for (i = 0; i < repetitions; ++i) {
        tree[*num_symbols] = value;
        extra_bits_data[*num_symbols] = 0;
        ++(*num_symbols);
      }
      return;
    } else if (repetitions < 7) {
      // 3 to 6 left
      tree[*num_symbols] = 16;
      extra_bits_data[*num_symbols] = repetitions - 3;
      ++(*num_symbols);
      return;
    } else {
      tree[*num_symbols] = 16;
      extra_bits_data[*num_symbols] = 3;
      ++(*num_symbols);
      repetitions -= 6;
    }
  }
}

static void WriteHuffmanTreeRepetitionsZeros(
    const int value,
    int repetitions,
    int* num_symbols,
    uint8_t* tree,
    uint8_t* extra_bits_data) {
  while (repetitions >= 1) {
    if (repetitions < 3) {
      int i;
      for (i = 0; i < repetitions; ++i) {
        tree[*num_symbols] = value;
        extra_bits_data[*num_symbols] = 0;
        ++(*num_symbols);
      }
      return;
    } else if (repetitions < 11) {
      tree[*num_symbols] = 17;
      extra_bits_data[*num_symbols] = repetitions - 3;
      ++(*num_symbols);
      return;
    } else if (repetitions < 139) {
      tree[*num_symbols] = 18;
      extra_bits_data[*num_symbols] = repetitions - 11;
      ++(*num_symbols);
      return;
    } else {
      tree[*num_symbols] = 18;
      extra_bits_data[*num_symbols] = 0x7f;  // 138 repeated 0s
      ++(*num_symbols);
      repetitions -= 138;
    }
  }
}

void VP8LCreateCompressedHuffmanTree(const uint8_t* const depth,
                                     int depth_size,
                                     int* num_symbols,
                                     uint8_t* tree,
                                     uint8_t* extra_bits_data) {
  int prev_value = 8;  // 8 is the initial value for rle.
  int i;
  for (i = 0; i < depth_size;) {
    const int value = depth[i];
    int reps = 1;
    int k;
    for (k = i + 1; k < depth_size && depth[k] == value; ++k) {
      ++reps;
    }
    if (value == 0) {
      WriteHuffmanTreeRepetitionsZeros(value, reps,
                                       num_symbols,
                                       tree, extra_bits_data);
    } else {
      WriteHuffmanTreeRepetitions(value, prev_value, reps,
                                  num_symbols,
                                  tree, extra_bits_data);
      prev_value = value;
    }
    i += reps;
  }
}

static uint32_t ReverseBits(int num_bits, uint32_t bits) {
  uint32_t retval = 0;
  int i;
  for (i = 0; i < num_bits; ++i) {
    retval <<= 1;
    retval |= bits & 1;
    bits >>= 1;
  }
  return retval;
}

void VP8LConvertBitDepthsToSymbols(const uint8_t* depth, int len,
                                   uint16_t* bits) {
  // This function is based on RFC 1951.
  //
  // In deflate, all bit depths are [1..15]
  // 0 bit depth means that the symbol does not exist.

  // 0..15 are values for bits
#define MAX_BITS 16
  uint32_t next_code[MAX_BITS];
  uint32_t bl_count[MAX_BITS] = { 0 };
  int i;
  {
    for (i = 0; i < len; ++i) {
      ++bl_count[depth[i]];
    }
    bl_count[0] = 0;
  }
  next_code[0] = 0;
  {
    int code = 0;
    int bits;
    for (bits = 1; bits < MAX_BITS; ++bits) {
      code = (code + bl_count[bits - 1]) << 1;
      next_code[bits] = code;
    }
  }
  for (i = 0; i < len; ++i) {
    if (depth[i]) {
      bits[i] = ReverseBits(depth[i], next_code[depth[i]]++);
    }
  }
}
#undef MAX_BITS

#endif
