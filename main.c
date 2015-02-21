#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libmio0.h"
#include "sm64_tools.h"
#include "utils.h"

// default extended ROM size
#define DEFAULT_OUT_SIZE (64 * MB)

// generate an output file name from input name by replacing
// file extension with .ext.z64
static void generate_filename(const char *in_name, char *out_name)
{
   char tmp_name[256];
   int len;
   int i;
   strcpy(tmp_name, in_name);
   len = strlen(tmp_name);
   for (i = len - 1; i > 0; i--) {
      if (tmp_name[i] == '.') {
         break;
      }
   }
   if (i <= 0) {
      i = len;
   }
   tmp_name[i] = '\0';
   sprintf(out_name, "%s.ext.z64", tmp_name);
}

int main(int argc, char *argv[])
{
   char out_file[256];
   char *in_file = NULL;
   unsigned char *in_buf = NULL;
   unsigned char *out_buf = NULL;
   long in_size;
   long out_size = DEFAULT_OUT_SIZE;

   if (argc < 2) {
      fprintf(stderr, "Usage: m64extend <in_file>\n");
      exit(1);
   }

   in_file = argv[1];
   generate_filename(in_file, out_file);

   // allocate output memory
   out_buf = malloc(out_size);

   // read input file into memory
   in_size = read_file(in_file, &in_buf);

   // if necessary, swap bytes
   if (in_buf[0] == 0x37)  {
      swap_bytes(in_buf, in_size);
   }

   // copy file from input to output
   memcpy(out_buf, in_buf, in_size);

   // fill new space with 0x01
   memset(&out_buf[in_size], 0x01, out_size - in_size);

   // decode SM64 MIO0 files and adjust pointers
   sm64_decompress_mio0(in_buf, in_size, out_buf);

   // update N64 header CRC
   sm64_update_checksums(out_buf);

   // write to output file
   write_file(out_file, out_buf, out_size);

   return 0;
}
