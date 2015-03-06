// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Additional WebP utilities.
//

#include "../webp/extras.h"

#define XTRA_MAJ_VERSION 0
#define XTRA_MIN_VERSION 0
#define XTRA_REV_VERSION 0

//------------------------------------------------------------------------------

int WebPGetExtrasVersion(void) {
  return (XTRA_MAJ_VERSION << 16) | (XTRA_MIN_VERSION << 8) | XTRA_REV_VERSION;
}
