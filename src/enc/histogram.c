// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Author: Jyrki Alakuijala (jyrki@google.com)
//


#include <math.h>
#include <stdio.h>

#include "./backward_references.h"
#include "./histogram.h"

// A lookup table for small values of log(int) to be used in entropy
// computation.
//
// ", ".join(["%.16ff" % x for x in [0.0]+[log(x) for x in range(1, 256)]])
static const float kLogTable[] = {
  0.0000000000000000f, 0.0000000000000000f, 0.6931471805599453f,
  1.0986122886681098f, 1.3862943611198906f, 1.6094379124341003f,
  1.7917594692280550f, 1.9459101490553132f, 2.0794415416798357f,
  2.1972245773362196f, 2.3025850929940459f, 2.3978952727983707f,
  2.4849066497880004f, 2.5649493574615367f, 2.6390573296152584f,
  2.7080502011022101f, 2.7725887222397811f, 2.8332133440562162f,
  2.8903717578961645f, 2.9444389791664403f, 2.9957322735539909f,
  3.0445224377234230f, 3.0910424533583161f, 3.1354942159291497f,
  3.1780538303479458f, 3.2188758248682006f, 3.2580965380214821f,
  3.2958368660043291f, 3.3322045101752038f, 3.3672958299864741f,
  3.4011973816621555f, 3.4339872044851463f, 3.4657359027997265f,
  3.4965075614664802f, 3.5263605246161616f, 3.5553480614894135f,
  3.5835189384561099f, 3.6109179126442243f, 3.6375861597263857f,
  3.6635616461296463f, 3.6888794541139363f, 3.7135720667043080f,
  3.7376696182833684f, 3.7612001156935624f, 3.7841896339182610f,
  3.8066624897703196f, 3.8286413964890951f, 3.8501476017100584f,
  3.8712010109078911f, 3.8918202981106265f, 3.9120230054281460f,
  3.9318256327243257f, 3.9512437185814275f, 3.9702919135521220f,
  3.9889840465642745f, 4.0073331852324712f, 4.0253516907351496f,
  4.0430512678345503f, 4.0604430105464191f, 4.0775374439057197f,
  4.0943445622221004f, 4.1108738641733114f, 4.1271343850450917f,
  4.1431347263915326f, 4.1588830833596715f, 4.1743872698956368f,
  4.1896547420264252f, 4.2046926193909657f, 4.2195077051761070f,
  4.2341065045972597f, 4.2484952420493594f, 4.2626798770413155f,
  4.2766661190160553f, 4.2904594411483910f, 4.3040650932041702f,
  4.3174881135363101f, 4.3307333402863311f, 4.3438054218536841f,
  4.3567088266895917f, 4.3694478524670215f, 4.3820266346738812f,
  4.3944491546724391f, 4.4067192472642533f, 4.4188406077965983f,
  4.4308167988433134f, 4.4426512564903167f, 4.4543472962535073f,
  4.4659081186545837f, 4.4773368144782069f, 4.4886363697321396f,
  4.4998096703302650f, 4.5108595065168497f, 4.5217885770490405f,
  4.5325994931532563f, 4.5432947822700038f, 4.5538768916005408f,
  4.5643481914678361f, 4.5747109785033828f, 4.5849674786705723f,
  4.5951198501345898f, 4.6051701859880918f, 4.6151205168412597f,
  4.6249728132842707f, 4.6347289882296359f, 4.6443908991413725f,
  4.6539603501575231f, 4.6634390941120669f, 4.6728288344619058f,
  4.6821312271242199f, 4.6913478822291435f, 4.7004803657924166f,
  4.7095302013123339f, 4.7184988712950942f, 4.7273878187123408f,
  4.7361984483944957f, 4.7449321283632502f, 4.7535901911063645f,
  4.7621739347977563f, 4.7706846244656651f, 4.7791234931115296f,
  4.7874917427820458f, 4.7957905455967413f, 4.8040210447332568f,
  4.8121843553724171f, 4.8202815656050371f, 4.8283137373023015f,
  4.8362819069514780f, 4.8441870864585912f, 4.8520302639196169f,
  4.8598124043616719f, 4.8675344504555822f, 4.8751973232011512f,
  4.8828019225863706f, 4.8903491282217537f, 4.8978397999509111f,
  4.9052747784384296f, 4.9126548857360524f, 4.9199809258281251f,
  4.9272536851572051f, 4.9344739331306915f, 4.9416424226093039f,
  4.9487598903781684f, 4.9558270576012609f, 4.9628446302599070f,
  4.9698132995760007f, 4.9767337424205742f, 4.9836066217083363f,
  4.9904325867787360f, 4.9972122737641147f, 5.0039463059454592f,
  5.0106352940962555f, 5.0172798368149243f, 5.0238805208462765f,
  5.0304379213924353f, 5.0369526024136295f, 5.0434251169192468f,
  5.0498560072495371f, 5.0562458053483077f, 5.0625950330269669f,
  5.0689042022202315f, 5.0751738152338266f, 5.0814043649844631f,
  5.0875963352323836f, 5.0937502008067623f, 5.0998664278241987f,
  5.1059454739005803f, 5.1119877883565437f, 5.1179938124167554f,
  5.1239639794032588f, 5.1298987149230735f, 5.1357984370502621f,
  5.1416635565026603f, 5.1474944768134527f, 5.1532915944977793f,
  5.1590552992145291f, 5.1647859739235145f, 5.1704839950381514f,
  5.1761497325738288f, 5.1817835502920850f, 5.1873858058407549f,
  5.1929568508902104f, 5.1984970312658261f, 5.2040066870767951f,
  5.2094861528414214f, 5.2149357576089859f, 5.2203558250783244f,
  5.2257466737132017f, 5.2311086168545868f, 5.2364419628299492f,
  5.2417470150596426f, 5.2470240721604862f, 5.2522734280466299f,
  5.2574953720277815f, 5.2626901889048856f, 5.2678581590633282f,
  5.2729995585637468f, 5.2781146592305168f, 5.2832037287379885f,
  5.2882670306945352f, 5.2933048247244923f, 5.2983173665480363f,
  5.3033049080590757f, 5.3082676974012051f, 5.3132059790417872f,
  5.3181199938442161f, 5.3230099791384085f, 5.3278761687895813f,
  5.3327187932653688f, 5.3375380797013179f, 5.3423342519648109f,
  5.3471075307174685f, 5.3518581334760666f, 5.3565862746720123f,
  5.3612921657094255f, 5.3659760150218512f, 5.3706380281276624f,
  5.3752784076841653f, 5.3798973535404597f, 5.3844950627890888f,
  5.3890717298165010f, 5.3936275463523620f, 5.3981627015177525f,
  5.4026773818722793f, 5.4071717714601188f, 5.4116460518550396f,
  5.4161004022044201f, 5.4205349992722862f, 5.4249500174814029f,
  5.4293456289544411f, 5.4337220035542400f, 5.4380793089231956f,
  5.4424177105217932f, 5.4467373716663099f, 5.4510384535657002f,
  5.4553211153577017f, 5.4595855141441589f, 5.4638318050256105f,
  5.4680601411351315f, 5.4722706736714750f, 5.4764635519315110f,
  5.4806389233419912f, 5.4847969334906548f, 5.4889377261566867f,
  5.4930614433405482f, 5.4971682252932021f, 5.5012582105447274f,
  5.5053315359323625f, 5.5093883366279774f, 5.5134287461649825f,
  5.5174528964647074f, 5.5214609178622460f, 5.5254529391317835f,
  5.5294290875114234f, 5.5333894887275203f, 5.5373342670185366f,
  5.5412635451584258f,
};

// Faster logarithm for small integers, with the property of log(0) == 0.
static WEBP_INLINE double FastLog(int v) {
  if (v < (int)(sizeof(kLogTable) / sizeof(kLogTable[0]))) {
    return kLogTable[v];
  }
  return log(v);
}

void ConvertPopulationCountTableToBitEstimates(
    int num_symbols,
    const int* const population_counts,
    double* const output) {
  int sum = 0;
  int nonzeros = 0;
  int i;
  for (i = 0; i < num_symbols; ++i) {
    sum += population_counts[i];
    if (population_counts[i] > 0) {
      ++nonzeros;
    }
  }
  if (nonzeros <= 1) {
    memset(output, 0, num_symbols * sizeof(*output));
    return;
  }
  {
    const double log2sum = log2(sum);
    for (i = 0; i < num_symbols; ++i) {
      if (population_counts[i] == 0) {
        output[i] = log2sum;
      } else {
        output[i] = log2sum - log2(population_counts[i]);
      }
    }
  }
}

void HistogramAddSinglePixOrCopy(Histogram* const p, const PixOrCopy v) {
  if (PixOrCopyIsLiteral(&v)) {
    ++p->alpha_[PixOrCopyLiteral(&v, 3)];
    ++p->red_[PixOrCopyLiteral(&v, 2)];
    ++p->literal_[PixOrCopyLiteral(&v, 1)];
    ++p->blue_[PixOrCopyLiteral(&v, 0)];
  } else if (PixOrCopyIsPaletteIx(&v)) {
    int literal_ix = 256 + kLengthCodes + PixOrCopyPaletteIx(&v);
    ++p->literal_[literal_ix];
  } else {
    int code, extra_bits_count, extra_bits_value;
    PrefixEncode(PixOrCopyLength(&v),
                 &code, &extra_bits_count, &extra_bits_value);
    ++p->literal_[256 + code];
    PrefixEncode(PixOrCopyDistance(&v),
                 &code, &extra_bits_count, &extra_bits_value);
    ++p->distance_[code];
  }
}

void HistogramBuild(Histogram* const p,
                    const PixOrCopy* const literal_and_length,
                    int n_literal_and_length) {
  int i;
  HistogramClear(p);
  for (i = 0; i < n_literal_and_length; ++i) {
    HistogramAddSinglePixOrCopy(p, literal_and_length[i]);
  }
}

double ShannonEntropy(const int* const array, int n) {
  int i;
  double retval = 0;
  int sum = 0;
  for (i = 0; i < n; ++i) {
    if (array[i] != 0) {
      sum += array[i];
      retval += array[i] * FastLog(array[i]);
    }
  }
  retval -= sum * FastLog(sum);
  retval *= -1.4426950408889634;  // 1.0 / -FastLog(2);
  return retval;
}

static double BitsEntropy(const int* const array, int n) {
  double retval = 0;
  int sum = 0;
  int nonzeros = 0;
  int max_val = 0;
  int i;
  double mix;
  for (i = 0; i < n; ++i) {
    if (array[i] != 0) {
      sum += array[i];
      ++nonzeros;
      retval += array[i] * FastLog(array[i]);
      if (max_val < array[i]) {
        max_val = array[i];
      }
    }
  }
  retval -= sum * FastLog(sum);
  retval *= -1.4426950408889634;  // 1.0 / -FastLog(2);
  mix = 0.627;
  if (nonzeros < 5) {
    if (nonzeros <= 1) {
      return 0;
    }
    // Two symbols, they will be 0 and 1 in a Huffman code.
    // Let's mix in a bit of entropy to favor good clustering when
    // distributions of these are combined.
    if (nonzeros == 2) {
      return 0.99 * sum + 0.01 * retval;
    }
    // No matter what the entropy says, we cannot be better than min_limit
    // with Huffman coding. I am mixing a bit of entropy into the
    // min_limit since it produces much better (~0.5 %) compression results
    // perhaps because of better entropy clustering.
    if (nonzeros == 3) {
      mix = 0.95;
    } else {
      mix = 0.7;  // nonzeros == 4.
    }
  }
  {
    double min_limit = 2 * sum - max_val;
    min_limit = mix * min_limit + (1.0 - mix) * retval;
    if (retval < min_limit) {
      return min_limit;
    }
  }
  return retval;
}

double HistogramEstimateBitsBulk(const Histogram* const p) {
  double retval = BitsEntropy(&p->literal_[0], HistogramNumPixOrCopyCodes(p)) +
      BitsEntropy(&p->red_[0], 256) +
      BitsEntropy(&p->blue_[0], 256) +
      BitsEntropy(&p->alpha_[0], 256) +
      BitsEntropy(&p->distance_[0], DISTANCE_CODES_MAX);
  // Compute the extra bits cost.
  size_t i;
  for (i = 2; i < kLengthCodes - 2; ++i) {
    retval +=
        (i >> 1) * p->literal_[256 + i + 2];
  }
  for (i = 2; i < DISTANCE_CODES_MAX - 2; ++i) {
    retval += (i >> 1) * p->distance_[i + 2];
  }
  return retval;
}

double HistogramEstimateBits(const Histogram* const p) {
  return HistogramEstimateBitsHeader(p) + HistogramEstimateBitsBulk(p);
}

// Returns the cost encode the rle-encoded entropy code.
// The constants in this function are experimental.
static double HuffmanCost(const int* const population, int length) {
  // Small bias because Huffman code length is typically not stored in
  // full length.
  static const int kHuffmanCodeOfHuffmanCodeSize = CODE_LENGTH_CODES * 3;
  static const double kSmallBias = 9.1;
  double retval = kHuffmanCodeOfHuffmanCodeSize - kSmallBias;
  int streak = 0;
  int i = 0;
  for (; i < length - 1; ++i) {
    ++streak;
    if (population[i] == population[i + 1]) {
      continue;
    }
 last_streak_hack:
    // population[i] points now to the symbol in the streak of same values.
    if (streak > 3) {
      if (population[i] == 0) {
        retval += 1.5625 + 0.234375 * streak;
      } else {
        retval += 2.578125 + 0.703125 * streak;
      }
    } else {
      if (population[i] == 0) {
        retval += 1.796875 * streak;
      } else {
        retval += 3.28125 * streak;
      }
    }
    streak = 0;
  }
  if (i == length - 1) {
    ++streak;
    goto last_streak_hack;
  }
  return retval;
}

double HistogramEstimateBitsHeader(const Histogram* const p) {
  return HuffmanCost(&p->alpha_[0], 256) +
      HuffmanCost(&p->red_[0], 256) +
      HuffmanCost(&p->literal_[0], HistogramNumPixOrCopyCodes(p)) +
      HuffmanCost(&p->blue_[0], 256) +
      HuffmanCost(&p->distance_[0], DISTANCE_CODES_MAX);
}

int BuildHistogramImage(int xsize, int ysize,
                        int histobits,
                        int palettebits,
                        const PixOrCopy* backward_refs,
                        int backward_refs_size,
                        Histogram*** image_arg,
                        int* image_size) {
  int histo_xsize = histobits ? (xsize + (1 << histobits) - 1) >> histobits : 1;
  int histo_ysize = histobits ? (ysize + (1 << histobits) - 1) >> histobits : 1;
  int i;
  int x = 0;
  int y = 0;
  Histogram** image;
  *image_arg = NULL;
  *image_size = histo_xsize * histo_ysize;
  image = (Histogram**)calloc(*image_size, sizeof(*image));
  if (image == NULL) {
    return 0;
  }
  for (i = 0; i < *image_size; ++i) {
    image[i] = (Histogram*)malloc(sizeof(*image[i]));
    if (!image[i]) {
      int k;
      for (k = 0; k < *image_size; ++k) {
        free(image[k]);
      }
      free(image);
      return 0;
    }
    HistogramInit(image[i], palettebits);
  }
  // x and y trace the position in the image.
  for (i = 0; i < backward_refs_size; ++i) {
    const PixOrCopy v = backward_refs[i];
    const int ix =
        histobits ? (y >> histobits) * histo_xsize + (x >> histobits) : 0;
    HistogramAddSinglePixOrCopy(image[ix], v);
    x += PixOrCopyLength(&v);
    while (x >= xsize) {
      x -= xsize;
      ++y;
    }
  }
  *image_arg = image;
  return 1;
}

int CombineHistogramImage(Histogram** in,
                          int in_size,
                          int quality,
                          Histogram*** out_arg,
                          int* out_size) {
  int ok = 0;
  int i;
  unsigned int seed = 0;
  int tries_with_no_success = 0;
  int inner_iters = 10 + quality / 2;
  int iter;
  double* bit_costs = (double*)malloc(in_size * sizeof(*bit_costs));
  Histogram** out = (Histogram**)calloc(in_size, sizeof(*out));
  *out_arg = out;
  *out_size = in_size;
  if (bit_costs == NULL || out == NULL) {
    goto Error;
  }
  // Copy
  for (i = 0; i < in_size; ++i) {
    Histogram* new_histo = (Histogram*)malloc(sizeof(*new_histo));
    if (new_histo == NULL) {
      goto Error;
    }
    *new_histo = *(in[i]);
    out[i] = new_histo;
    bit_costs[i] = HistogramEstimateBits(out[i]);
  }
  // Collapse similar histograms.
  for (iter = 0; iter < in_size * 3 && *out_size >= 2; ++iter) {
    double best_val = 0;
    int best_ix0 = 0;
    int best_ix1 = 0;
    // Try a few times.
    int k;
    for (k = 0; k < inner_iters; ++k) {
      // Choose two, build a combo out of them.
      double cost_val;
      Histogram* combo;
      int ix0 = rand_r(&seed) % *out_size;
      int ix1;
      int diff = ((k & 7) + 1) % (*out_size - 1);
      if (diff >= 3) {
        diff = rand_r(&seed) % (*out_size - 1);
      }
      ix1 = (ix0 + diff + 1) % *out_size;
      if (ix0 == ix1) {
        continue;
      }
      combo = (Histogram*)malloc(sizeof(*combo));
      if (combo == NULL) {
        goto Error;
      }
      *combo = *out[ix0];
      HistogramAdd(combo, out[ix1]);
      cost_val = HistogramEstimateBits(combo) - bit_costs[ix0] - bit_costs[ix1];
      if (best_val > cost_val) {
        best_val = cost_val;
        best_ix0 = ix0;
        best_ix1 = ix1;
      }
      free(combo);
    }
    if (best_val < 0.0) {
      HistogramAdd(out[best_ix0], out[best_ix1]);
      bit_costs[best_ix0] =
          best_val + bit_costs[best_ix0] + bit_costs[best_ix1];
      // Erase (*out)[best_ix1]
      free(out[best_ix1]);
      memmove(&out[best_ix1], &out[best_ix1 + 1],
              (*out_size - best_ix1 - 1) * sizeof(out[0]));
      memmove(&bit_costs[best_ix1], &bit_costs[best_ix1 + 1],
              (*out_size - best_ix1 - 1) * sizeof(bit_costs[0]));
      --(*out_size);
      tries_with_no_success = 0;
    }
    if (++tries_with_no_success >= 50) {
      break;
    }
  }
  ok = 1;
Error:
  free(bit_costs);
  if (!ok) {
    if (out) {
      int i;
      for (i = 0; i < *out_size; ++i) {
        free(out[i]);
      }
      free(out);
    }
  }
  return ok;
}

// What is the bit cost of moving square_histogram from
// cur_symbol to candidate_symbol.
static double HistogramDistance(const Histogram* const square_histogram,
                                int cur_symbol,
                                int candidate_symbol,
                                Histogram** candidate_histograms) {
  double new_bit_cost;
  double previous_bit_cost;
  Histogram modified;
  if (cur_symbol == candidate_symbol) {
    return 0;  // Going nowhere. No savings.
  }
  previous_bit_cost =
      HistogramEstimateBits(candidate_histograms[candidate_symbol]);
  if (cur_symbol != -1) {
    previous_bit_cost +=
        HistogramEstimateBits(candidate_histograms[cur_symbol]);
  }

  // Compute the bit cost of the histogram where the data moves to.
  modified = *candidate_histograms[candidate_symbol];
  HistogramAdd(&modified, square_histogram);
  new_bit_cost = HistogramEstimateBits(&modified);

  // Compute the bit cost of the histogram where the data moves away.
  if (cur_symbol != -1) {
    modified = *candidate_histograms[cur_symbol];
    HistogramRemove(&modified, square_histogram);
    new_bit_cost += HistogramEstimateBits(&modified);
  }
  return new_bit_cost - previous_bit_cost;
}

void RefineHistogramImage(Histogram** raw,
                          int raw_size,
                          uint32_t* symbols,
                          int out_size,
                          Histogram** out) {
  int i;
  // Find the best 'out' histogram for each of the raw histograms
  for (i = 0; i < raw_size; ++i) {
    int best_out = 0;
    double best_bits = HistogramDistance(raw[i], symbols[i], 0, out);
    int k;
    for (k = 1; k < out_size; ++k) {
      double cur_bits = HistogramDistance(raw[i], symbols[i], k, out);
      if (cur_bits < best_bits) {
        best_bits = cur_bits;
        best_out = k;
      }
    }
    symbols[i] = best_out;
  }

  // Recompute each out based on raw and symbols.
  for (i = 0; i < out_size; ++i) {
    HistogramClear(out[i]);
  }
  for (i = 0; i < raw_size; ++i) {
    HistogramAdd(out[symbols[i]], raw[i]);
  }
}
