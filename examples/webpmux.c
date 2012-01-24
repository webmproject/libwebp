// Copyright 2011 Google Inc. All Rights Reserved.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
//  Simple command-line to create a WebP container file and to extract or strip
//  relevant data from the container file.
//
//  Compile with:     gcc -o webpmux webpmux.c -lwebpmux -lwebp
//
//
// Authors: Vikas (vikaas.arora@gmail.com),
//          Urvang (urvang@google.com)

/*  Usage examples:

  Create container WebP file:
    webpmux -tile tile_1.webp +0+0 \
            -tile tile_2.webp +960+0 \
            -tile tile_3.webp +0+576 \
            -tile tile_4.webp +960+576 \
            -o out_tile_container.webp

    webpmux -frame anim_1.webp +0+0+0 \
            -frame anim_2.webp +25+25+100 \
            -frame anim_3.webp +50+50+100 \
            -frame anim_4.webp +0+0+100 \
            -loop 10 \
            -o out_animation_container.webp

    webpmux -set icc image_profile.icc in.webp -o out_icc_container.webp
    webpmux -set xmp image_metadata.xmp in.webp -o out_xmp_container.webp

  Extract relevant data from WebP container file:
    webpmux -get tile n in.webp -o out_tile.webp
    webpmux -get frame n in.webp -o out_frame.webp
    webpmux -get icc in.webp -o image_profile.icc
    webpmux -get xmp in.webp -o image_metadata.xmp

  Strip data from WebP Container file:
    webpmux -strip icc in.webp -o out.webp
    webpmux -strip xmp in.webp -o out.webp

  Misc:
    webpmux -info in.webp
    webpmux [ -h | -help ]
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "webp/mux.h"

//------------------------------------------------------------------------------
// Config object to parse command-line arguments.

typedef enum {
  NIL_ACTION = 0,
  ACTION_GET,
  ACTION_SET,
  ACTION_STRIP,
  ACTION_INFO,
  ACTION_HELP
} ActionType;

typedef enum {
  NIL_SUBTYPE = 0,
  SUBTYPE_FRM,
  SUBTYPE_LOOP
} FeatureSubType;

typedef struct {
  FeatureSubType subtype_;
  const char* filename_;
  const char* params_;
} FeatureArg;

typedef enum {
  NIL_FEATURE = 0,
  FEATURE_XMP,
  FEATURE_ICCP,
  FEATURE_FRM,
  FEATURE_TILE
} FeatureType;

typedef struct {
  FeatureType type_;
  FeatureArg* args_;
  int arg_count_;
} Feature;

typedef struct {
  ActionType action_type_;
  const char* input_;
  const char* output_;
  Feature feature_;
} WebPMuxConfig;

//------------------------------------------------------------------------------
// Helper functions.

static int CountOccurrences(const char* arglist[], int list_length,
                            const char* arg) {
  int i;
  int num_occurences = 0;

  for (i = 0; i < list_length; ++i) {
    if (!strcmp(arglist[i], arg)) {
      ++num_occurences;
    }
  }
  return num_occurences;
}

static int IsNotCompatible(int count1, int count2) {
  return (count1 > 0) != (count2 > 0);
}

#define RETURN_IF_ERROR(ERR_MSG)                                      \
    if (err != WEBP_MUX_OK) {                                         \
      fprintf(stderr, ERR_MSG);                                       \
      return err;                                                     \
    }

#define RETURN_IF_ERROR2(ERR_MSG, FORMAT_STR)                         \
    if (err != WEBP_MUX_OK) {                                         \
      fprintf(stderr, ERR_MSG, FORMAT_STR);                           \
      return err;                                                     \
    }

#define ERROR_GOTO1(ERR_MSG, LABEL)                                   \
    do {                                                              \
      fprintf(stderr, ERR_MSG);                                       \
      ok = 0;                                                         \
      goto LABEL;                                                     \
    } while (0)

#define ERROR_GOTO2(ERR_MSG, FORMAT_STR, LABEL)                       \
    do {                                                              \
      fprintf(stderr, ERR_MSG, FORMAT_STR);                           \
      ok = 0;                                                         \
      goto LABEL;                                                     \
    } while (0)

#define ERROR_GOTO3(ERR_MSG, FORMAT_STR1, FORMAT_STR2, LABEL)         \
    do {                                                              \
      fprintf(stderr, ERR_MSG, FORMAT_STR1, FORMAT_STR2);             \
      ok = 0;                                                         \
      goto LABEL;                                                     \
    } while (0)

static WebPMuxError DisplayInfo(const WebPMux* mux) {
  uint32_t flag;

  WebPMuxError err = WebPMuxGetFeatures(mux, &flag);
  RETURN_IF_ERROR("Failed to retrieve features\n");

  if (flag == 0) {
    fprintf(stderr, "No features present.\n");
    return err;
  }

  // Print the features present.
  fprintf(stderr, "Features present:");
  if (flag & ANIMATION_FLAG) fprintf(stderr, " animation");
  if (flag & TILE_FLAG)      fprintf(stderr, " tiling");
  if (flag & ICCP_FLAG)      fprintf(stderr, " icc profile");
  if (flag & META_FLAG)      fprintf(stderr, " metadata");
  if (flag & ALPHA_FLAG)     fprintf(stderr, " transparency");
  fprintf(stderr, "\n");

  if (flag & ANIMATION_FLAG) {
    int nFrames;
    uint32_t loop_count;
    err = WebPMuxGetLoopCount(mux, &loop_count);
    RETURN_IF_ERROR("Failed to retrieve loop count\n");
    fprintf(stderr, "Loop Count : %d\n", loop_count);

    err = WebPMuxNumNamedElements(mux, "frame", &nFrames);
    RETURN_IF_ERROR("Failed to retrieve number of frames\n");

    fprintf(stderr, "Number of frames: %d\n", nFrames);
    if (nFrames > 0) {
      int i;
      uint32_t x_offset, y_offset, duration;
      fprintf(stderr, "No.: x_offset y_offset duration");
      if (flag & ALPHA_FLAG) fprintf(stderr, " alpha_size");
      fprintf(stderr, "\n");
      for (i = 1; i <= nFrames; i++) {
        WebPData image, alpha;
        err = WebPMuxGetFrame(mux, i, &image, &alpha,
                              &x_offset, &y_offset, &duration);
        RETURN_IF_ERROR2("Failed to retrieve frame#%d\n", i);
        fprintf(stderr, "%3d: %8d %8d %8d", i, x_offset, y_offset, duration);
        if (flag & ALPHA_FLAG) fprintf(stderr, " %10u", alpha.size_);
        fprintf(stderr, "\n");
      }
    }
  }

  if (flag & TILE_FLAG) {
    int nTiles;
    err = WebPMuxNumNamedElements(mux, "tile", &nTiles);
    RETURN_IF_ERROR("Failed to retrieve number of tiles\n");

    fprintf(stderr, "Number of tiles: %d\n", nTiles);
    if (nTiles > 0) {
      int i;
      uint32_t x_offset, y_offset;
      fprintf(stderr, "No.: x_offset y_offset");
      if (flag & ALPHA_FLAG) fprintf(stderr, " alpha_size");
      fprintf(stderr, "\n");
      for (i = 1; i <= nTiles; i++) {
        WebPData image, alpha;
        err = WebPMuxGetTile(mux, i, &image, &alpha, &x_offset, &y_offset);
        RETURN_IF_ERROR2("Failed to retrieve tile#%d\n", i);
        fprintf(stderr, "%3d: %8d %8d", i, x_offset, y_offset);
        if (flag & ALPHA_FLAG) fprintf(stderr, " %10u", alpha.size_);
        fprintf(stderr, "\n");
      }
    }
  }

  if (flag & ICCP_FLAG) {
    WebPData icc_profile;
    err = WebPMuxGetColorProfile(mux, &icc_profile);
    RETURN_IF_ERROR("Failed to retrieve the color profile\n");
    fprintf(stderr, "Size of the color profile data: %u\n", icc_profile.size_);
  }

  if (flag & META_FLAG) {
    WebPData metadata;
    err = WebPMuxGetMetadata(mux, &metadata);
    RETURN_IF_ERROR("Failed to retrieve the XMP metadata\n");
    fprintf(stderr, "Size of the XMP metadata: %u\n", metadata.size_);
  }

  if ((flag & ALPHA_FLAG) && !(flag & (ANIMATION_FLAG | TILE_FLAG))) {
    WebPData image, alpha;
    err = WebPMuxGetImage(mux, &image, &alpha);
    RETURN_IF_ERROR("Failed to retrieve the image\n");
    fprintf(stderr, "Size of the alpha data: %u\n", alpha.size_);
  }

  return WEBP_MUX_OK;
}

static void PrintHelp(void) {
  fprintf(stderr, "Usage: webpmux -get GET_OPTIONS INPUT -o OUTPUT          "
          "             Extract relevant data.\n");
  fprintf(stderr, "   or: webpmux -set SET_OPTIONS INPUT -o OUTPUT          "
          "             Set color profile/metadata.\n");
  fprintf(stderr, "   or: webpmux -strip STRIP_OPTIONS INPUT -o OUTPUT      "
          "             Strip color profile/metadata.\n");
  fprintf(stderr, "   or: webpmux [-tile TILE_OPTIONS]... -o OUTPUT         "
          "             Create tiled image.\n");
  fprintf(stderr, "   or: webpmux [-frame FRAME_OPTIONS]... -loop LOOP_COUNT"
          " -o OUTPUT   Create animation.\n");
  fprintf(stderr, "   or: webpmux -info INPUT                               "
          "             Print info about given webp file.\n");
  fprintf(stderr, "   or: webpmux -help OR -h                               "
          "             Print this help.\n");

  fprintf(stderr, "\n");
  fprintf(stderr, "GET_OPTIONS:\n");
  fprintf(stderr, "   icc       Get ICCP Color profile.\n");
  fprintf(stderr, "   xmp       Get XMP metadata.\n");
  fprintf(stderr, "   tile n    Get nth tile.\n");
  fprintf(stderr, "   frame n   Get nth frame.\n");

  fprintf(stderr, "\n");
  fprintf(stderr, "SET_OPTIONS:\n");
  fprintf(stderr, "   icc       Set ICC Color profile.\n");
  fprintf(stderr, "   xmp       Set XMP metadata.\n");

  fprintf(stderr, "\n");
  fprintf(stderr, "STRIP_OPTIONS:\n");
  fprintf(stderr, "   icc       Strip ICCP color profile.\n");
  fprintf(stderr, "   xmp       Strip XMP metadata.\n");

  fprintf(stderr, "\n");
  fprintf(stderr, "TILE_OPTIONS(i):\n");
  fprintf(stderr, "   file_i +xi+yi\n");
  fprintf(stderr, "   where:    'file_i' is the i'th tile (webp format),\n");
  fprintf(stderr, "             'xi','yi' specify the image offset for this "
          "tile.\n");

  fprintf(stderr, "\n");
  fprintf(stderr, "FRAME_OPTIONS(i):\n");
  fprintf(stderr, "   file_i +xi+yi+di\n");
  fprintf(stderr, "   where:    'file_i' is the i'th animation frame (webp "
          "format),\n");
  fprintf(stderr, "             'xi','yi' specify the image offset for this "
          "frame.\n");
  fprintf(stderr, "             'di' is the pause duration before next frame."
          "\n");

  fprintf(stderr, "\n");
  fprintf(stderr, "INPUT & OUTPUT are in webp format.");
  fprintf(stderr, "\n");
}

static int ReadData(const char* filename, void** data_ptr, uint32_t* size_ptr) {
  void* data = NULL;
  long size = 0;
  int ok = 0;
  FILE* in;

  *size_ptr = 0;
  in = fopen(filename, "rb");
  if (!in) {
    fprintf(stderr, "Failed to open file %s\n", filename);
    return 0;
  }
  fseek(in, 0, SEEK_END);
  size = ftell(in);
  fseek(in, 0, SEEK_SET);
  if (size > 0xffffffffu) {
    fprintf(stderr, "Size (%ld bytes) is out of range for file %s\n",
            size, filename);
    size = 0;
    goto Err;
  }
  if (size < 0) {
    size = 0;
    goto Err;
  }
  data = malloc(size);
  if (data) {
    if (fread(data, size, 1, in) != 1) {
      free(data);
      data = NULL;
      size = 0;
      fprintf(stderr, "Failed to read %ld bytes from file %s\n",
              size, filename);
      goto Err;
    }
    ok = 1;
  } else {
    fprintf(stderr, "Failed to allocate %ld bytes for reading file %s\n",
            size, filename);
    size = 0;
  }

 Err:
  if (in != stdin) fclose(in);
  *size_ptr = (uint32_t)size;
  *data_ptr = data;
  return ok;
}

static int ReadFile(const char* const filename, WebPMux** mux) {
  uint32_t size = 0;
  void* data = NULL;
  WebPMuxState mux_state;

  assert(mux != NULL);

  if (!ReadData(filename, &data, &size)) return 0;
  *mux = WebPMuxCreate((const uint8_t*)data, size, 1, &mux_state);
  free(data);
  if (*mux != NULL && mux_state == WEBP_MUX_STATE_COMPLETE) return 1;
  fprintf(stderr, "Failed to create mux object from file %s. mux_state = %d.\n",
          filename, mux_state);
  return 0;
}

static int ReadImage(const char* filename,
                     const uint8_t** data_ptr, uint32_t* size_ptr,
                     const uint8_t** alpha_data_ptr, uint32_t* alpha_size_ptr) {
  void* data = NULL;
  uint32_t size = 0;
  WebPData image, alpha;
  WebPMux* mux;
  WebPMuxError err;
  int ok = 0;
  WebPMuxState mux_state;

  if (!ReadData(filename, &data, &size)) return 0;

  mux = WebPMuxCreate((const uint8_t*)data, size, 1, &mux_state);
  free(data);
  if (mux == NULL || mux_state != WEBP_MUX_STATE_COMPLETE) {
    fprintf(stderr,
            "Failed to create mux object from file %s. mux_state = %d.\n",
            filename, mux_state);
    return 0;
  }
  err = WebPMuxGetImage(mux, &image, &alpha);
  if (err == WEBP_MUX_OK) {
    uint8_t* const data_mem = (uint8_t*)malloc(image.size_);
    uint8_t* const alpha_mem = (uint8_t*)malloc(alpha.size_);
    if ((data_mem != NULL) && (alpha_mem != NULL)) {
      memcpy(data_mem, image.bytes_, image.size_);
      memcpy(alpha_mem, alpha.bytes_, alpha.size_);
      *data_ptr = data_mem;
      *size_ptr = image.size_;
      *alpha_data_ptr = alpha_mem;
      *alpha_size_ptr = alpha.size_;
      ok = 1;
    } else {
      free(data_mem);
      free(alpha_mem);
      err = WEBP_MUX_MEMORY_ERROR;
      fprintf(stderr, "Failed to allocate %u bytes to extract image data from"
              " file %s. Error: %d\n",
              image.size_ + alpha.size_, filename, err);
    }
  } else {
    fprintf(stderr, "Failed to extract image data from file %s. Error: %d\n",
            filename, err);
  }
  WebPMuxDelete(mux);
  return ok;
}

static int WriteData(const char* filename, const void* data, uint32_t size) {
  int ok = 0;
  FILE* fout = strcmp(filename, "-") ? fopen(filename, "wb") : stdout;
  if (!fout) {
    fprintf(stderr, "Error opening output WebP file %s!\n", filename);
    return 0;
  }
  if (fwrite(data, size, 1, fout) != 1) {
    fprintf(stderr, "Error writing file %s!\n", filename);
  } else {
    fprintf(stderr, "Saved file %s (%d bytes)\n", filename, size);
    ok = 1;
  }
  if (fout != stdout) fclose(fout);
  return ok;
}

static int WriteWebP(WebPMux* const mux, const char* filename) {
  uint8_t* data = NULL;
  uint32_t size = 0;
  int ok;

  WebPMuxError err = WebPMuxAssemble(mux, &data, &size);
  if (err != WEBP_MUX_OK) {
    fprintf(stderr, "Error (%d) assembling the WebP file.\n", err);
    return 0;
  }
  ok = WriteData(filename, data, size);
  free(data);
  return ok;
}

static int ParseFrameArgs(const char* args, uint32_t* x_offset,
                          uint32_t* y_offset, uint32_t* duration) {
  return (sscanf(args, "+%d+%d+%d", x_offset, y_offset, duration) == 3);
}

static int ParseTileArgs(const char* args, uint32_t* x_offset,
                         uint32_t* y_offset) {
  return (sscanf(args, "+%d+%d", x_offset, y_offset) == 2);
}

//------------------------------------------------------------------------------
// Clean-up.

static void DeleteConfig(WebPMuxConfig* config) {
  if (config != NULL) {
    free(config->feature_.args_);
    free(config);
  }
}

//------------------------------------------------------------------------------
// Parsing.

// Basic syntactic checks on the command-line arguments.
// Returns 1 on valid, 0 otherwise.
// Also fills up num_feature_args to be number of feature arguments given.
// (e.g. if there are 4 '-frame's and 1 '-loop', then num_feature_args = 5).
static int ValidateCommandLine(int argc, const char* argv[],
                               int* num_feature_args) {
  int num_frame_args;
  int num_tile_args;
  int num_loop_args;
  int ok = 1;

  assert(num_feature_args != NULL);
  *num_feature_args = 0;

  // Simple checks.
  if (CountOccurrences(argv, argc, "-get") > 1) {
    ERROR_GOTO1("ERROR: Multiple '-get' arguments specified.\n", ErrValidate);
  }
  if (CountOccurrences(argv, argc, "-set") > 1) {
    ERROR_GOTO1("ERROR: Multiple '-set' arguments specified.\n", ErrValidate);
  }
  if (CountOccurrences(argv, argc, "-strip") > 1) {
    ERROR_GOTO1("ERROR: Multiple '-strip' arguments specified.\n", ErrValidate);
  }
  if (CountOccurrences(argv, argc, "-info") > 1) {
    ERROR_GOTO1("ERROR: Multiple '-info' arguments specified.\n", ErrValidate);
  }
  if (CountOccurrences(argv, argc, "-o") > 1) {
    ERROR_GOTO1("ERROR: Multiple output files specified.\n", ErrValidate);
  }

  // Compound checks.
  num_frame_args = CountOccurrences(argv, argc, "-frame");
  num_tile_args = CountOccurrences(argv, argc, "-tile");
  num_loop_args = CountOccurrences(argv, argc, "-loop");

  if (num_loop_args > 1) {
    ERROR_GOTO1("ERROR: Multiple loop counts specified.\n", ErrValidate);
  }

  if (IsNotCompatible(num_frame_args, num_loop_args)) {
    ERROR_GOTO1("ERROR: Both frames and loop count have to be specified.\n",
                ErrValidate);
  }
  if (num_frame_args > 0 && num_tile_args > 0) {
    ERROR_GOTO1("ERROR: Only one of frames & tiles can be specified at a time."
                "\n", ErrValidate);
  }

  assert(ok == 1);
  if (num_frame_args == 0 && num_tile_args == 0) {
    // Single argument ('set' action for XMP or ICCP, OR a 'get' action).
    *num_feature_args = 1;
  } else {
    // Multiple arguments ('set' action for animation or tiling).
    if (num_frame_args > 0) {
      *num_feature_args = num_frame_args + num_loop_args;
    } else {
      *num_feature_args = num_tile_args;
    }
  }

 ErrValidate:
  return ok;
}

#define ACTION_IS_NIL (config->action_type_ == NIL_ACTION)

#define FEATURETYPE_IS_NIL (feature->type_ == NIL_FEATURE)

#define CHECK_NUM_ARGS_LESS(NUM, LABEL)                                  \
  if (argc < i + (NUM)) {                                                \
    fprintf(stderr, "ERROR: Too few arguments for '%s'.\n", argv[i]);    \
    goto LABEL;                                                          \
  }

#define CHECK_NUM_ARGS_NOT_EQUAL(NUM, LABEL)                             \
  if (argc != i + (NUM)) {                                               \
    fprintf(stderr, "ERROR: Too many arguments for '%s'.\n", argv[i]);   \
    goto LABEL;                                                          \
  }

// Parses command-line arguments to fill up config object. Also performs some
// semantic checks.
static int ParseCommandLine(int argc, const char* argv[],
                            WebPMuxConfig* config) {
  int i = 0;
  int feature_arg_index = 0;
  int ok = 1;

  while (i < argc) {
    Feature* const feature = &config->feature_;
    FeatureArg* const arg = &feature->args_[feature_arg_index];
    if (argv[i][0] == '-') {  // One of the action types or output.
      if (!strcmp(argv[i], "-set")) {
        if (ACTION_IS_NIL) {
          config->action_type_ = ACTION_SET;
        } else {
          ERROR_GOTO1("ERROR: Multiple actions specified.\n", ErrParse);
        }
        ++i;
      } else if (!strcmp(argv[i], "-get")) {
        if (ACTION_IS_NIL) {
          config->action_type_ = ACTION_GET;
        } else {
          ERROR_GOTO1("ERROR: Multiple actions specified.\n", ErrParse);
        }
        ++i;
      } else if (!strcmp(argv[i], "-strip")) {
        if (ACTION_IS_NIL) {
          config->action_type_ = ACTION_STRIP;
          feature->arg_count_ = 0;
        } else {
          ERROR_GOTO1("ERROR: Multiple actions specified.\n", ErrParse);
        }
        ++i;
      } else if (!strcmp(argv[i], "-frame")) {
        CHECK_NUM_ARGS_LESS(3, ErrParse);
        if (ACTION_IS_NIL || config->action_type_ == ACTION_SET) {
          config->action_type_ = ACTION_SET;
        } else {
          ERROR_GOTO1("ERROR: Multiple actions specified.\n", ErrParse);
        }
        if (FEATURETYPE_IS_NIL || feature->type_ == FEATURE_FRM) {
          feature->type_ = FEATURE_FRM;
        } else {
          ERROR_GOTO1("ERROR: Multiple features specified.\n", ErrParse);
        }
        arg->subtype_ = SUBTYPE_FRM;
        arg->filename_ = argv[i + 1];
        arg->params_ = argv[i + 2];
        ++feature_arg_index;
        i += 3;
      } else if (!strcmp(argv[i], "-loop")) {
        CHECK_NUM_ARGS_LESS(2, ErrParse);
        if (ACTION_IS_NIL || config->action_type_ == ACTION_SET) {
          config->action_type_ = ACTION_SET;
        } else {
          ERROR_GOTO1("ERROR: Multiple actions specified.\n", ErrParse);
        }
        if (FEATURETYPE_IS_NIL || feature->type_ == FEATURE_FRM) {
          feature->type_ = FEATURE_FRM;
        } else {
          ERROR_GOTO1("ERROR: Multiple features specified.\n", ErrParse);
        }
        arg->subtype_ = SUBTYPE_LOOP;
        arg->params_ = argv[i + 1];
        ++feature_arg_index;
        i += 2;
      } else if (!strcmp(argv[i], "-tile")) {
        CHECK_NUM_ARGS_LESS(3, ErrParse);
        if (ACTION_IS_NIL || config->action_type_ == ACTION_SET) {
          config->action_type_ = ACTION_SET;
        } else {
          ERROR_GOTO1("ERROR: Multiple actions specified.\n", ErrParse);
        }
        if (FEATURETYPE_IS_NIL || feature->type_ == FEATURE_TILE) {
          feature->type_ = FEATURE_TILE;
        } else {
          ERROR_GOTO1("ERROR: Multiple features specified.\n", ErrParse);
        }
        arg->filename_ = argv[i + 1];
        arg->params_ = argv[i + 2];
        ++feature_arg_index;
        i += 3;
      } else if (!strcmp(argv[i], "-o")) {
        CHECK_NUM_ARGS_LESS(2, ErrParse);
        config->output_ = argv[i + 1];
        i += 2;
      } else if (!strcmp(argv[i], "-info")) {
        CHECK_NUM_ARGS_NOT_EQUAL(2, ErrParse);
        if (config->action_type_ != NIL_ACTION) {
          ERROR_GOTO1("ERROR: Multiple actions specified.\n", ErrParse);
        } else {
          config->action_type_ = ACTION_INFO;
          feature->arg_count_ = 0;
          config->input_ = argv[i + 1];
        }
        i += 2;
      } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help")) {
        PrintHelp();
        DeleteConfig(config);
        exit(1);
      } else {
        ERROR_GOTO2("ERROR: Unknown option: '%s'.\n", argv[i], ErrParse);
      }
    } else {  // One of the feature types or input.
      if (ACTION_IS_NIL) {
        ERROR_GOTO1("ERROR: Action must be specified before other arguments.\n",
                    ErrParse);
      }
      if (!strcmp(argv[i], "icc") || !strcmp(argv[i], "xmp")) {
        if (FEATURETYPE_IS_NIL) {
          feature->type_ = (!strcmp(argv[i], "icc")) ? FEATURE_ICCP :
              FEATURE_XMP;
        } else {
          ERROR_GOTO1("ERROR: Multiple features specified.\n", ErrParse);
        }
        if (config->action_type_ == ACTION_SET) {
          CHECK_NUM_ARGS_LESS(2, ErrParse);
          arg->filename_ = argv[i + 1];
          ++feature_arg_index;
          i += 2;
        } else {
          ++i;
        }
      } else if ((!strcmp(argv[i], "frame") ||
                  !strcmp(argv[i], "tile")) &&
                  (config->action_type_ == ACTION_GET)) {
        CHECK_NUM_ARGS_LESS(2, ErrParse);
        feature->type_ = (!strcmp(argv[i], "frame")) ? FEATURE_FRM :
            FEATURE_TILE;
        arg->params_ = argv[i + 1];
        ++feature_arg_index;
        i += 2;
      } else {  // Assume input file.
        if (config->input_ == NULL) {
          config->input_ = argv[i];
        } else {
          ERROR_GOTO2("ERROR at '%s': Multiple input files specified.\n",
                      argv[i], ErrParse);
        }
        ++i;
      }
    }
  }
 ErrParse:
  return ok;
}

// Additional checks after config is filled.
static int ValidateConfig(WebPMuxConfig* config) {
  int ok = 1;
  Feature* const feature = &config->feature_;

  // Action.
  if (ACTION_IS_NIL) {
    ERROR_GOTO1("ERROR: No action specified.\n", ErrValidate2);
  }

  // Feature type.
  if (FEATURETYPE_IS_NIL && config->action_type_ != ACTION_INFO) {
    ERROR_GOTO1("ERROR: No feature specified.\n", ErrValidate2);
  }

  // Input file.
  if (config->input_ == NULL) {
    if (config->action_type_ != ACTION_SET) {
      ERROR_GOTO1("ERROR: No input file specified.\n", ErrValidate2);
    } else if (feature->type_ != FEATURE_FRM &&
               feature->type_ != FEATURE_TILE) {
      ERROR_GOTO1("ERROR: No input file specified.\n", ErrValidate2);
    }
  }

  // Output file.
  if (config->output_ == NULL && config->action_type_ != ACTION_INFO) {
    ERROR_GOTO1("ERROR: No output file specified.\n", ErrValidate2);
  }

 ErrValidate2:
  return ok;
}

// Create config object from command-line arguments.
static int InitializeConfig(int argc, const char* argv[],
                            WebPMuxConfig** config) {
  int num_feature_args = 0;
  int ok = 1;

  assert(config != NULL);
  *config = NULL;

  // Validate command-line arguments.
  if (!ValidateCommandLine(argc, argv, &num_feature_args)) {
    ERROR_GOTO1("Exiting due to command-line parsing error.\n", Err1);
  }

  // Allocate memory.
  *config = (WebPMuxConfig*)calloc(1, sizeof(**config));
  if (*config == NULL) {
    ERROR_GOTO1("ERROR: Memory allocation error.\n", Err1);
  }
  (*config)->feature_.arg_count_ = num_feature_args;
  (*config)->feature_.args_ =
      (FeatureArg*)calloc(num_feature_args, sizeof(FeatureArg));
  if ((*config)->feature_.args_ == NULL) {
    ERROR_GOTO1("ERROR: Memory allocation error.\n", Err1);
  }

  // Parse command-line.
  if (!ParseCommandLine(argc, argv, *config) ||
      !ValidateConfig(*config)) {
    ERROR_GOTO1("Exiting due to command-line parsing error.\n", Err1);
  }

 Err1:
  return ok;
}

#undef ACTION_IS_NIL
#undef FEATURETYPE_IS_NIL
#undef CHECK_NUM_ARGS_LESS
#undef CHECK_NUM_ARGS_MORE

//------------------------------------------------------------------------------
// Processing.

static int GetFrameTile(const WebPMux* mux,
                        const WebPMuxConfig* config, int isFrame) {
  WebPData image, alpha;
  uint32_t x_offset = 0;
  uint32_t y_offset = 0;
  uint32_t duration = 0;
  WebPMuxError err = WEBP_MUX_OK;
  WebPMux* mux_single = NULL;
  long num = 0;
  int ok = 1;

  num = strtol(config->feature_.args_[0].params_, NULL, 10);
  if (num < 0) {
    ERROR_GOTO1("ERROR: Frame/Tile index must be non-negative.\n", ErrGet);
  }

  if (isFrame) {
    err = WebPMuxGetFrame(mux, num, &image, &alpha,
                          &x_offset, &y_offset, &duration);
    if (err != WEBP_MUX_OK) {
      ERROR_GOTO3("ERROR#%d: Could not get frame %ld.\n", err, num, ErrGet);
    }
  } else {
    err = WebPMuxGetTile(mux, num, &image, &alpha, &x_offset, &y_offset);
    if (err != WEBP_MUX_OK) {
      ERROR_GOTO3("ERROR#%d: Could not get frame %ld.\n", err, num, ErrGet);
    }
  }

  mux_single = WebPMuxNew();
  if (mux_single == NULL) {
    err = WEBP_MUX_MEMORY_ERROR;
    ERROR_GOTO2("ERROR#%d: Could not allocate a mux object.\n", err, ErrGet);
  }
  err = WebPMuxSetImage(mux_single, image.bytes_, image.size_,
                        alpha.bytes_, alpha.size_, 1);
  if (err != WEBP_MUX_OK) {
    ERROR_GOTO2("ERROR#%d: Could not create single image mux object.\n", err,
                ErrGet);
  }
  ok = WriteWebP(mux_single, config->output_);

 ErrGet:
  WebPMuxDelete(mux_single);
  return ok;
}

// Read and process config.
static int Process(const WebPMuxConfig* config) {
  WebPMux* mux = NULL;
  WebPData webpdata;
  const uint8_t* data = NULL;
  uint32_t size = 0;
  const uint8_t* alpha_data = NULL;
  uint32_t alpha_size = 0;
  uint32_t x_offset = 0;
  uint32_t y_offset = 0;
  uint32_t duration = 0;
  uint32_t loop_count = 0;
  WebPMuxError err = WEBP_MUX_OK;
  long num;
  int index = 0;
  int ok = 1;
  const Feature* const feature = &config->feature_;

  switch (config->action_type_) {
    case ACTION_GET:
      ok = ReadFile(config->input_, &mux);
      if (!ok) goto Err2;
      switch (feature->type_) {
        case FEATURE_FRM:
          ok = GetFrameTile(mux, config, 1);
          break;

        case FEATURE_TILE:
          ok = GetFrameTile(mux, config, 0);
          break;

        case FEATURE_ICCP:
          err = WebPMuxGetColorProfile(mux, &webpdata);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR#%d: Could not get color profile.\n", err, Err2);
          }
          ok = WriteData(config->output_, webpdata.bytes_, webpdata.size_);
          break;
        case FEATURE_XMP:
          err = WebPMuxGetMetadata(mux, &webpdata);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR#%d: Could not get XMP metadata.\n", err, Err2);
          }
          ok = WriteData(config->output_, webpdata.bytes_, webpdata.size_);
          break;

        default:
          ERROR_GOTO1("ERROR: Invalid feature for action 'get'.\n", Err2);
          break;
      }
      break;

    case ACTION_SET:
      switch (feature->type_) {
        case FEATURE_FRM:
          mux = WebPMuxNew();
          if (mux == NULL) {
            ERROR_GOTO2("ERROR#%d: Could not allocate a mux object.\n",
                        WEBP_MUX_MEMORY_ERROR, Err2);
          }
          for (index = 0; index < feature->arg_count_; ++index) {
            if (feature->args_[index].subtype_ == SUBTYPE_LOOP) {
              num = strtol(feature->args_[index].params_, NULL, 10);
              if (num < 0) {
                ERROR_GOTO1("ERROR: Loop count must be non-negative.\n", Err2);
              } else {
                loop_count = num;
              }
              err  = WebPMuxSetLoopCount(mux, loop_count);
              if (err != WEBP_MUX_OK) {
                ERROR_GOTO2("ERROR#%d: Could not set loop count.\n", err, Err2);
              }
            } else if (feature->args_[index].subtype_ == SUBTYPE_FRM) {
              ok = ReadImage(feature->args_[index].filename_,
                             &data, &size, &alpha_data, &alpha_size);
              if (!ok) goto Err2;
              ok = ParseFrameArgs(feature->args_[index].params_,
                                  &x_offset, &y_offset, &duration);
              if (!ok) {
                free((void*)data);
                free((void*)alpha_data);
                ERROR_GOTO1("ERROR: Could not parse frame properties.\n", Err2);
              }
              err = WebPMuxAddFrame(mux, 0, data, size, alpha_data, alpha_size,
                                    x_offset, y_offset, duration, 1);
              free((void*)data);
              free((void*)alpha_data);
              if (err != WEBP_MUX_OK) {
                ERROR_GOTO3("ERROR#%d: Could not add a frame at index %d.\n",
                            err, index, Err2);
              }
            } else {
              ERROR_GOTO1("ERROR: Invalid subtype for 'frame'", Err2);
            }
          }
          break;

        case FEATURE_TILE:
          mux = WebPMuxNew();
          if (mux == NULL) {
            ERROR_GOTO2("ERROR#%d: Could not allocate a mux object.\n",
                        WEBP_MUX_MEMORY_ERROR, Err2);
          }
          for (index = 0; index < feature->arg_count_; ++index) {
            ok = ReadImage(feature->args_[index].filename_,
                           &data, &size, &alpha_data, &alpha_size);
            if (!ok) goto Err2;
            ok = ParseTileArgs(feature->args_[index].params_, &x_offset,
                               &y_offset);
            if (!ok) {
              free((void*)data);
              free((void*)alpha_data);
              ERROR_GOTO1("ERROR: Could not parse tile properties.\n", Err2);
            }
            err = WebPMuxAddTile(mux, 0, data, size, alpha_data, alpha_size,
                                 x_offset, y_offset, 1);
            free((void*)data);
            free((void*)alpha_data);
            if (err != WEBP_MUX_OK) {
              ERROR_GOTO3("ERROR#%d: Could not add a tile at index %d.\n",
                          err, index, Err2);
            }
          }
          break;

        case FEATURE_ICCP:
          ok = ReadFile(config->input_, &mux);
          if (!ok) goto Err2;
          ok = ReadData(feature->args_[0].filename_, (void**)&data, &size);
          if (!ok) goto Err2;
          err = WebPMuxSetColorProfile(mux, data, size, 1);
          free((void*)data);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR#%d: Could not set color profile.\n", err, Err2);
          }
          break;

        case FEATURE_XMP:
          ok = ReadFile(config->input_, &mux);
          if (!ok) goto Err2;
          ok = ReadData(feature->args_[0].filename_, (void**)&data, &size);
          if (!ok) goto Err2;
          err = WebPMuxSetMetadata(mux, data, size, 1);
          free((void*)data);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR#%d: Could not set XMP metadata.\n", err, Err2);
          }
          break;

        default:
          ERROR_GOTO1("ERROR: Invalid feature for action 'set'.\n", Err2);
          break;
      }
      ok = WriteWebP(mux, config->output_);
      break;

    case ACTION_STRIP:
      ok = ReadFile(config->input_, &mux);
      if (!ok) goto Err2;
      switch (feature->type_) {
        case FEATURE_ICCP:
          err = WebPMuxDeleteColorProfile(mux);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR#%d: Could not delete color profile.\n", err,
                        Err2);
          }
          break;
        case FEATURE_XMP:
          err = WebPMuxDeleteMetadata(mux);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR#%d: Could not delete XMP metadata.\n", err,
                        Err2);
          }
          break;
        default:
          ERROR_GOTO1("ERROR: Invalid feature for action 'strip'.\n", Err2);
          break;
      }
      ok = WriteWebP(mux, config->output_);
      break;

    case ACTION_INFO:
      ok = ReadFile(config->input_, &mux);
      if (!ok) goto Err2;
      ok = (DisplayInfo(mux) == WEBP_MUX_OK);
      break;

    default:
      assert(0);  // Invalid action.
      break;
  }

 Err2:
  WebPMuxDelete(mux);
  return ok;
}

//------------------------------------------------------------------------------
// Main.

int main(int argc, const char* argv[]) {
  int ok = 1;
  WebPMuxConfig* config;
  ok = InitializeConfig(argc-1, argv+1, &config);
  if (ok) {
    Process(config);
  } else {
    PrintHelp();
  }
  DeleteConfig(config);
  return ok;
}

//------------------------------------------------------------------------------
