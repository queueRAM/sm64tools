#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libmio0.h"
#include "utils.h"

// defines

#define MIO0_HEADER_LENGTH 16

#define GET_BIT(buf, bit) ((buf)[(bit) / 8] & (1 << (7 - ((bit) % 8))))

// functions

static void PUT_BIT(unsigned char *buf, int bit, int val)
{
   unsigned char mask = 1 << (7 - (bit % 8));
   unsigned int offset = bit / 8;
   buf[offset] = (buf[offset] & ~(mask)) | (val ? mask : 0);
}

// used to find longest matching stream in buffer
// buf: buffer
// start_offset: offset in buf to look back from
// max_search: max number of bytes to find
// found_offset: returned offset found (0 if none found)
// returns max length of matching stream (0 if none found)
static int find_longest(const unsigned char *buf, int start_offset, int max_search, int *found_offset)
{
   int length = 0;
   int best_offset = 0;
   int i, j;

   // check at most the past 4096 values
   for (i = MAX(start_offset - 4096, 0); i <= start_offset - 2; i++) {
      for (j = 0; j < MIN(max_search, start_offset - i); j++) {
         if (buf[start_offset + j] != buf[i + j]) {
            break;
         }
      }
      if (j > length) {
         best_offset = start_offset - i;
         length = j;
      }
   }

   // return best reverse offset and length (may be 0)
   *found_offset = best_offset;
   return length;
}

// decode MIO0 header
// returns 1 if valid header, 0 otherwise
int mio0_decode_header(const unsigned char *buf, mio0_header_t *head)
{
   if (!memcmp(buf, "MIO0", 4)) {
      head->dest_size = read_u32_be(&buf[4]);
      head->comp_offset = read_u32_be(&buf[8]);
      head->uncomp_offset = read_u32_be(&buf[12]);
      head->big_endian = 1;
      return 1;
   } else if (!memcmp(buf, "IM0O", 4)) {
      head->dest_size = read_u32_le(&buf[4]);
      head->comp_offset = read_u32_le(&buf[8]);
      head->uncomp_offset = read_u32_le(&buf[12]);
      head->big_endian = 0;
      return 1;
   }
   return 0;
}

// encode MIO0 header
void mio0_encode_header(unsigned char *buf, const mio0_header_t *head)
{
   memcpy(buf, "MIO0", 4);
   write_u32_be(&buf[4], head->dest_size);
   write_u32_be(&buf[8], head->comp_offset);
   write_u32_be(&buf[12], head->uncomp_offset);
}

// decode MIO0 data in memory
// in: buffer containing MIO0 data
// out: buffer for output data - allocated by this function
// returns bytes extracted into output or negative value on failure
int mio0_decode(const unsigned char *in, unsigned char *out)
{
   mio0_header_t head;
   unsigned int bytes_written = 0;
   int bit_idx = 0;
   int comp_idx = 0;
   int uncomp_idx = 0;
   int valid;

   // extract header
   valid = mio0_decode_header(in, &head);
   // verify MIO0 header
   if (!valid) {
      return -2;
   }

   //printf("Found: %s, size = %d, comp_offset = 0x%X, uncomp_offset = 0x%X\n",
   //      head.big_endian ? "BE" : "LE", head.dest_size, head.comp_offset, head.uncomp_offset);

   if (!head.big_endian) {
      printf("Sorry, only big endian supported right now\n");
      return -1;
   }

   // decode data
   while (bytes_written < head.dest_size) {
      if (GET_BIT(&in[16], bit_idx)) {
         // 1 - pull uncompressed data
         out[bytes_written] = in[head.uncomp_offset + uncomp_idx];
         bytes_written++;
         uncomp_idx++;
      } else {
         // 0 - read compressed data
         int idx;
         int length;
         int i;
         const unsigned char *vals = &in[head.comp_offset + comp_idx];
         comp_idx += 2;
         length = ((vals[0] & 0xF0) >> 4) + 3;
         idx = ((vals[0] & 0x0F) << 8) + vals[1] + 1;
         for (i = 0; i < length; i++) {
            out[bytes_written] = out[bytes_written - idx];
            bytes_written++;
         }
      }
      bit_idx++;
   }

   return bytes_written;
}

// encode MIO0 data in memory
// in: buffer containing raw data
// out: buffer for MIO0 data allocated by this function
// returns size of output buffer
int mio0_encode(const unsigned char *in, unsigned int length, unsigned char **out)
{
   unsigned char *bit_buf;
   unsigned char *comp_buf;
   unsigned char *uncomp_buf;
   unsigned char *out_buf;
   unsigned int comp_offset;
   unsigned int uncomp_offset;
   unsigned int bytes_proc = 0;
   int bytes_written;
   int bit_idx = 0;
   int comp_idx = 0;
   int uncomp_idx = 0;

   // allocate some temporary buffers worst case size
   bit_buf = malloc(length / 8); // 1-bit/byte
   comp_buf = malloc(length); // 16-bits/2bytes
   uncomp_buf = malloc(length); // all uncompressed

   // encode data
   while (bytes_proc < length) {
      // special case for first 2 bytes
      if (bytes_proc < 2) {
         memcpy(&uncomp_buf[uncomp_idx], in, 2);
         uncomp_idx += 2;
         bytes_proc += 2;
         PUT_BIT(bit_buf, bit_idx++, 1);
         PUT_BIT(bit_buf, bit_idx++, 1);
      } else {
         int offset;
         int longest_string;
         longest_string = find_longest(in, bytes_proc, MIN(length - bytes_proc, 18), &offset);
         if (longest_string > 2) {
            // compress
            comp_buf[comp_idx] = (((longest_string - 3) & 0x0F) << 4) | 
                                 (((offset - 1) >> 8) & 0x0F);
            comp_buf[comp_idx + 1] = (offset - 1) & 0xFF;
            comp_idx += 2;
            PUT_BIT(bit_buf, bit_idx, 0);
            bytes_proc += longest_string;
         } else {
            // uncompress
            uncomp_buf[uncomp_idx] = in[bytes_proc];
            uncomp_idx++;
            PUT_BIT(bit_buf, bit_idx, 1);
            bytes_proc++;
         }
         bit_idx++;
      }
   }

   // compute final sizes and offsets
   // +7 so int division accounts for all bits
   comp_offset = ((bit_idx + 7) / 8) + MIO0_HEADER_LENGTH;
   uncomp_offset = comp_offset + comp_idx;
   bytes_written = uncomp_offset + uncomp_idx;

   // output header
   out_buf = malloc(bytes_written);
   memcpy(out_buf, "MIO0", 4);
   write_u32_be(&out_buf[4], length);
   write_u32_be(&out_buf[8], comp_offset);
   write_u32_be(&out_buf[12], uncomp_offset);
   // output data
   memcpy(&out_buf[MIO0_HEADER_LENGTH], bit_buf, ((bit_idx + 7) / 8));
   memcpy(&out_buf[comp_offset], comp_buf, comp_idx);
   memcpy(&out_buf[uncomp_offset], uncomp_buf, uncomp_idx);

   *out = out_buf;

   return bytes_written;
}

int mio0_decode_file(const char *in_file, unsigned long offset, const char *out_file)
{
   mio0_header_t head;
   FILE *in;
   FILE *out;
   unsigned char *in_buf = NULL;
   unsigned char *out_buf = NULL;
   long file_size;
   int ret_val = 0;
   size_t bytes_read;
   int bytes_decoded;
   int bytes_written;
   int valid;

   in = fopen(in_file, "rb");
   if (in == NULL) {
      return 1;
   }

   // allocate buffer to read from offset to end of file
   fseek(in, 0, SEEK_END);
   file_size = ftell(in);
   in_buf = malloc(file_size - offset);
   fseek(in, offset, SEEK_SET);

   // read bytes
   bytes_read = fread(in_buf, 1, file_size - offset, in);
   if (bytes_read != file_size - offset) {
      ret_val = 2;
      goto free_all;
   }

   // verify header
   valid = mio0_decode_header(in_buf, &head);
   if (!valid) {
      ret_val = 3;
      goto free_all;
   }
   out_buf = malloc(head.dest_size);

   // decompress MIO0 encoded data
   bytes_decoded = mio0_decode(in_buf, out_buf);
   if (bytes_decoded < 0) {
      ret_val = 3;
      goto free_all;
   }

   // open output file
   out = fopen(out_file, "w");
   if (out == NULL) {
      ret_val = 4;
      goto free_all;
   }

   // write data to file
   bytes_written = fwrite(out_buf, 1, bytes_decoded, out);
   if (bytes_written != bytes_decoded) {
      ret_val = 5;
   }

   // clean up
   fclose(out);
free_all:
   if (out_buf) {
      free(out_buf);
   }
   if (in_buf) {
      free(in_buf);
   }
   fclose(in);

   return ret_val;
}

int mio0_encode_file(const char *in_file, const char *out_file)
{
   FILE *in;
   FILE *out;
   unsigned char *in_buf = NULL;
   unsigned char *out_buf = NULL;
   size_t file_size;
   size_t bytes_read;
   int bytes_encoded;
   int bytes_written;
   int ret_val = 0;

   in = fopen(in_file, "rb");
   if (in == NULL) {
      return 1;
   }

   // allocate buffer to read entire contents of files
   fseek(in, 0, SEEK_END);
   file_size = ftell(in);
   fseek(in, 0, SEEK_SET);
   in_buf = malloc(file_size);

   // read bytes
   bytes_read = fread(in_buf, 1, file_size, in);
   if (bytes_read != file_size) {
      ret_val = 2;
      goto free_all;
   }

   // compress data in MIO0 format
   bytes_encoded = mio0_encode(in_buf, file_size, &out_buf);

   // open output file
   out = fopen(out_file, "wb");
   if (out == NULL) {
      ret_val = 4;
      goto free_all;
   }

   // write data to file
   bytes_written = fwrite(out_buf, 1, bytes_encoded, out);
   if (bytes_written != bytes_encoded) {
      ret_val = 5;
   }

   // clean up
   fclose(out);
free_all:
   if (out_buf) {
      free(out_buf);
   }
   if (in_buf) {
      free(in_buf);
   }
   fclose(in);

   return ret_val;
}

#ifdef MIO0_TEST
int main(int argc, char *argv[])
{
   char out_file_name[256];
   char *in_file = NULL;
   char *out_file = NULL;
   char *op;
   unsigned long offset;
   int ret_val;

   if (argc < 3) {
      printf("Usage: mio0 <op> <in_file> [offset] [out_file]\n");
      exit(1);
   }

   op = argv[1];
   in_file = argv[2];
   sprintf(out_file_name, "%s.out", in_file);
   out_file = out_file_name;
   if (argc > 3) {
      offset = strtol(argv[3], NULL, 0);
      if (argc > 4) {
         out_file = argv[4];
      }
   } else {
      offset = 0;
   }

   // operation
   if (op[0] == 'e') {
      ret_val = mio0_encode_file(in_file, out_file);
   } else {
      ret_val = mio0_decode_file(in_file, offset, out_file);
   }

   switch (ret_val) {
      case 1:
         printf("Error opening input file \"%s\"\n", in_file);
         break;
      case 2:
         printf("Error reading from input file \"%s\"\n", in_file);
         break;
      case 3:
         printf("Error decoding MIO0 data. Wrong offset (0x%lX)?\n", offset);
         break;
      case 4:
         printf("Error opening output file \"%s\"\n", out_file);
         break;
      case 5:
         printf("Error writing bytes to output file \"%s\"\n", out_file);
         break;
   }

   return ret_val;
}
#endif // MIO0_TEST

