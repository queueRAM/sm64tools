#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libmio0.h"
#include "../n64graphics.h"
#include "../utils.h"

#define MK64KARTS_VERSION "0.2"

// first and last MIO0 blocks for kart textures
#define MIO0_FIRST 0x145470
#define MIO0_LAST  0x619730

#define WIDTH 64
#define HEIGHT 64

typedef struct
{
   char png_dir[FILENAME_MAX];
   char rom_filename[FILENAME_MAX];
   int wheel;
} arg_config;

// default configuration
static const arg_config default_args = 
{
   "karts",  // PNG directory
   "",       // ROM filename
   0,        // wheel animation index
};

// start of wheel palette data, kart palette is +0x24200
static unsigned palette_groups[] =
{
   0x1BBE6C, // Luigi
   0x24E6A0, // Mario
   0x2E5BC8, // Yoshi
   0x379D38, // Peach
   0x41705C, // Wario
   0x4A7AC8, // Toad
   0x559AE4, // DK
   0x619EF8, // Bowser
};

static void print_usage(void)
{
   ERROR("Usage: mk64karts [-d PNG_DIR] [-w WHEEL] [-v] MK64_ROM\n"
         "\n"
         "mk64karts v" MK64KARTS_VERSION ": MK64 kart texture dumper\n"
         "\n"
         "Optional arguments:\n"
         " -d PNG_DIR    output directory for PNG textures (default: \"%s\")\n"
         " -w WHEEL      wheel animation index [0-3] (default: %d)\n"
         " -v            verbose output\n"
         "\n"
         "File arguments:\n"
         " MK64_ROM      input MK64 ROM file\n",
         default_args.png_dir, default_args.wheel);
   exit(1);
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], arg_config *config)
{
   int i;
   int file_count = 0;
   if (argc < 2) {
      print_usage();
      exit(1);
   }
   for (i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case 'd':
               if (++i >= argc) {
                  print_usage();
               }
               strcpy(config->png_dir, argv[i]);
               break;
            case 'w':
               if (++i >= argc) {
                  print_usage();
               }
               config->wheel = strtol(argv[i], NULL, 0);
               if (config->wheel < 0 || config->wheel > 3) {
                  print_usage();
               }
               break;
            case 'v':
               g_verbosity = 1;
               break;
            default:
               print_usage();
               break;
         }
      } else {
         if (file_count == 0) {
            strcpy(config->rom_filename, argv[i]);
         } else {
            // too many
            print_usage();
         }
         file_count++;
      }
   }
   if (file_count < 1) {
      print_usage();
   }
}

int main(int argc, char *argv[])
{
   char pngfilename[FILENAME_MAX];
   arg_config args;
   char *palette;
   unsigned char *rom;
   unsigned char *inflated;
   rgba *imgr;
   int i;
   int kart;
   int cur_count;

   args = default_args;
   parse_arguments(argc, argv, &args);
   INFO("Arguments: \"%s\" \"%s\" %d\n", args.rom_filename, args.png_dir, args.wheel);

   INFO("Loading \"%s\"\n", args.rom_filename);
   long rom_len = read_file(args.rom_filename, &rom);

   if (rom_len <= 0) {
      ERROR("Error reading in \"%s\"\n", args.rom_filename);
      exit(1);
   }
   INFO("Loaded 0x%lX bytes from \"%s\"\n", rom_len, args.rom_filename);

   // ensure PNG directory exists
   make_dir(args.png_dir);

   // prepare palette - wheels are from index 0xC0-0xFF (offset 0x180-0x1FF)
   kart = -1;
   palette = malloc(2*256);
   inflated = malloc(2*WIDTH*HEIGHT);
   cur_count = 0;
   for (i = MIO0_FIRST; i <= MIO0_LAST; i += 4) {
      if (!memcmp(&rom[i], "MIO0", 4)) {
         unsigned int end;
         if (kart < 0 || i > palette_groups[kart]) {
            kart++;
            cur_count = 0;
            memcpy(palette, &rom[palette_groups[kart] + 0x24200], 0x180);
         }
         unsigned wheel_offset = palette_groups[kart] + 0x80*(4*cur_count + args.wheel);
         memcpy(&palette[0x180], &rom[wheel_offset], 0x80);
         INFO("Inflating MIO0 block 0x%X\n", i);
         int len = mio0_decode(&rom[i], inflated, &end);
         if (len != WIDTH*HEIGHT) {
            ERROR("%X: %X > %X\n", i, len, WIDTH*HEIGHT);
            exit(1);
         }
         sprintf(pngfilename, "%s/%X.png", args.png_dir, i);
         INFO("Converting 0x%X bytes from raw CI to RGBA\n", len);
         imgr = rawci2rgba(inflated, palette, WIDTH, HEIGHT, 16);
         INFO("Writing out PNG \"%s\"\n", pngfilename);
         rgba2png(imgr, WIDTH, HEIGHT, pngfilename);

         i += end - 4;
         i &= ~(0x3); // ensure still aligned
         cur_count++;
      }
   }

   free(inflated);
   free(palette);

   return 0;
}
