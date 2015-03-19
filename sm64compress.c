#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsm64.h"
#include "utils.h"

#define SM64COMPRESS_VERSION "0.1"

// default configuration
static const sm64_config_t default_config = 
{
   NULL, // input filename
   NULL, // output filename
   0,    // output size: unused in compress
   0,    // MIO0 padding: unused in compress
   16,   // MIO0 alignment
   0,    // TODO: fill old MIO0 blocks
   0,    // compress all MIO0 blocks
   0,    // TODO: dump
};

static void print_usage(void)
{
   ERROR("Usage: sm64compress [-c] [-d] [-v] FILE [OUT_FILE]\n"
         "\n"
         "sm64compress v" SM64COMPRESS_VERSION ": Super Mario 64 ROM compressor\n"
         "\n"
         "Optional arguments:\n"
         " -a ALIGNMENT byte boundary to align MIO0 blocks (default: %d)\n"
         " -c           compress all blocks using MIO0\n"
         " -d           dump MIO0 blocks to files in mio0 directory\n"
         " -v           verbose progress output\n"
         "\n"
         "File arguments:\n"
         " FILE         input ROM file\n"
         " OUT_FILE     output shrunk ROM file (default: replaces input extension with .out.z64)\n",
         default_config.alignment);
   exit(1);
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], sm64_config_t *config)
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
            case 'a':
               if (++i >= argc) {
                  print_usage();
               }
               config->alignment = strtoul(argv[i], NULL, 0);
               if (!is_power2(config->alignment)) {
                  ERROR("Error: Alignment must be power of 2\n");
                  exit(2);
               }
               break;
            case 'c':
               config->compress = 1;
               break;
            case 'd':
               config->dump = 1;
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
   sm64_config_t config;
   unsigned char *in_buf = NULL;
   unsigned char *out_buf = NULL;
   long in_size;
   long bytes_written;
   int out_size;

   // get configuration from arguments
   config = default_config;
   parse_arguments(argc, argv, &config);
   if (config.ext_filename == NULL) {
      config.ext_filename = ext_filename;
      generate_filename(config.in_filename, config.ext_filename, "out.z64");
   }

   // read input file into memory
   in_size = read_file(config.in_filename, &in_buf);
   if (in_size <= 0) {
      ERROR("Error reading input file \"%s\"\n", config.in_filename);
      exit(1);
   }

   // TODO: confirm valid SM64

   // allocate output memory
   out_buf = malloc(in_size);

   // copy base file from input to output
   // TODO: hardcoded length
   memcpy(out_buf, in_buf, 8*MB);

   // compact the SM64 MIO0 files and adjust pointers
   out_size = sm64_compress_mio0(&config, in_buf, in_size, out_buf);

   // update N64 header CRC
   sm64_update_checksums(out_buf);

   // write to output file
   bytes_written = write_file(config.ext_filename, out_buf, out_size);
   if (bytes_written < out_size) {
      ERROR("Error writing bytes to output file \"%s\"\n", config.ext_filename);
      exit(1);
   }

   return 0;
}
