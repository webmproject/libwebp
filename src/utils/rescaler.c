// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Rescaling functions
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../dsp/dsp.h"
#include "./rescaler.h"

//------------------------------------------------------------------------------

void WebPRescalerInit(WebPRescaler* const wrk, int src_width, int src_height,
                      uint8_t* const dst,
                      int dst_width, int dst_height, int dst_stride,
                      int num_channels, int32_t* const work) {
  const int x_add = src_width, x_sub = dst_width;
  const int y_add = src_height, y_sub = dst_height;
  wrk->x_expand = (src_width < dst_width);
  wrk->y_expand = (src_height < dst_height);
  wrk->src_width = src_width;
  wrk->src_height = src_height;
  wrk->dst_width = dst_width;
  wrk->dst_height = dst_height;
  wrk->dst = dst;
  wrk->dst_stride = dst_stride;
  wrk->num_channels = num_channels;

  // for 'x_expand', we use bilinear interpolation
  wrk->x_add = wrk->x_expand ? (x_sub - 1) : x_add;
  wrk->x_sub = wrk->x_expand ? (x_add - 1) : x_sub;
  if (!wrk->x_expand) {  // fx_scale is not used otherwise
    wrk->fx_scale = (1 << WEBP_RESCALER_RFIX) / wrk->x_sub;
  }

  // vertical scaling parameters
  wrk->y_accum = y_add;
  wrk->y_add = y_add;
  wrk->y_sub = y_sub;
  wrk->fy_scale = (1 << WEBP_RESCALER_RFIX) / wrk->y_sub;

  wrk->fxy_scale =
      ((int64_t)dst_height << WEBP_RESCALER_RFIX) / (wrk->x_add * wrk->y_add);

  wrk->irow = work;
  wrk->frow = work + num_channels * dst_width;
  memset(work, 0, 2 * dst_width * num_channels * sizeof(*work));

  WebPRescalerDspInit();
}

int WebPRescalerGetScaledDimensions(int src_width, int src_height,
                                    int* const scaled_width,
                                    int* const scaled_height) {
  assert(scaled_width != NULL);
  assert(scaled_height != NULL);
  {
    int width = *scaled_width;
    int height = *scaled_height;

    // if width is unspecified, scale original proportionally to height ratio.
    if (width == 0) {
      width = (src_width * height + src_height / 2) / src_height;
    }
    // if height is unspecified, scale original proportionally to width ratio.
    if (height == 0) {
      height = (src_height * width + src_width / 2) / src_width;
    }
    // Check if the overall dimensions still make sense.
    if (width <= 0 || height <= 0) {
      return 0;
    }

    *scaled_width = width;
    *scaled_height = height;
    return 1;
  }
}

//------------------------------------------------------------------------------
// all-in-one calls

int WebPRescaleNeededLines(const WebPRescaler* const wrk, int max_num_lines) {
  const int num_lines = (wrk->y_accum + wrk->y_sub - 1) / wrk->y_sub;
  return (num_lines > max_num_lines) ? max_num_lines : num_lines;
}

int WebPRescalerImport(WebPRescaler* const wrk, int num_lines,
                       const uint8_t* src, int src_stride) {
  int total_imported = 0;
  while (total_imported < num_lines && !WebPRescalerHasPendingOutput(wrk)) {
    int channel;
    for (channel = 0; channel < wrk->num_channels; ++channel) {
      WebPRescalerImportRow(wrk, src, channel);
    }
    src += src_stride;
    ++total_imported;
    wrk->y_accum -= wrk->y_sub;
  }
  return total_imported;
}

int WebPRescalerExport(WebPRescaler* const rescaler) {
  int total_exported = 0;
  while (WebPRescalerHasPendingOutput(rescaler)) {
    WebPRescalerExportRow(rescaler, 0);
    ++total_exported;
  }
  return total_exported;
}

//------------------------------------------------------------------------------
