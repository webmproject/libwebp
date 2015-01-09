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
#include "./rescaler.h"

//------------------------------------------------------------------------------
// all-in-one calls

int WebPRescaleNeededLines(const WebPRescaler* const wrk, int max_num_lines) {
  const int num_lines = (wrk->y_accum + wrk->y_sub - 1) / wrk->y_sub;
  return (num_lines > max_num_lines) ? max_num_lines : num_lines;
}

int WebPRescalerImport(WebPRescaler* const wrk, int num_lines,
                       const uint8_t* src, int src_stride) {
  int total_imported = 0;
  while (total_imported < num_lines && wrk->y_accum > 0) {
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
