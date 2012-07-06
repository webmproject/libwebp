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
#include "./example_util.h"

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

static const char* const kErrorMessages[] = {
  "WEBP_MUX_NOT_FOUND", "WEBP_MUX_INVALID_ARGUMENT", "WEBP_MUX_BAD_DATA",
  "WEBP_MUX_MEMORY_ERROR", "WEBP_MUX_NOT_ENOUGH_DATA"
};

static const char* ErrorString(WebPMuxError err) {
  assert(err <= WEBP_MUX_NOT_FOUND && err >= WEBP_MUX_NOT_ENOUGH_DATA);
  return kErrorMessages[-err];
}

static int IsNotCompatible(int count1, int count2) {
  return (count1 > 0) != (count2 > 0);
}

#define RETURN_IF_ERROR(ERR_MSG)                                     \
  if (err != WEBP_MUX_OK) {                                          \
    fprintf(stderr, ERR_MSG);                                        \
    return err;                                                      \
  }

#define RETURN_IF_ERROR2(ERR_MSG, FORMAT_STR)                        \
  if (err != WEBP_MUX_OK) {                                          \
    fprintf(stderr, ERR_MSG, FORMAT_STR);                            \
    return err;                                                      \
  }

#define ERROR_GOTO1(ERR_MSG, LABEL)                                  \
  do {                                                               \
    fprintf(stderr, ERR_MSG);                                        \
    ok = 0;                                                          \
    goto LABEL;                                                      \
  } while (0)

#define ERROR_GOTO2(ERR_MSG, FORMAT_STR, LABEL)                      \
  do {                                                               \
    fprintf(stderr, ERR_MSG, FORMAT_STR);                            \
    ok = 0;                                                          \
    goto LABEL;                                                      \
  } while (0)

#define ERROR_GOTO3(ERR_MSG, FORMAT_STR1, FORMAT_STR2, LABEL)        \
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
  printf("Features present:");
  if (flag & ANIMATION_FLAG) printf(" animation");
  if (flag & TILE_FLAG)      printf(" tiling");
  if (flag & ICCP_FLAG)      printf(" icc profile");
  if (flag & META_FLAG)      printf(" metadata");
  if (flag & ALPHA_FLAG)     printf(" transparency");
  printf("\n");

  if (flag & ANIMATION_FLAG) {
    int nFrames;
    int loop_count;
    err = WebPMuxGetLoopCount(mux, &loop_count);
    RETURN_IF_ERROR("Failed to retrieve loop count\n");
    printf("Loop Count : %d\n", loop_count);

    err = WebPMuxNumChunks(mux, WEBP_CHUNK_FRAME, &nFrames);
    RETURN_IF_ERROR("Failed to retrieve number of frames\n");

    printf("Number of frames: %d\n", nFrames);
    if (nFrames > 0) {
      int i;
      printf("No.: x_offset y_offset duration image_size");
      printf("\n");
      for (i = 1; i <= nFrames; i++) {
        int x_offset, y_offset, duration;
        WebPData bitstream;
        err = WebPMuxGetFrame(mux, i, &bitstream,
                              &x_offset, &y_offset, &duration);
        RETURN_IF_ERROR2("Failed to retrieve frame#%d\n", i);
        printf("%3d: %8d %8d %8d %10zu",
               i, x_offset, y_offset, duration, bitstream.size_);
        printf("\n");
      }
    }
  }

  if (flag & TILE_FLAG) {
    int nTiles;
    err = WebPMuxNumChunks(mux, WEBP_CHUNK_TILE, &nTiles);
    RETURN_IF_ERROR("Failed to retrieve number of tiles\n");

    printf("Number of tiles: %d\n", nTiles);
    if (nTiles > 0) {
      int i;
      printf("No.: x_offset y_offset image_size");
      printf("\n");
      for (i = 1; i <= nTiles; i++) {
        int x_offset, y_offset;
        WebPData bitstream;
        err = WebPMuxGetTile(mux, i, &bitstream, &x_offset, &y_offset);
        RETURN_IF_ERROR2("Failed to retrieve tile#%d\n", i);
        printf("%3d: %8d %8d %10zu", i, x_offset, y_offset, bitstream.size_);
        printf("\n");
      }
    }
  }

  if (flag & ICCP_FLAG) {
    WebPData icc_profile;
    err = WebPMuxGetColorProfile(mux, &icc_profile);
    RETURN_IF_ERROR("Failed to retrieve the color profile\n");
    printf("Size of the color profile data: %zu\n", icc_profile.size_);
  }

  if (flag & META_FLAG) {
    WebPData metadata;
    err = WebPMuxGetMetadata(mux, &metadata);
    RETURN_IF_ERROR("Failed to retrieve the XMP metadata\n");
    printf("Size of the XMP metadata: %zu\n", metadata.size_);
  }

  if ((flag & ALPHA_FLAG) && !(flag & (ANIMATION_FLAG | TILE_FLAG))) {
    WebPData bitstream;
    err = WebPMuxGetImage(mux, &bitstream);
    RETURN_IF_ERROR("Failed to retrieve the image\n");
    printf("Size of the image (with alpha): %zu\n", bitstream.size_);
  }

  return WEBP_MUX_OK;
}

static void PrintHelp(void) {
  printf("Usage: webpmux -get GET_OPTIONS INPUT -o OUTPUT\n");
  printf("       webpmux -set SET_OPTIONS INPUT -o OUTPUT\n");
  printf("       webpmux -strip STRIP_OPTIONS INPUT -o OUTPUT\n");
  printf("       webpmux -tile TILE_OPTIONS [-tile...] -o OUTPUT\n");
  printf("       webpmux -frame FRAME_OPTIONS [-frame...]");
  printf(" -loop LOOP_COUNT -o OUTPUT\n");
  printf("       webpmux -info INPUT\n");
  printf("       webpmux [-h|-help]\n");

  printf("\n");
  printf("GET_OPTIONS:\n");
  printf(" Extract relevant data.\n");
  printf("   icc       Get ICCP Color profile.\n");
  printf("   xmp       Get XMP metadata.\n");
  printf("   tile n    Get nth tile.\n");
  printf("   frame n   Get nth frame.\n");

  printf("\n");
  printf("SET_OPTIONS:\n");
  printf(" Set color profile/metadata.\n");
  printf("   icc       Set ICC Color profile.\n");
  printf("   xmp       Set XMP metadata.\n");

  printf("\n");
  printf("STRIP_OPTIONS:\n");
  printf(" Strip color profile/metadata.\n");
  printf("   icc       Strip ICCP color profile.\n");
  printf("   xmp       Strip XMP metadata.\n");

  printf("\n");
  printf("TILE_OPTIONS(i):\n");
  printf(" Create tiled image.\n");
  printf("   file_i +xi+yi\n");
  printf("   where:    'file_i' is the i'th tile (webp format),\n");
  printf("             'xi','yi' specify the image offset for this tile.\n");

  printf("\n");
  printf("FRAME_OPTIONS(i):\n");
  printf(" Create animation.\n");
  printf("   file_i +xi+yi+di\n");
  printf("   where:    'file_i' is the i'th animation frame (webp format),\n");
  printf("             'xi','yi' specify the image offset for this frame.\n");
  printf("             'di' is the pause duration before next frame.\n");

  printf("\nINPUT & OUTPUT are in webp format.\n");
}

static int ReadFileToWebPData(const char* const filename,
                              WebPData* const webp_data) {
  const uint8_t* data;
  size_t size;
  if (!ExUtilReadFile(filename, &data, &size)) return 0;
  webp_data->bytes_ = data;
  webp_data->size_ = size;
  return 1;
}

static int CreateMux(const char* const filename, WebPMux** mux) {
  WebPData bitstream;
  assert(mux != NULL);
  if (!ReadFileToWebPData(filename, &bitstream)) return 0;
  *mux = WebPMuxCreate(&bitstream, 1);
  free((void*)bitstream.bytes_);
  if (*mux != NULL) return 1;
  fprintf(stderr, "Failed to create mux object from file %s.\n", filename);
  return 0;
}

static int WriteData(const char* filename, const WebPData* const webpdata) {
  int ok = 0;
  FILE* fout = strcmp(filename, "-") ? fopen(filename, "wb") : stdout;
  if (!fout) {
    fprintf(stderr, "Error opening output WebP file %s!\n", filename);
    return 0;
  }
  if (fwrite(webpdata->bytes_, webpdata->size_, 1, fout) != 1) {
    fprintf(stderr, "Error writing file %s!\n", filename);
  } else {
    fprintf(stderr, "Saved file %s (%zu bytes)\n", filename, webpdata->size_);
    ok = 1;
  }
  if (fout != stdout) fclose(fout);
  return ok;
}

static int WriteWebP(WebPMux* const mux, const char* filename) {
  int ok;
  WebPData webp_data;
  const WebPMuxError err = WebPMuxAssemble(mux, &webp_data);
  if (err != WEBP_MUX_OK) {
    fprintf(stderr, "Error (%s) assembling the WebP file.\n", ErrorString(err));
    return 0;
  }
  ok = WriteData(filename, &webp_data);
  WebPDataClear(&webp_data);
  return ok;
}

static int ParseFrameArgs(const char* args, int* const x_offset,
                          int* const y_offset, int* const duration) {
  return (sscanf(args, "+%d+%d+%d", x_offset, y_offset, duration) == 3);
}

static int ParseTileArgs(const char* args,
                         int* const x_offset, int* const y_offset) {
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
        exit(0);
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
  WebPData bitstream;
  int x_offset = 0;
  int y_offset = 0;
  int duration = 0;
  WebPMuxError err = WEBP_MUX_OK;
  WebPMux* mux_single = NULL;
  long num = 0;
  int ok = 1;

  num = strtol(config->feature_.args_[0].params_, NULL, 10);
  if (num < 0) {
    ERROR_GOTO1("ERROR: Frame/Tile index must be non-negative.\n", ErrGet);
  }

  if (isFrame) {
    err = WebPMuxGetFrame(mux, num, &bitstream,
                          &x_offset, &y_offset, &duration);
    if (err != WEBP_MUX_OK) {
      ERROR_GOTO3("ERROR (%s): Could not get frame %ld.\n",
                  ErrorString(err), num, ErrGet);
    }
  } else {
    err = WebPMuxGetTile(mux, num, &bitstream, &x_offset, &y_offset);
    if (err != WEBP_MUX_OK) {
      ERROR_GOTO3("ERROR (%s): Could not get frame %ld.\n",
                  ErrorString(err), num, ErrGet);
    }
  }

  mux_single = WebPMuxNew();
  if (mux_single == NULL) {
    err = WEBP_MUX_MEMORY_ERROR;
    ERROR_GOTO2("ERROR (%s): Could not allocate a mux object.\n",
                ErrorString(err), ErrGet);
  }
  err = WebPMuxSetImage(mux_single, &bitstream, 1);
  if (err != WEBP_MUX_OK) {
    ERROR_GOTO2("ERROR (%s): Could not create single image mux object.\n",
                ErrorString(err), ErrGet);
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
  WebPData metadata, color_profile;
  int x_offset = 0;
  int y_offset = 0;
  WebPMuxError err = WEBP_MUX_OK;
  int index = 0;
  int ok = 1;
  const Feature* const feature = &config->feature_;

  switch (config->action_type_) {
    case ACTION_GET:
      ok = CreateMux(config->input_, &mux);
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
            ERROR_GOTO2("ERROR (%s): Could not get color profile.\n",
                        ErrorString(err), Err2);
          }
          ok = WriteData(config->output_, &webpdata);
          break;
        case FEATURE_XMP:
          err = WebPMuxGetMetadata(mux, &webpdata);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR (%s): Could not get XMP metadata.\n",
                        ErrorString(err), Err2);
          }
          ok = WriteData(config->output_, &webpdata);
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
            ERROR_GOTO2("ERROR (%s): Could not allocate a mux object.\n",
                        ErrorString(WEBP_MUX_MEMORY_ERROR), Err2);
          }
          for (index = 0; index < feature->arg_count_; ++index) {
            if (feature->args_[index].subtype_ == SUBTYPE_LOOP) {
              const long num = strtol(feature->args_[index].params_, NULL, 10);
              if (num < 0) {
                ERROR_GOTO1("ERROR: Loop count must be non-negative.\n", Err2);
              }
              err = WebPMuxSetLoopCount(mux, num);
              if (err != WEBP_MUX_OK) {
                ERROR_GOTO2("ERROR (%s): Could not set loop count.\n",
                            ErrorString(err), Err2);
              }
            } else if (feature->args_[index].subtype_ == SUBTYPE_FRM) {
              int duration;
              ok = ReadFileToWebPData(feature->args_[index].filename_,
                                      &webpdata);
              if (!ok) goto Err2;
              ok = ParseFrameArgs(feature->args_[index].params_,
                                  &x_offset, &y_offset, &duration);
              if (!ok) {
                WebPDataClear(&webpdata);
                ERROR_GOTO1("ERROR: Could not parse frame properties.\n", Err2);
              }
              err = WebPMuxPushFrame(mux, &webpdata, x_offset, y_offset,
                                     duration, 1);
              WebPDataClear(&webpdata);
              if (err != WEBP_MUX_OK) {
                ERROR_GOTO3("ERROR (%s): Could not add a frame at index %d.\n",
                            ErrorString(err), index, Err2);
              }
            } else {
              ERROR_GOTO1("ERROR: Invalid subtype for 'frame'", Err2);
            }
          }
          break;

        case FEATURE_TILE:
          mux = WebPMuxNew();
          if (mux == NULL) {
            ERROR_GOTO2("ERROR (%s): Could not allocate a mux object.\n",
                        ErrorString(WEBP_MUX_MEMORY_ERROR), Err2);
          }
          for (index = 0; index < feature->arg_count_; ++index) {
            ok = ReadFileToWebPData(feature->args_[index].filename_, &webpdata);
            if (!ok) goto Err2;
            ok = ParseTileArgs(feature->args_[index].params_, &x_offset,
                               &y_offset);
            if (!ok) {
              WebPDataClear(&webpdata);
              ERROR_GOTO1("ERROR: Could not parse tile properties.\n", Err2);
            }
            err = WebPMuxPushTile(mux, &webpdata, x_offset, y_offset, 1);
            WebPDataClear(&webpdata);
            if (err != WEBP_MUX_OK) {
              ERROR_GOTO3("ERROR (%s): Could not add a tile at index %d.\n",
                          ErrorString(err), index, Err2);
            }
          }
          break;

        case FEATURE_ICCP:
          ok = CreateMux(config->input_, &mux);
          if (!ok) goto Err2;
          ok = ReadFileToWebPData(feature->args_[0].filename_, &color_profile);
          if (!ok) goto Err2;
          err = WebPMuxSetColorProfile(mux, &color_profile, 1);
          free((void*)color_profile.bytes_);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR (%s): Could not set color profile.\n",
                        ErrorString(err), Err2);
          }
          break;

        case FEATURE_XMP:
          ok = CreateMux(config->input_, &mux);
          if (!ok) goto Err2;
          ok = ReadFileToWebPData(feature->args_[0].filename_, &metadata);
          if (!ok) goto Err2;
          err = WebPMuxSetMetadata(mux, &metadata, 1);
          free((void*)metadata.bytes_);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR (%s): Could not set XMP metadata.\n",
                        ErrorString(err), Err2);
          }
          break;

        default:
          ERROR_GOTO1("ERROR: Invalid feature for action 'set'.\n", Err2);
          break;
      }
      ok = WriteWebP(mux, config->output_);
      break;

    case ACTION_STRIP:
      ok = CreateMux(config->input_, &mux);
      if (!ok) goto Err2;
      switch (feature->type_) {
        case FEATURE_ICCP:
          err = WebPMuxDeleteColorProfile(mux);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR (%s): Could not delete color profile.\n",
                        ErrorString(err), Err2);
          }
          break;
        case FEATURE_XMP:
          err = WebPMuxDeleteMetadata(mux);
          if (err != WEBP_MUX_OK) {
            ERROR_GOTO2("ERROR (%s): Could not delete XMP metadata.\n",
                        ErrorString(err), Err2);
          }
          break;
        default:
          ERROR_GOTO1("ERROR: Invalid feature for action 'strip'.\n", Err2);
          break;
      }
      ok = WriteWebP(mux, config->output_);
      break;

    case ACTION_INFO:
      ok = CreateMux(config->input_, &mux);
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
  WebPMuxConfig* config;
  int ok = InitializeConfig(argc - 1, argv + 1, &config);
  if (ok) {
    ok = Process(config);
  } else {
    PrintHelp();
  }
  DeleteConfig(config);
  return !ok;
}

//------------------------------------------------------------------------------
