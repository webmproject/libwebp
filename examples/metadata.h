// Copyright 2012 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  Metadata types and functions.
//

#ifndef WEBP_EXAMPLES_METADATA_H_
#define WEBP_EXAMPLES_METADATA_H_

#include "webp/types.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct MetadataPayload {
  uint8_t* bytes;
  size_t size;
} MetadataPayload;

typedef struct Metadata {
  MetadataPayload exif;
  MetadataPayload iccp;
  MetadataPayload xmp;
} Metadata;

#define METADATA_OFFSET(x) offsetof(Metadata, x)

static void MetadataInit(Metadata* const m) {
  memset(m, 0, sizeof(*m));
}

static void MetadataPayloadDelete(MetadataPayload* const payload) {
  free(payload->bytes);
  payload->bytes = NULL;
  payload->size = 0;
}

static void MetadataFree(Metadata* const m) {
  MetadataPayloadDelete(&m->exif);
  MetadataPayloadDelete(&m->iccp);
  MetadataPayloadDelete(&m->xmp);
}

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif

#endif  // WEBP_EXAMPLES_METADATA_H_
