#ifndef WEBP_ENC_DELTA_PALETTIZATION_H_
#define WEBP_ENC_DELTA_PALETTIZATION_H_

#ifdef WEBP_EXPERIMENTAL_FEATURES

#include "../webp/types.h"

#define DELTA_PALETTE_SIZE (256 - 10)

extern const uint32_t kDeltaPalette[DELTA_PALETTE_SIZE];

// Find palette entry with minimum error from difference of actual pixel value
// and predicted pixel value. Propagate error of pixel to its top and left pixel
// in src array. Write predicted_value + palette_entry to new_image. Return
// index of best palette entry.
int FindBestPaletteEntry(int x, int y, const uint32_t* palette,
                         int palette_size, uint32_t* src,
                         int src_stride, uint32_t* new_image);

#endif  // WEBP_EXPERIMENTAL_FEATURES

#endif  // WEBP_ENC_DELTA_PALETTIZATION_H_
