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

#include "./huffman_encode.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int total_count_;
  int value_;
  int pool_index_left_;
  int pool_index_right_;
} HuffmanTree;

// Sort the root nodes, most popular first.
static int CompHuffmanTree(const void* vp0, const void* vp1) {
  const HuffmanTree* v0 = (const HuffmanTree*)vp0;
  const HuffmanTree* v1 = (const HuffmanTree*)vp1;
  if (v0->total_count_ > v1->total_count_) {
    return -1;
  } else if (v0->total_count_ < v1->total_count_) {
    return 1;
  } else {
    if (v0->value_ < v1->value_) {
      return -1;
    }
    if (v0->value_ > v1->value_) {
      return 1;
    }
    return 0;
  }
}

static void SetDepth(const HuffmanTree* p,
                     HuffmanTree* pool,
                     uint8_t* depth,
                     const int level) {
  if (p->pool_index_left_ >= 0) {
    SetDepth(&pool[p->pool_index_left_], pool, depth, level + 1);
    SetDepth(&pool[p->pool_index_right_], pool, depth, level + 1);
  } else {
    depth[p->value_] = level;
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
int CreateHuffmanTree(const int* const histogram, int histogram_size,
                      int tree_depth_limit,
                      uint8_t* const bit_depths) {
  HuffmanTree* tree;
  HuffmanTree* tree_pool;
  int tree_pool_size;
  // For block sizes with less than 64k symbols we never need to do a
  // second iteration of this loop.
  // If we actually start running inside this loop a lot, we would perhaps
  // be better off with the Katajainen algorithm.
  int count_limit;
  for (count_limit = 1; ; count_limit *= 2) {
    int tree_size = 0;
    int i;
    for (i = 0; i < histogram_size; ++i) {
      if (histogram[i]) {
        ++tree_size;
      }
    }
    // 3 * tree_size is enough to cover all the nodes representing a
    // population and all the inserted nodes combining two existing nodes.
    // The tree pool needs 2 * (tree_size - 1) entities, and the
    // tree needs exactly tree_size entities.
    tree = (HuffmanTree*)malloc(3 * tree_size * sizeof(*tree));
    if (tree == NULL) {
      return 0;
    }
    {
      int j = 0;
      int i;
      for (i = 0; i < histogram_size; ++i) {
        if (histogram[i]) {
          const int count =
              (histogram[i] < count_limit) ? count_limit : histogram[i];
          tree[j].total_count_ = count;
          tree[j].value_ = i;
          tree[j].pool_index_left_ = -1;
          tree[j].pool_index_right_ = -1;
          ++j;
        }
      }
    }
    qsort((void*)tree, tree_size, sizeof(*tree), CompHuffmanTree);
    tree_pool = tree + tree_size;
    tree_pool_size = 0;
    if (tree_size >= 2) {
      while (tree_size >= 2) {  // Finish when we have only one root.
        int count;
        tree_pool[tree_pool_size] = tree[tree_size - 1];
        ++tree_pool_size;
        tree_pool[tree_pool_size] = tree[tree_size - 2];
        ++tree_pool_size;
        count =
            tree_pool[tree_pool_size - 1].total_count_ +
            tree_pool[tree_pool_size - 2].total_count_;
        tree_size -= 2;
        {
          int k = 0;
          // Search for the insertion point.
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
      SetDepth(&tree[0], tree_pool, bit_depths, 0);
    } else {
      if (tree_size == 1) {
        // Only one element.
        bit_depths[tree[0].value_] = 1;
      }
    }
    free(tree);
    // We need to pack the Huffman tree in tree_depth_limit bits.
    // If this was not successful, add fake entities to the lowest values
    // and retry.
    {
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

void CreateCompressedHuffmanTree(const uint8_t* depth,
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

void ConvertBitDepthsToSymbols(const uint8_t* depth, int len,
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
