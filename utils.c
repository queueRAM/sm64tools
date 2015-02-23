#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

// global verbosity setting
int g_verbosity = 0;

int is_power2(unsigned int val)
{
   while (((val & 1) == 0) && (val > 1)) {
      val >>= 1;
   }
   return (val == 1);
}

void print_hex(unsigned char *buf, int length)
{
   int i;
   for (i = 0; i < length; i++) {
      print_byte(buf[i]);
   }
}

void swap_bytes(unsigned char *data, long length)
{
   long i;
   unsigned char tmp;
   for (i = 0; i < length; i += 2) {
      tmp = data[i];
      data[i] = data[i+1];
      data[i+1] = tmp;
   }
}

long read_file(const char *file_name, unsigned char **data)
{
   FILE *in;
   unsigned char *in_buf = NULL;
   long file_size;
   long bytes_read;
   in = fopen(file_name, "rb");
   if (in == NULL) {
      return -1;
   }

   // allocate buffer to read from offset to end of file
   fseek(in, 0, SEEK_END);
   file_size = ftell(in);
   in_buf = malloc(file_size);
   fseek(in, 0, SEEK_SET);

   // read bytes
   bytes_read = fread(in_buf, 1, file_size, in);
   if (bytes_read != file_size) {
      return -2;
   }

   fclose(in);
   *data = in_buf;
   return bytes_read;
}

long write_file(const char *file_name, unsigned char *data, long length)
{
   FILE *out;
   long bytes_written;
   // open output file
   out = fopen(file_name, "wb");
   if (out == NULL) {
      return -1;
   }
   bytes_written = fwrite(data, 1, length, out);
   fclose(out);
   return bytes_written;
}

