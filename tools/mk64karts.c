#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libmio0.h"
#include "../n64graphics.h"
#include "../utils.h"

#define MK64KARTS_VERSION "0.1"

// first and last MIO0 blocks for kart textures
#define MIO0_FIRST 0x145470
#define MIO0_LAST  0x619730

typedef struct
{
   char png_dir[FILENAME_MAX];
   char rom_filename[FILENAME_MAX];
   unsigned wheel_pal;
} arg_config;

// default configuration
static const arg_config default_args = 
{
   "karts",  // PNG directory
   "",       // ROM filename
   0x1BBEEC, // there are a bunch of these in rom, this is the first one
};

static const unsigned palettes[] = 
{
	0x1E006C, // Luigi
	0x2728A0, // Mario
	0x309DC8, // Yoshi
	0x39DF38, // Peach
	0x43B25C, // Wario
	0x4CBCC8, // Toad
	0x57DCE4, // DK
	0x63E0F8, // Bowser
};

static void print_usage(void)
{
   ERROR("Usage: mk64karts [-d PNG_DIR] [-w WHEEL_PAL] MK64_ROM\n"
         "\n"
         "mk64karts v" MK64KARTS_VERSION ": MK64 kart texture dumper\n"
         "\n"
         "Optional arguments:\n"
         " -d PNG_DIR    output directory for PNG textures (default: \"%s\")\n"
         " -w WHEEL_PAL  wheel palette offset in ROM (default: 0x%X)\n"
         " -v            verbose progress output\n"
         "\n"
         "File arguments:\n"
         " MK64_ROM      input MK64 ROM file\n",
         default_args.png_dir, default_args.wheel_pal);
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
               config->wheel_pal = strtoul(argv[i], NULL, 0);
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
   int width, height;
   unsigned i;
   unsigned pal_count;

   args = default_args;
   parse_arguments(argc, argv, &args);
   INFO("Arguments: \"%s\" \"%s\" 0x%X\n", args.rom_filename, args.png_dir, args.wheel_pal);

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
   pal_count = 0;
   palette = malloc(2*256);
   INFO("Allocating palettes from C: 0x%X W: 0x%X\n", palettes[pal_count], args.wheel_pal);
   memcpy(palette, &rom[palettes[pal_count]], 0x180);
   memcpy(&palette[0x180], &rom[args.wheel_pal], 0x80);

   width = 64;
   height = 64;
   inflated = malloc(width*height);
   for (i = MIO0_FIRST; i <= MIO0_LAST; i += 4) {
      if (!memcmp(&rom[i], "MIO0", 4)) {
         unsigned int end;
         INFO("Inflating MIO0 block 0x%X\n", i);
         int len = mio0_decode(&rom[i], inflated, &end);
         if (len != width*height) {
            ERROR("%X: %X > %X\n", i, len, width*height);
            exit(1);
         }
         if (i > palettes[pal_count]) {
            pal_count++;
            memcpy(palette, &rom[palettes[pal_count]], 0x180);
         }
         sprintf(pngfilename, "%s/%X.png", args.png_dir, i);
         INFO("Converting 0x%X bytes from raw CI to RGBA\n", len);
         imgr = rawci2rgba(inflated, palette, width, height, 16);
         INFO("Writing out PNG \"%s\"\n", pngfilename);
         rgba2png(imgr, width, height, pngfilename);

         i += end - 4;
         i &= ~(0x3); // ensure still aligned
      }
   }

   free(inflated);
   free(palette);

   return 0;
}
