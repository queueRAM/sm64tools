#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../n64graphics.h"
#include "../utils.h"

#define N64CI_VERSION "0.1"

#define SCALE_8_5(VAL_) (((VAL_) * 0x1F) / 0xFF)

typedef struct
{
   char pal_filename[FILENAME_MAX];
   unsigned pal_entries;
   char **input_files;
   unsigned input_count;
} arg_config;

typedef struct
{
   rgba *data;
   unsigned char *ci;
   int width;
   int height;
} image_t;

typedef struct
{
   unsigned short data[256];
   unsigned max; // max number of entries
   unsigned used;
} palette_t;

// default configuration
static const arg_config default_args = 
{
   "palette.bin", // output palette filename
   256,           // number of palette entries
   NULL,          // array of input file names
   0              // count of input files
};

// find index of palette color
// return -1 if not found
static int pal_find_color(palette_t *pal, unsigned short col)
{
   unsigned i;
   for (i = 0; i < pal->used; i++) {
      if (pal->data[i] == col) {
         return i;
      }
   }
   return -1;
}

// find color in palette, or add if not there
// returns palette index entered or -1 if palette full
static int pal_add_color(palette_t *pal, rgba *col)
{
   unsigned short rgba16;
   char r, g, b, a;
   int idx;
   r = SCALE_8_5(col->red);
   g = SCALE_8_5(col->green);
   b = SCALE_8_5(col->blue);
   a = col->alpha ? 0x1 : 0x0;
   rgba16 = (((r << 3) | (g >> 2)) << 8) | ((g & 0x3) << 6) | (b << 1) | a;
   idx = pal_find_color(pal, rgba16);
   if (idx < 0) {
      if (pal->used == pal->max) {
         ERROR("Error: trying to use more than %d\n", pal->max);
      } else {
         idx = pal->used;
         pal->data[pal->used] = rgba16;
         pal->used++;
      }
   }
   return idx;
}

static void print_usage(void)
{
   ERROR("Usage: n64ci [-e PAL_ENTIRES] [-p PAL_FILE] [-v] [PNG images]\n"
         "\n"
         "n64ci v" N64CI_VERSION ": N64 CI image encoder\n"
         "\n"
         "Optional arguments:\n"
         " -e PAL_ENTRIES number of palette entires (default: %d)\n"
         " -p PAL_FILE    output palette file (default: \"%s\")\n"
         " -v             verbose progress output\n"
         "\n"
         "File arguments:\n"
         " PNG images      input PNG images to encode\n",
         default_args.pal_entries, default_args.pal_filename);
   exit(1);
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], arg_config *config)
{
   int i;
   // allocate max input files
   config->input_files = malloc(sizeof(config->input_files) * argc);
   config->input_count = 0;
   if (argc < 2) {
      print_usage();
      exit(1);
   }
   for (i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case 'e':
               if (++i >= argc) {
                  print_usage();
               }
               config->pal_entries = strtoul(argv[i], NULL, 0);
               break;
            case 'p':
               if (++i >= argc) {
                  print_usage();
               }
               strcpy(config->pal_filename, argv[i]);
               break;
            case 'v':
               g_verbosity = 1;
               break;
            default:
               print_usage();
               break;
         }
      } else {
         // assume input filename
         config->input_files[config->input_count] = argv[i];
         config->input_count++;
      }
   }
   if (config->input_count < 1) {
      print_usage();
   }
}

int main(int argc, char *argv[])
{
   char bin_filename[FILENAME_MAX];
   unsigned char *palette_bin;
   arg_config config;
   palette_t palette;
   image_t *images;
   unsigned pal_length;
   unsigned i;
   int x, y;

   config = default_args;
   parse_arguments(argc, argv, &config);
   INFO("Arguments: \"%s\" %d %d\n", config.pal_filename, config.pal_entries, config.input_count);

   images = malloc(sizeof(*images) * config.input_count);

   // load all images
   for (i = 0; i < config.input_count; i++) {
      images[i].data = pngfile2rgba(config.input_files[i], &images[i].width, &images[i].height);
   }

   // assign colors to palette
   palette.max = config.pal_entries;
   palette.used = 0;
   for (i = 0; i < config.input_count; i++) {
      images[i].ci = malloc(sizeof(*images[i].ci) * images[i].width * images[i].height);
      for (x = 0; x < images[i].width; x++) {
         for (y = 0; y < images[i].height; y++) {
            unsigned img_idx = y * images[i].width + x;
            int pal_idx = pal_add_color(&palette, &images[i].data[img_idx]);
            if (pal_idx < 0) {
               ERROR("Error adding color @ (%d, %d): %d (used: %u)\n", x, y, pal_idx, palette.used);
               exit(1);
            } else {
               images[i].ci[img_idx] = (unsigned char)pal_idx;
            }
         }
      }
   }

   // output bin files
   for (i = 0; i < config.input_count; i++) {
      generate_filename(config.input_files[i], bin_filename, "bin");
      write_file(bin_filename, images[i].ci, images[i].width * images[i].height);
   }

   // output palette file
   pal_length = config.pal_entries*2;
   // unused entries set to 0xFFFF
   palette_bin = malloc(pal_length);
   memset(palette_bin, 0xFF, pal_length);
   for (i = 0; i < palette.used; i++) {
      write_u16_be(&palette_bin[i*2], palette.data[i]);
   }
   write_file(config.pal_filename, palette_bin, pal_length);

   ERROR("Used: %d\n", palette.used);

   free(config.input_files);
   free(palette_bin);
   free(images);

   return 0;
}
