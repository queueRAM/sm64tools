#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsm64.h"
#include "utils.h"

#define SM64EXTEND_VERSION "0.3"

// default configuration
static const sm64_config default_config =
{
   NULL, // input filename
   NULL, // extended filename
   64,   // extended size
   32,   // MIO0 padding
   1,    // MIO0 alignment
   0,    // fill old MIO0 blocks
   0,    // dump MIO0 blocks to files
};

static void print_usage(void)
{
   ERROR("Usage: sm64extend [-a ALIGNMENT] [-p PADDING] [-s SIZE] [-d] [-f] [-v] FILE [OUT_FILE]\n"
         "\n"
         "sm64extend v" SM64EXTEND_VERSION ": Super Mario 64 ROM extender\n"
         "Supports (E), (J), (U), and Shindou ROMs in .n64, .v64, or .z64 formats\n"
         "\n"
         "Optional arguments:\n"
         " -a ALIGNMENT byte boundary to align MIO0 blocks (default: %d)\n"
         " -p PADDING   padding to insert between MIO0 blocks in KB (default: %d)\n"
         " -s SIZE      size of the extended ROM in MB (default: %d)\n"
         " -d           dump MIO0 blocks to files in 'mio0files' directory\n"
         " -f           fill old MIO0 blocks with 0x01\n"
         " -v           verbose progress output\n"
         "\n"
         "File arguments:\n"
         " FILE        input ROM file\n"
         " OUT_FILE    output ROM file (default: replaces FILE extension with .ext.z64)\n",
         default_config.ext_size, default_config.padding, default_config.alignment);
   exit(EXIT_FAILURE);
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], sm64_config *config)
{
   int i;
   int file_count = 0;
   if (argc < 2) {
      print_usage();
   }
   for (i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case 'a':
               if (++i >= argc) {
                  print_usage();
               }
               config->alignment = strtoul(argv[i], NULL, 0);
               break;
            case 'd':
               config->dump = 1;
               break;
            case 'f':
               config->fill = 1;
               break;
            case 'p':
               if (++i >= argc) {
                  print_usage();
               }
               config->padding = strtoul(argv[i], NULL, 0);
               break;
            case 's':
               if (++i >= argc) {
                  print_usage();
               }
               config->ext_size = strtoul(argv[i], NULL, 0);
               break;
            case 'v':
               g_verbosity = 1;
               break;
            default:
               print_usage();
               break;
         }
      } else {
         switch (file_count) {
            case 0:
               config->in_filename = argv[i];
               break;
            case 1:
               config->ext_filename = argv[i];
               break;
            default: // too many
               print_usage();
               break;
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
   char ext_filename[FILENAME_MAX];
   sm64_config config;
   unsigned char *in_buf = NULL;
   unsigned char *out_buf = NULL;
   long in_size;
   long bytes_written;
   rom_type rtype;
   rom_version rversion;

   // get configuration from arguments
   config = default_config;
   parse_arguments(argc, argv, &config);
   if (config.ext_filename == NULL) {
      config.ext_filename = ext_filename;
      generate_filename(config.in_filename, config.ext_filename, "ext.z64");
   }

   // validate arguments
   if (config.ext_size < 16 || config.ext_size > 64) {
      ERROR("Error: Extended size must be between 16 and 64 MB\n");
      exit(EXIT_FAILURE);
   }
   if (!is_power2(config.alignment)) {
      ERROR("Error: Alignment must be power of 2\n");
      exit(EXIT_FAILURE);
   }

   // convert sizes to bytes
   config.ext_size *= MB;
   config.padding *= KB;

   // generate MIO0 directory
   if (config.dump) {
      make_dir(MIO0_DIR);
   }

   // read input file into memory
   in_size = read_file(config.in_filename, &in_buf);
   if (in_size <= 0) {
      ERROR("Error reading input file \"%s\"\n", config.in_filename);
      exit(EXIT_FAILURE);
   }

   // confirm valid SM64
   rtype = sm64_rom_type(in_buf, in_size);
   switch (rtype) {
      case ROM_INVALID:
         ERROR("This does not appear to be a valid SM64 ROM\n");
         exit(EXIT_FAILURE);
         break;
      case ROM_SM64_BS:
         INFO("Converting ROM from byte-swapped to big-endian\n");
         swap_bytes(in_buf, in_size);
         break;
      case ROM_SM64_BE:
         break;
      case ROM_SM64_LE:
         INFO("Converting ROM from little to big-endian\n");
         reverse_endian(in_buf, in_size);
         break;
      case ROM_SM64_BE_EXT:
         ERROR("This ROM is already extended!\n");
         exit(EXIT_FAILURE);
         break;
   }

   rversion = sm64_rom_version(in_buf);
   if (rversion == VERSION_UNKNOWN) {
      ERROR("Unknown SM64 ROM version\n");
      exit(EXIT_FAILURE);
   }

   // allocate output memory
   out_buf = malloc(config.ext_size);

   // copy file from input to output
   memcpy(out_buf, in_buf, in_size);

   // fill new space with 0x01
   memset(&out_buf[in_size], 0x01, config.ext_size - in_size);

   // decode SM64 MIO0 files and adjust pointers
   sm64_decompress_mio0(&config, in_buf, in_size, out_buf);

   // update N64 header CRC
   sm64_update_checksums(out_buf);

   // write to output file
   bytes_written = write_file(config.ext_filename, out_buf, config.ext_size);
   if (bytes_written < (long)config.ext_size) {
      ERROR("Error writing bytes to output file \"%s\"\n", config.ext_filename);
      exit(EXIT_FAILURE);
   }

   return EXIT_SUCCESS;
}
