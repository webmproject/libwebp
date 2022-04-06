// Copyright 2022 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Colorspace utilities.

#include "sharpyuv/sharpyuv_csp.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static int ToFixed16(float f) { return (int)round(f * (1 << 16)); }

void SharpYuvComputeConversionMatrix(const SharpYuvColorSpace* yuv_color_space,
                                     SharpYuvConversionMatrix* matrix) {
  const float kr = yuv_color_space->kr;
  const float kb = yuv_color_space->kb;
  const float kg = 1.0f - kr - kb;
  const float cr = 0.5f / (1.0f - kb);
  const float cb = 0.5f / (1.0f - kr);

  const int shift = yuv_color_space->bits - 8;

  const float denom = (float)((1 << yuv_color_space->bits) - 1);
  float scaleY = 1.0f;
  float addY = 0.0f;
  float scaleU = cr;
  float scaleV = cb;
  float addUV = (128 << shift);

  assert(yuv_color_space->bits >= 8);

  if (yuv_color_space->range == kSharpYuvRangeLimited) {
    scaleY *= (219 << shift) / denom;
    scaleU *= (224 << shift) / denom;
    scaleV *= (224 << shift) / denom;
    addY = (16 << shift);
  }

  matrix->rgb_to_y[0] = ToFixed16(kr * scaleY);
  matrix->rgb_to_y[1] = ToFixed16(kg * scaleY);
  matrix->rgb_to_y[2] = ToFixed16(kb * scaleY);
  matrix->rgb_to_y[3] = ToFixed16(addY);

  matrix->rgb_to_u[0] = ToFixed16(-kr * scaleU);
  matrix->rgb_to_u[1] = ToFixed16(-kg * scaleU);
  matrix->rgb_to_u[2] = ToFixed16((1 - kb) * scaleU);
  matrix->rgb_to_u[3] = ToFixed16(addUV);

  matrix->rgb_to_v[0] = ToFixed16((1 - kr) * scaleV);
  matrix->rgb_to_v[1] = ToFixed16(-kg * scaleV);
  matrix->rgb_to_v[2] = ToFixed16(-kb * scaleV);
  matrix->rgb_to_v[3] = ToFixed16(addUV);
}

// Matrices are in YUV_FIX fixed point precision.
// WebP's matrix, similar but not identical to kRec601LimitedMatrix.
static const SharpYuvConversionMatrix kWebpMatrix = {
  {16839, 33059, 6420, 16 << 16},
  {-9719, -19081, 28800, 128 << 16},
  {28800, -24116, -4684, 128 << 16},
};
// Kr=0.2990f Kb=0.1140f btits=8 range=kLimited
static const SharpYuvConversionMatrix kRec601LimitedMatrix = {
  {16829, 33039, 6416, 16 << 16},
  {-9714, -19071, 28784, 128 << 16},
  {28784, -24103, -4681, 128 << 16},
};
// Kr=0.2990f Kb=0.1140f btits=8 range=kFull
static const SharpYuvConversionMatrix kRec601FullMatrix = {
  {19595, 38470, 7471, 0},
  {-11058, -21710, 32768, 128 << 16},
  {32768, -27439, -5329, 128 << 16},
};
// Kr=0.2126f Kb=0.0722f bits=8 range=kLimited
static const SharpYuvConversionMatrix kRec709LimitedMatrix = {
  {11966, 40254, 4064, 16 << 16},
  {-6596, -22189, 28784, 128 << 16},
  {28784, -26145, -2639, 128 << 16},
};
// Kr=0.2126f Kb=0.0722f bits=8 range=kFull
static const SharpYuvConversionMatrix kRec709FullMatrix = {
  {13933, 46871, 4732, 0},
  {-7509, -25259, 32768, 128 << 16},
  {32768, -29763, -3005, 128 << 16},
};

const SharpYuvConversionMatrix* SharpYuvGetConversionMatrix(
    SharpYuvMatrixType matrix_type) {
  switch (matrix_type) {
    case kSharpYuvMatrixWebp:
      return &kWebpMatrix;
    case kSharpYuvMarixRec601Limited:
      return &kRec601LimitedMatrix;
    case kSharpYuvMarixRec601Full:
      return &kRec601FullMatrix;
    case kSharpYuvMarixRec709Limited:
      return &kRec709LimitedMatrix;
    case kSharpYuvMarixRec709Full:
      return &kRec709FullMatrix;
    case kSharpYuvMarixNum:
      return NULL;
  }
  return NULL;
}
