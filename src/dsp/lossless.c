// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Image transforms and color space conversion methods for lossless decoder.
//
// Authors: Vikas Arora (vikaas.arora@gmail.com)
//          Jyrki Alakuijala (jyrki@google.com)
//          Urvang Joshi (urvang@google.com)

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <math.h>
#include <stdlib.h>
#include "./lossless.h"
#include "../dec/vp8li.h"
#include "../dsp/yuv.h"
#include "../dsp/dsp.h"
#include "../enc/histogram.h"

// A lookup table for small values of log(int) to be used in entropy
// computation.
//
// ", ".join(["%.16ff" % x for x in [0.0]+[log(x) for x in range(1, 256)]])
#define LOG_LOOKUP_IDX_MAX 256
static const float kLogTable[LOG_LOOKUP_IDX_MAX] = {
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
  5.5412635451584258f
};

#define APPROX_LOG_MAX  4096
#define LOG_2_BASE_E    0.6931471805599453f

float VP8LFastLog(int v) {
  if (v < APPROX_LOG_MAX) {
    int log_cnt = 0;
    while (v >= LOG_LOOKUP_IDX_MAX) {
      ++log_cnt;
      v = v >> 1;
    }
    return kLogTable[v] + (log_cnt * LOG_2_BASE_E);
  }
  return (float)log(v);
}

//------------------------------------------------------------------------------
// Image transforms.

// In-place sum of each component with mod 256.
static WEBP_INLINE void AddPixelsEq(uint32_t* a, uint32_t b) {
  const uint32_t alpha_and_green = (*a & 0xff00ff00u) + (b & 0xff00ff00u);
  const uint32_t red_and_blue = (*a & 0x00ff00ffu) + (b & 0x00ff00ffu);
  *a = (alpha_and_green & 0xff00ff00u) | (red_and_blue & 0x00ff00ffu);
}

static WEBP_INLINE uint32_t Average2(uint32_t a0, uint32_t a1) {
  return (((a0 ^ a1) & 0xfefefefeL) >> 1) + (a0 & a1);
}

static WEBP_INLINE uint32_t Average3(uint32_t a0, uint32_t a1, uint32_t a2) {
  return Average2(Average2(a0, a2), a1);
}

static WEBP_INLINE uint32_t Average4(uint32_t a0, uint32_t a1,
                                     uint32_t a2, uint32_t a3) {
  return Average2(Average2(a0, a1), Average2(a2, a3));
}

static WEBP_INLINE uint32_t Clip255(uint32_t a) {
  if (a < 256) {
    return a;
  }
  // return 0, when a is a negative integer.
  // return 255, when a is positive.
  return ~a >> 24;
}

static WEBP_INLINE int AddSubtractComponentFull(int a, int b, int c) {
  return Clip255(a + b - c);
}

static WEBP_INLINE uint32_t ClampedAddSubtractFull(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  const int a = AddSubtractComponentFull(c0 >> 24, c1 >> 24, c2 >> 24);
  const int r = AddSubtractComponentFull((c0 >> 16) & 0xff,
                                         (c1 >> 16) & 0xff,
                                         (c2 >> 16) & 0xff);
  const int g = AddSubtractComponentFull((c0 >> 8) & 0xff,
                                         (c1 >> 8) & 0xff,
                                         (c2 >> 8) & 0xff);
  const int b = AddSubtractComponentFull(c0 & 0xff, c1 & 0xff, c2 & 0xff);
  return (a << 24) | (r << 16) | (g << 8) | b;
}

static WEBP_INLINE int AddSubtractComponentHalf(int a, int b) {
  return Clip255(a + (a - b) / 2);
}

static WEBP_INLINE uint32_t ClampedAddSubtractHalf(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  const uint32_t ave = Average2(c0, c1);
  const int a = AddSubtractComponentHalf(ave >> 24, c2 >> 24);
  const int r = AddSubtractComponentHalf((ave >> 16) & 0xff, (c2 >> 16) & 0xff);
  const int g = AddSubtractComponentHalf((ave >> 8) & 0xff, (c2 >> 8) & 0xff);
  const int b = AddSubtractComponentHalf((ave >> 0) & 0xff, (c2 >> 0) & 0xff);
  return (a << 24) | (r << 16) | (g << 8) | b;
}

static WEBP_INLINE int Sub3(int a, int b, int c) {
  const int pa = b - c;
  const int pb = a - c;
  return abs(pa) - abs(pb);
}

static WEBP_INLINE uint32_t Select(uint32_t a, uint32_t b, uint32_t c) {
  const int pa_minus_pb =
      Sub3((a >> 24)       , (b >> 24)       , (c >> 24)       ) +
      Sub3((a >> 16) & 0xff, (b >> 16) & 0xff, (c >> 16) & 0xff) +
      Sub3((a >>  8) & 0xff, (b >>  8) & 0xff, (c >>  8) & 0xff) +
      Sub3((a      ) & 0xff, (b      ) & 0xff, (c      ) & 0xff);

  return (pa_minus_pb <= 0) ? a : b;
}

//------------------------------------------------------------------------------
// Predictors

static uint32_t Predictor0(uint32_t left, const uint32_t* const top) {
  (void)top;
  (void)left;
  return ARGB_BLACK;
}
static uint32_t Predictor1(uint32_t left, const uint32_t* const top) {
  (void)top;
  return left;
}
static uint32_t Predictor2(uint32_t left, const uint32_t* const top) {
  (void)left;
  return top[0];
}
static uint32_t Predictor3(uint32_t left, const uint32_t* const top) {
  (void)left;
  return top[1];
}
static uint32_t Predictor4(uint32_t left, const uint32_t* const top) {
  (void)left;
  return top[-1];
}
static uint32_t Predictor5(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average3(left, top[0], top[1]);
  return pred;
}
static uint32_t Predictor6(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(left, top[-1]);
  return pred;
}
static uint32_t Predictor7(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(left, top[0]);
  return pred;
}
static uint32_t Predictor8(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(top[-1], top[0]);
  (void)left;
  return pred;
}
static uint32_t Predictor9(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average2(top[0], top[1]);
  (void)left;
  return pred;
}
static uint32_t Predictor10(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Average4(left, top[-1], top[0], top[1]);
  return pred;
}
static uint32_t Predictor11(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = Select(top[0], left, top[-1]);
  return pred;
}
static uint32_t Predictor12(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractFull(left, top[0], top[-1]);
  return pred;
}
static uint32_t Predictor13(uint32_t left, const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractHalf(left, top[0], top[-1]);
  return pred;
}

typedef uint32_t (*PredictorFunc)(uint32_t left, const uint32_t* const top);
static const PredictorFunc kPredictors[16] = {
  Predictor0, Predictor1, Predictor2, Predictor3,
  Predictor4, Predictor5, Predictor6, Predictor7,
  Predictor8, Predictor9, Predictor10, Predictor11,
  Predictor12, Predictor13,
  Predictor0, Predictor0    // <- padding security sentinels
};

// TODO(vikasa): Replace 256 etc with defines.
static double PredictionCostSpatial(const int* counts,
                                    int weight_0, double exp_val) {
  const int significant_symbols = 16;
  const double exp_decay_factor = 0.6;
  double bits = weight_0 * counts[0];
  int i;
  for (i = 1; i < significant_symbols; ++i) {
    bits += exp_val * (counts[i] + counts[256 - i]);
    exp_val *= exp_decay_factor;
  }
  return -0.1 * bits;
}

// Compute the Shanon's entropy: Sum(p*log2(p))
static double ShannonEntropy(const int* const array, int n) {
  int i;
  double retval = 0;
  int sum = 0;
  for (i = 0; i < n; ++i) {
    if (array[i] != 0) {
      sum += array[i];
      retval += array[i] * VP8LFastLog(array[i]);
    }
  }
  retval -= sum * VP8LFastLog(sum);
  retval *= -1.4426950408889634;  // 1.0 / -FastLog(2);
  return retval;
}

static double PredictionCostSpatialHistogram(int accumulated[4][256],
                                             int tile[4][256]) {
  int i;
  int k;
  int combo[256];
  double retval = 0;
  for (i = 0; i < 4; ++i) {
    const double exp_val = 0.94;
    retval += PredictionCostSpatial(&tile[i][0], 1, exp_val);
    retval += ShannonEntropy(&tile[i][0], 256);
    for (k = 0; k < 256; ++k) {
      combo[k] = accumulated[i][k] + tile[i][k];
    }
    retval += ShannonEntropy(&combo[0], 256);
  }
  return retval;
}

static int GetBestPredictorForTile(int width, int height,
                                   int tile_x, int tile_y, int bits,
                                   int accumulated[4][256],
                                   const uint32_t* const argb_scratch) {
  const int kNumPredModes = 14;
  const int col_start = tile_x << bits;
  const int row_start = tile_y << bits;
  const int tile_size = 1 << bits;
  const int ymax = (tile_size <= height - row_start) ?
      tile_size : height - row_start;
  const int xmax = (tile_size <= width - col_start) ?
      tile_size : width - col_start;
  int histo[4][256];
  double best_diff = 1e99;
  int best_mode = 0;

  int mode;
  for (mode = 0; mode < kNumPredModes; ++mode) {
    const uint32_t* current_row = argb_scratch;
    const PredictorFunc pred_func = kPredictors[mode];
    double cur_diff;
    int y;
    memset(&histo[0][0], 0, sizeof(histo));
    for (y = 0; y < ymax; ++y) {
      int x;
      const int row = row_start + y;
      const uint32_t* const upper_row = current_row;
      current_row = upper_row + width;
      for (x = 0; x < xmax; ++x) {
        const int col = col_start + x;
        uint32_t predict;
        uint32_t predict_diff;
        if (row == 0) {
          predict = (col == 0) ? ARGB_BLACK : current_row[col - 1];  // Left.
        } else if (col == 0) {
          predict = upper_row[col];  // Top.
        } else {
          predict = pred_func(current_row[col - 1], upper_row + col);
        }
        predict_diff = VP8LSubPixels(current_row[col], predict);
        ++histo[0][predict_diff >> 24];
        ++histo[1][((predict_diff >> 16) & 0xff)];
        ++histo[2][((predict_diff >> 8) & 0xff)];
        ++histo[3][(predict_diff & 0xff)];
      }
    }
    cur_diff = PredictionCostSpatialHistogram(accumulated, histo);
    if (cur_diff < best_diff) {
      best_diff = cur_diff;
      best_mode = mode;
    }
  }

  return best_mode;
}

static void CopyTileWithPrediction(int width, int height,
                                   int tile_x, int tile_y, int bits, int mode,
                                   const uint32_t* const argb_scratch,
                                   uint32_t* const argb) {
  const int col_start = tile_x << bits;
  const int row_start = tile_y << bits;
  const int tile_size = 1 << bits;
  const int ymax = (tile_size <= height - row_start) ?
      tile_size : height - row_start;
  const int xmax = (tile_size <= width - col_start) ?
      tile_size : width - col_start;
  const PredictorFunc pred_func = kPredictors[mode];
  const uint32_t* current_row = argb_scratch;

  int y;
  for (y = 0; y < ymax; ++y) {
    int x;
    const int row = row_start + y;
    const uint32_t* const upper_row = current_row;
    current_row = upper_row + width;
    for (x = 0; x < xmax; ++x) {
      const int col = col_start + x;
      const int pix = row * width + col;
      uint32_t predict;
      if (row == 0) {
        predict = (col == 0) ? ARGB_BLACK : current_row[col - 1];  // Left.
      } else if (col == 0) {
        predict = upper_row[col];  // Top.
      } else {
        predict = pred_func(current_row[col - 1], upper_row + col);
      }
      argb[pix] = VP8LSubPixels(current_row[col], predict);
    }
  }
}

void VP8LResidualImage(int width, int height, int bits,
                       uint32_t* const argb, uint32_t* const argb_scratch,
                       uint32_t* const image) {
  const int max_tile_size = 1 << bits;
  const int tiles_per_row = VP8LSubSampleSize(width, bits);
  const int tiles_per_col = VP8LSubSampleSize(height, bits);
  uint32_t* const upper_row = argb_scratch;
  uint32_t* const current_tile_rows = argb_scratch + width;
  int tile_y;
  int histo[4][256];
  memset(histo, 0, sizeof(histo));
  for (tile_y = 0; tile_y < tiles_per_col; ++tile_y) {
    const int tile_y_offset = tile_y * max_tile_size;
    const int this_tile_height =
        (tile_y < tiles_per_col - 1) ? max_tile_size : height - tile_y_offset;
    int tile_x;
    if (tile_y > 0) {
      memcpy(upper_row, current_tile_rows + (max_tile_size - 1) * width,
             width * sizeof(*upper_row));
    }
    memcpy(current_tile_rows, &argb[tile_y_offset * width],
           this_tile_height * width * sizeof(*current_tile_rows));
    for (tile_x = 0; tile_x < tiles_per_row; ++tile_x) {
      int pred;
      int y;
      const int tile_x_offset = tile_x * max_tile_size;
      int all_x_max = tile_x_offset + max_tile_size;
      if (all_x_max > width) {
        all_x_max = width;
      }
      pred = GetBestPredictorForTile(width, height, tile_x, tile_y, bits, histo,
                                     argb_scratch);
      image[tile_y * tiles_per_row + tile_x] = 0xff000000u | (pred << 8);
      CopyTileWithPrediction(width, height, tile_x, tile_y, bits, pred,
                             argb_scratch, argb);
      for (y = 0; y < max_tile_size; ++y) {
        int ix;
        int all_x;
        int all_y = tile_y_offset + y;
        if (all_y >= height) {
          break;
        }
        ix = all_y * width + tile_x_offset;
        for (all_x = tile_x_offset; all_x < all_x_max; ++all_x, ++ix) {
          const uint32_t a = argb[ix];
          ++histo[0][a >> 24];
          ++histo[1][((a >> 16) & 0xff)];
          ++histo[2][((a >> 8) & 0xff)];
          ++histo[3][(a & 0xff)];
        }
      }
    }
  }
}

// Inverse prediction.
static void PredictorInverseTransform(const VP8LTransform* const transform,
                                      int y_start, int y_end, uint32_t* data) {
  const int width = transform->xsize_;
  if (y_start == 0) {  // First Row follows the L (mode=1) mode.
    int x;
    const uint32_t pred0 = Predictor0(data[-1], NULL);
    AddPixelsEq(data, pred0);
    for (x = 1; x < width; ++x) {
      const uint32_t pred1 = Predictor1(data[x - 1], NULL);
      AddPixelsEq(data + x, pred1);
    }
    data += width;
    ++y_start;
  }

  {
    int y = y_start;
    const int mask = (1 << transform->bits_) - 1;
    const int tiles_per_row = VP8LSubSampleSize(width, transform->bits_);
    const uint32_t* pred_mode_base =
        transform->data_ + (y >> transform->bits_) * tiles_per_row;

    while (y < y_end) {
      int x;
      const uint32_t pred2 = Predictor2(data[-1], data - width);
      const uint32_t* pred_mode_src = pred_mode_base;
      PredictorFunc pred_func;

      // First pixel follows the T (mode=2) mode.
      AddPixelsEq(data, pred2);

      // .. the rest:
      pred_func = kPredictors[((*pred_mode_src++) >> 8) & 0xf];
      for (x = 1; x < width; ++x) {
        uint32_t pred;
        if ((x & mask) == 0) {    // start of tile. Read predictor function.
          pred_func = kPredictors[((*pred_mode_src++) >> 8) & 0xf];
        }
        pred = pred_func(data[x - 1], data + x - width);
        AddPixelsEq(data + x, pred);
      }
      data += width;
      ++y;
      if ((y & mask) == 0) {   // Use the same mask, since tiles are squares.
        pred_mode_base += tiles_per_row;
      }
    }
  }
}

void VP8LSubtractGreenFromBlueAndRed(uint32_t* argb_data, int num_pixs) {
  int i;
  for (i = 0; i < num_pixs; ++i) {
    const uint32_t argb = argb_data[i];
    const uint32_t green = (argb >> 8) & 0xff;
    const uint32_t new_r = (((argb >> 16) & 0xff) - green) & 0xff;
    const uint32_t new_b = ((argb & 0xff) - green) & 0xff;
    argb_data[i] = (argb & 0xff00ff00) | (new_r << 16) | new_b;
  }
}

// Add green to blue and red channels (i.e. perform the inverse transform of
// 'subtract green').
static void AddGreenToBlueAndRed(const VP8LTransform* const transform,
                                 int y_start, int y_end, uint32_t* data) {
  const int width = transform->xsize_;
  const uint32_t* const data_end = data + (y_end - y_start) * width;
  while (data < data_end) {
    const uint32_t argb = *data;
    // "* 0001001u" is equivalent to "(green << 16) + green)"
    const uint32_t green = ((argb >> 8) & 0xff);
    uint32_t red_blue = (argb & 0x00ff00ffu);
    red_blue += (green << 16) | green;
    red_blue &= 0x00ff00ffu;
    *data++ = (argb & 0xff00ff00u) | red_blue;
  }
}

typedef struct {
  // Note: the members are uint8_t, so that any negative values are
  // automatically converted to "mod 256" values.
  uint8_t green_to_red_;
  uint8_t green_to_blue_;
  uint8_t red_to_blue_;
} Multipliers;

static WEBP_INLINE void MultipliersClear(Multipliers* m) {
  m->green_to_red_ = 0;
  m->green_to_blue_ = 0;
  m->red_to_blue_ = 0;
}

static WEBP_INLINE uint32_t ColorTransformDelta(int8_t color_pred,
                                                int8_t color) {
  return (uint32_t)((int)(color_pred) * color) >> 5;
}

static WEBP_INLINE void ColorCodeToMultipliers(uint32_t color_code,
                                               Multipliers* const m) {
  m->green_to_red_  = (color_code >>  0) & 0xff;
  m->green_to_blue_ = (color_code >>  8) & 0xff;
  m->red_to_blue_   = (color_code >> 16) & 0xff;
}

static WEBP_INLINE uint32_t MultipliersToColorCode(Multipliers* const m) {
  return 0xff000000u |
         ((uint32_t)(m->red_to_blue_) << 16) |
         ((uint32_t)(m->green_to_blue_) << 8) |
         m->green_to_red_;
}

static WEBP_INLINE uint32_t TransformColor(const Multipliers* const m,
                                           uint32_t argb, int inverse) {
  const uint32_t green = argb >> 8;
  const uint32_t red = argb >> 16;
  uint32_t new_red = red;
  uint32_t new_blue = argb;

  if (inverse) {
    new_red += ColorTransformDelta(m->green_to_red_, green);
    new_red &= 0xff;
    new_blue += ColorTransformDelta(m->green_to_blue_, green);
    new_blue += ColorTransformDelta(m->red_to_blue_, new_red);
    new_blue &= 0xff;
  } else {
    new_red -= ColorTransformDelta(m->green_to_red_, green);
    new_red &= 0xff;
    new_blue -= ColorTransformDelta(m->green_to_blue_, green);
    new_blue -= ColorTransformDelta(m->red_to_blue_, red);
    new_blue &= 0xff;
  }
  return (argb & 0xff00ff00u) | (new_red << 16) | (new_blue);
}

static WEBP_INLINE int SkipRepeatedPixels(const uint32_t* const argb,
                                          int ix, int xsize) {
  const uint32_t v = argb[ix];
  if (ix >= xsize + 3) {
    if (v == argb[ix - xsize] &&
        argb[ix - 1] == argb[ix - xsize - 1] &&
        argb[ix - 2] == argb[ix - xsize - 2] &&
        argb[ix - 3] == argb[ix - xsize - 3]) {
      return 1;
    }
    return v == argb[ix - 3] && v == argb[ix - 2] && v == argb[ix - 1];
  } else if (ix >= 3) {
    return v == argb[ix - 3] && v == argb[ix - 2] && v == argb[ix - 1];
  }
  return 0;
}

static double PredictionCostCrossColor(const int accumulated[256],
                                       const int counts[256]) {
  // Favor low entropy, locally and globally.
  int i;
  int combo[256];
  for (i = 0; i < 256; ++i) {
    combo[i] = accumulated[i] + counts[i];
  }
  return ShannonEntropy(combo, 256) +
         ShannonEntropy(counts, 256) +
         PredictionCostSpatial(counts, 3, 2.4);  // Favor small absolute values.
}

static Multipliers GetBestColorTransformForTile(
    int tile_x, int tile_y, int bits,
    Multipliers prevX,
    Multipliers prevY,
    int step, int xsize, int ysize,
    int* accumulated_red_histo,
    int* accumulated_blue_histo,
    const uint32_t* const argb) {
  double best_diff = 1e99;
  double cur_diff;
  const int halfstep = step / 2;
  const int max_tile_size = 1 << bits;
  const int tile_y_offset = tile_y * max_tile_size;
  const int tile_x_offset = tile_x * max_tile_size;
  int green_to_red;
  int green_to_blue;
  int red_to_blue;
  int all_x_max = tile_x_offset + max_tile_size;
  int all_y_max = tile_y_offset + max_tile_size;
  Multipliers best_tx;
  MultipliersClear(&best_tx);
  if (all_x_max > xsize) {
    all_x_max = xsize;
  }
  if (all_y_max > ysize) {
    all_y_max = ysize;
  }
  for (green_to_red = -64; green_to_red <= 64; green_to_red += halfstep) {
    int histo[256] = { 0 };
    int all_y;
    Multipliers tx;
    MultipliersClear(&tx);
    tx.green_to_red_ = green_to_red & 0xff;

    for (all_y = tile_y_offset; all_y < all_y_max; ++all_y) {
      uint32_t predict;
      int ix = all_y * xsize + tile_x_offset;
      int all_x;
      for (all_x = tile_x_offset; all_x < all_x_max; ++all_x, ++ix) {
        if (SkipRepeatedPixels(argb, ix, xsize)) {
          continue;
        }
        predict = TransformColor(&tx, argb[ix], 0);
        ++histo[(predict >> 16) & 0xff];  // red.
      }
    }
    cur_diff = PredictionCostCrossColor(&accumulated_red_histo[0], &histo[0]);
    if (tx.green_to_red_ == prevX.green_to_red_) {
      cur_diff -= 3;  // favor keeping the areas locally similar
    }
    if (tx.green_to_red_ == prevY.green_to_red_) {
      cur_diff -= 3;  // favor keeping the areas locally similar
    }
    if (tx.green_to_red_ == 0) {
      cur_diff -= 3;
    }
    if (cur_diff < best_diff) {
      best_diff = cur_diff;
      best_tx = tx;
    }
  }
  best_diff = 1e99;
  green_to_red = best_tx.green_to_red_;
  for (green_to_blue = -32; green_to_blue <= 32; green_to_blue += step) {
    for (red_to_blue = -32; red_to_blue <= 32; red_to_blue += step) {
      int all_y;
      int histo[256] = { 0 };
      Multipliers tx;
      tx.green_to_red_ = green_to_red;
      tx.green_to_blue_ = green_to_blue;
      tx.red_to_blue_ = red_to_blue;
      for (all_y = tile_y_offset; all_y < all_y_max; ++all_y) {
        uint32_t predict;
        int all_x;
        int ix = all_y * xsize + tile_x_offset;
        for (all_x = tile_x_offset; all_x < all_x_max; ++all_x, ++ix) {
          if (SkipRepeatedPixels(argb, ix, xsize)) {
            continue;
          }
          predict = TransformColor(&tx, argb[ix], 0);
          ++histo[predict & 0xff];  // blue.
        }
      }
      cur_diff =
        PredictionCostCrossColor(&accumulated_blue_histo[0], &histo[0]);
      if (tx.green_to_blue_ == prevX.green_to_blue_) {
        cur_diff -= 3;  // favor keeping the areas locally similar
      }
      if (tx.green_to_blue_ == prevY.green_to_blue_) {
        cur_diff -= 3;  // favor keeping the areas locally similar
      }
      if (tx.red_to_blue_ == prevX.red_to_blue_) {
        cur_diff -= 3;  // favor keeping the areas locally similar
      }
      if (tx.red_to_blue_ == prevY.red_to_blue_) {
        cur_diff -= 3;  // favor keeping the areas locally similar
      }
      if (tx.green_to_blue_ == 0) {
        cur_diff -= 3;
      }
      if (tx.red_to_blue_ == 0) {
        cur_diff -= 3;
      }
      if (cur_diff < best_diff) {
        best_diff = cur_diff;
        best_tx = tx;
      }
    }
  }
  return best_tx;
}

static void CopyTileWithColorTransform(int xsize, int ysize,
                                       int tile_x, int tile_y, int bits,
                                       Multipliers color_transform,
                                       uint32_t* const argb) {
  int y;
  int xscan = 1 << bits;
  int yscan = 1 << bits;
  tile_x <<= bits;
  tile_y <<= bits;
  if (xscan > xsize - tile_x) {
    xscan = xsize - tile_x;
  }
  if (yscan > ysize - tile_y) {
    yscan = ysize - tile_y;
  }
  yscan += tile_y;
  for (y = tile_y; y < yscan; ++y) {
    int ix = y * xsize + tile_x;
    const int end_ix = ix + xscan;
    for (; ix < end_ix; ++ix) {
      argb[ix] = TransformColor(&color_transform, argb[ix], 0);
    }
  }
}

void VP8LColorSpaceTransform(int width, int height, int bits, int step,
                             uint32_t* const argb, uint32_t* image) {
  const int max_tile_size = 1 << bits;
  int tile_xsize = VP8LSubSampleSize(width, bits);
  int tile_ysize = VP8LSubSampleSize(height, bits);
  int accumulated_red_histo[256] = { 0 };
  int accumulated_blue_histo[256] = { 0 };
  int tile_y;
  int tile_x;
  Multipliers prevX;
  Multipliers prevY;
  MultipliersClear(&prevY);
  MultipliersClear(&prevX);
  for (tile_y = 0; tile_y < tile_ysize; ++tile_y) {
    for (tile_x = 0; tile_x < tile_xsize; ++tile_x) {
      Multipliers color_transform;
      int all_x_max;
      int y;
      const int tile_y_offset = tile_y * max_tile_size;
      const int tile_x_offset = tile_x * max_tile_size;
      if (tile_y != 0) {
        ColorCodeToMultipliers(image[tile_y * tile_xsize + tile_x - 1], &prevX);
        ColorCodeToMultipliers(image[(tile_y - 1) * tile_xsize + tile_x],
                               &prevY);
      } else if (tile_x != 0) {
        ColorCodeToMultipliers(image[tile_y * tile_xsize + tile_x - 1], &prevX);
      }
      color_transform =
          GetBestColorTransformForTile(tile_x, tile_y, bits,
                                       prevX, prevY,
                                       step, width, height,
                                       &accumulated_red_histo[0],
                                       &accumulated_blue_histo[0],
                                       argb);
      image[tile_y * tile_xsize + tile_x] =
          MultipliersToColorCode(&color_transform);
      CopyTileWithColorTransform(width, height, tile_x, tile_y, bits,
                                 color_transform, argb);

      // Gather accumulated histogram data.
      all_x_max = tile_x_offset + max_tile_size;
      if (all_x_max > width) {
        all_x_max = width;
      }
      for (y = 0; y < max_tile_size; ++y) {
        int ix;
        int all_x;
        int all_y = tile_y_offset + y;
        if (all_y >= height) {
          break;
        }
        ix = all_y * width + tile_x_offset;
        for (all_x = tile_x_offset; all_x < all_x_max; ++all_x, ++ix) {
          if (ix >= 2 &&
              argb[ix] == argb[ix - 2] &&
              argb[ix] == argb[ix - 1]) {
            continue;  // repeated pixels are handled by backward references
          }
          if (ix >= width + 2 &&
              argb[ix - 2] == argb[ix - width - 2] &&
              argb[ix - 1] == argb[ix - width - 1] &&
              argb[ix] == argb[ix - width]) {
            continue;  // repeated pixels are handled by backward references
          }
          ++accumulated_red_histo[(argb[ix] >> 16) & 0xff];
          ++accumulated_blue_histo[argb[ix] & 0xff];
        }
      }
    }
  }
}

// Color space inverse transform.
static void ColorSpaceInverseTransform(const VP8LTransform* const transform,
                                       int y_start, int y_end, uint32_t* data) {
  const int width = transform->xsize_;
  const int mask = (1 << transform->bits_) - 1;
  const int tiles_per_row = VP8LSubSampleSize(width, transform->bits_);
  int y = y_start;
  const uint32_t* pred_row =
      transform->data_ + (y >> transform->bits_) * tiles_per_row;

  while (y < y_end) {
    const uint32_t* pred = pred_row;
    Multipliers m = { 0, 0, 0 };
    int x;

    for (x = 0; x < width; ++x) {
      if ((x & mask) == 0) ColorCodeToMultipliers(*pred++, &m);
      data[x] = TransformColor(&m, data[x], 1);
    }
    data += width;
    ++y;
    if ((y & mask) == 0) pred_row += tiles_per_row;;
  }
}

// Separate out pixels packed together using pixel-bundling.
static void ColorIndexInverseTransform(
    const VP8LTransform* const transform,
    int y_start, int y_end, const uint32_t* src, uint32_t* dst) {
  int y;
  const int bits_per_pixel = 8 >> transform->bits_;
  const int width = transform->xsize_;
  const uint32_t* const color_map = transform->data_;
  if (bits_per_pixel < 8) {
    const int pixels_per_byte = 1 << transform->bits_;
    const int count_mask = pixels_per_byte - 1;
    const uint32_t bit_mask = (1 << bits_per_pixel) - 1;
    for (y = y_start; y < y_end; ++y) {
      uint32_t packed_pixels = 0;
      int x;
      for (x = 0; x < width; ++x) {
        // We need to load fresh 'packed_pixels' once every 'bytes_per_pixels'
        // increments of x. Fortunately, pixels_per_byte is a power of 2, so
        // can just use a mask for that, instead of decrementing a counter.
        if ((x & count_mask) == 0) packed_pixels = ((*src++) >> 8) & 0xff;
        *dst++ = color_map[packed_pixels & bit_mask];
        packed_pixels >>= bits_per_pixel;
      }
    }
  } else {
    for (y = y_start; y < y_end; ++y) {
      int x;
      for (x = 0; x < width; ++x) {
        *dst++ = color_map[((*src++) >> 8) & 0xff];
      }
    }
  }
}

void VP8LInverseTransform(const VP8LTransform* const transform,
                          int row_start, int row_end,
                          const uint32_t* const in, uint32_t* const out) {
  assert(row_start < row_end);
  assert(row_end <= transform->ysize_);
  switch (transform->type_) {
    case SUBTRACT_GREEN:
      AddGreenToBlueAndRed(transform, row_start, row_end, out);
      break;
    case PREDICTOR_TRANSFORM:
      PredictorInverseTransform(transform, row_start, row_end, out);
      if (row_end != transform->ysize_) {
        // The last predicted row in this iteration will be the top-pred row
        // for the first row in next iteration.
        const int width = transform->xsize_;
        memcpy(out - width, out + (row_end - row_start - 1) * width,
               width * sizeof(*out));
      }
      break;
    case CROSS_COLOR_TRANSFORM:
      ColorSpaceInverseTransform(transform, row_start, row_end, out);
      break;
    case COLOR_INDEXING_TRANSFORM:
      ColorIndexInverseTransform(transform, row_start, row_end, in, out);
      break;
  }
}

//------------------------------------------------------------------------------
// Color space conversion.

static int is_big_endian(void) {
  static const union {
    uint16_t w;
    uint8_t b[2];
  } tmp = { 1 };
  return (tmp.b[0] != 1);
}

static void ConvertBGRAToRGB(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = (argb >> 16) & 0xff;
    *dst++ = (argb >>  8) & 0xff;
    *dst++ = (argb >>  0) & 0xff;
  }
}

static void ConvertBGRAToRGBA(const uint32_t* src,
                              int num_pixels, uint8_t* dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = (argb >> 16) & 0xff;
    *dst++ = (argb >>  8) & 0xff;
    *dst++ = (argb >>  0) & 0xff;
    *dst++ = (argb >> 24) & 0xff;
  }
}

static void ConvertBGRAToRGBA4444(const uint32_t* src,
                                  int num_pixels, uint8_t* dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = ((argb >> 16) & 0xf0) | ((argb >> 12) & 0xf);
    *dst++ = ((argb >>  0) & 0xf0) | ((argb >> 28) & 0xf);
  }
}

static void ConvertBGRAToRGB565(const uint32_t* src,
                                int num_pixels, uint8_t* dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = ((argb >> 16) & 0xf8) | ((argb >> 13) & 0x7);
    *dst++ = ((argb >>  5) & 0xe0) | ((argb >>  3) & 0x1f);
  }
}

static void ConvertBGRAToBGR(const uint32_t* src,
                             int num_pixels, uint8_t* dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = (argb >>  0) & 0xff;
    *dst++ = (argb >>  8) & 0xff;
    *dst++ = (argb >> 16) & 0xff;
  }
}

static void CopyOrSwap(const uint32_t* src, int num_pixels, uint8_t* dst,
                       int swap_on_big_endian) {
  if (is_big_endian() == swap_on_big_endian) {
    const uint32_t* const src_end = src + num_pixels;
    while (src < src_end) {
      uint32_t argb = *src++;
#if !defined(__BIG_ENDIAN__) && (defined(__i386__) || defined(__x86_64__))
      __asm__ volatile("bswap %0" : "=r"(argb) : "0"(argb));
      *(uint32_t*)dst = argb;
      dst += sizeof(argb);
#elif !defined(__BIG_ENDIAN__) && defined(_MSC_VER)
      argb = _byteswap_ulong(argb);
      *(uint32_t*)dst = argb;
      dst += sizeof(argb);
#else
      *dst++ = (argb >> 24) & 0xff;
      *dst++ = (argb >> 16) & 0xff;
      *dst++ = (argb >>  8) & 0xff;
      *dst++ = (argb >>  0) & 0xff;
#endif
    }
  } else {
    memcpy(dst, src, num_pixels * sizeof(*src));
  }
}

void VP8LConvertFromBGRA(const uint32_t* const in_data, int num_pixels,
                         WEBP_CSP_MODE out_colorspace, uint8_t* const rgba) {
  switch (out_colorspace) {
    case MODE_RGB:
      ConvertBGRAToRGB(in_data, num_pixels, rgba);
      break;
    case MODE_RGBA:
      ConvertBGRAToRGBA(in_data, num_pixels, rgba);
      break;
    case MODE_rgbA:
      ConvertBGRAToRGBA(in_data, num_pixels, rgba);
      WebPApplyAlphaMultiply(rgba, 0, num_pixels, 1, 0);
      break;
    case MODE_BGR:
      ConvertBGRAToBGR(in_data, num_pixels, rgba);
      break;
    case MODE_BGRA:
      CopyOrSwap(in_data, num_pixels, rgba, 1);
      break;
    case MODE_bgrA:
      CopyOrSwap(in_data, num_pixels, rgba, 1);
      WebPApplyAlphaMultiply(rgba, 0, num_pixels, 1, 0);
      break;
    case MODE_ARGB:
      CopyOrSwap(in_data, num_pixels, rgba, 0);
      break;
    case MODE_Argb:
      CopyOrSwap(in_data, num_pixels, rgba, 0);
      WebPApplyAlphaMultiply(rgba, 1, num_pixels, 1, 0);
      break;
    case MODE_RGBA_4444:
      ConvertBGRAToRGBA4444(in_data, num_pixels, rgba);
      break;
    case MODE_rgbA_4444:
      ConvertBGRAToRGBA4444(in_data, num_pixels, rgba);
      WebPApplyAlphaMultiply4444(rgba, num_pixels, 1, 0);
      break;
    case MODE_RGB_565:
      ConvertBGRAToRGB565(in_data, num_pixels, rgba);
      break;
    default:
      assert(0);          // Code flow should not reach here.
  }
}

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
