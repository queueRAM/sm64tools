#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "utils.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef struct
{
   u8 *w0; // source ptr
   u32 w4; // length
   u32 w8; // type
   u32 wC;
} block_t;

// 802A5E10 (061650)
// just a memcpy from a0 to a3
int decode_block0(u8 *in, int length, int type, u8 *out)
{
   int ret_len = length;
   length >>= 3; // a1 - gets number of dwords
   while (length != 0) {
      int i;
      for (i = 0; i < 8; i++) {
         out[i] = in[i];
      }
      in += 8;
      out += 8;
      length--;
   }
   return ret_len;
}

// 802A5AE0 (061320)
int decode_block1(u8 *in, int length, int type, u8 *out)
{
   u16 t0, t1, t3;
   u8 *t2;
   int len = 0;
   while (length != 0) {
      t0 = read_u16_be(in); // a0
      in += 2; // a0
      if ((t0 & 0x8000) == 0) {
         t1 = (t0 & 0xFFC0) << 1;
         t0 &= 0x3F;
         t0 = t0 | t1;
         write_u16_be(out, t0);
         out += 2; // a3
         len += 2;
         length -= 2; // a1
      } else {
         t1 = t0 & 0x1F; // lookback length
         t0 = (t0 & 0x7FFF) >> 5; // lookback offset
         length -= 2; // a1
         t2 = out - t0; // t2 - lookback pointer from current out
         while (t1 != 0) {
            t3 = read_u16_be(t2);
            t2 += 2;
            out += 2; // a3
            len += 2;
            t1 -= 1;
            write_u16_be(out-2, t3);
         }
      }
   }
   return len;
}

// 802A5B90 (0613D0)
int decode_block2(u8 *in, int length, int type, u8 *out)
{
   u8 *look;
   u16 t0;
   u32 t1, t2, t3;
   int len = 0;
   while (length != 0) {
      t0 = read_u16_be(in);
      in += 2;
      if ((t0 & 0x8000) == 0) { // t0 >= 0
         t1 = t0 & 0x7800;
         t2 = t0 & 0x0780;
         t1 <<= 17; // 0x11
         t2 <<= 13; // 0xD;
         t1 |= t2;
         t2 = t0 & 0x78;
         t2 <<= 9;
         t1 |= t2;
         t2 = t0 & 7;
         t2 <<= 5;
         t1 |= t2;
         write_u32_be(out, t1);
         out += 4;
         length -= 2;
         len += 4;
      } else {
         t1 = t0 & 0x1f;
         t0 &= 0x7FE0;
         t0 >>= 4;
         length -= 2;
         look = out - t0; // t2
         while (t1 != 0) {
            t3 = read_u32_be(look); // lw t2
            write_u32_be(out, t3);
            look += 4; // t2
            t1 -= 1;
            out += 4;
            len += 4;
         }
      }
   }
   return len;
}

// 802A5C5C (06149C)
int decode_block4(u8 *in, int length, int type, u8 *out, u8 *other)
{
   u8 *look;
   u32 t3;
   u16 t0, t1, t2;
   int len = 0;
   while (length != 0) {
      t0 = read_u16_be(in);
      in += 2;
      if ((t0 & 0x8000) == 0) {
         t1 = t0 >> 8;
         t2 = t1 & 0xFE;
         look = other + t2; // t2 += t4; // t4 set in proc_802A57DC: lw    $t4, 0xc($a0)
         t2 = read_u16_be(look);
         t1 &= 1;
         t2 <<= 1;
         t1 |= t2;
         write_u16_be(out, t1);
         out += 2;
         t1 = t0 & 0xFE;
         look = other + t1;
         t1 = read_u16_be(look);
         t0 &= 1;
         length -= 2;
         t1 <<= 1;
         t1 |= t0;
         write_u16_be(out, t1);
         out += 2;
         len += 4;
      } else {
         t1 = t0 & 0x1F;
         t0 &= 0x7FE0;
         t0 >>= 4;
         length -= 2;
         look = out - t0;
         while (t1 != 0) {
            t3 = read_u32_be(look);
            look += 4;
            t1 -= 1;
            write_u32_be(out, t3);
            out += 4;
            len += 4;
         }
      }
   }
   return len;
}

// 802A5D34 (061574)
int decode_block5(u8 *in, int length, int type, u8 *out, u8 *other)
{
   u8 *tmp;
   u16 t0, t1;
   u32 t2, t3;
   int len = 0;
   while (length != 0) {
      t0 = read_u16_be(in);
      in += 2;
      if ((t0 & 0x8000) == 0) { // bltz
         t1 = t0 >> 4;
         t1 = t1 << 1;
         tmp = t1 + other; // t1 += t4
         t1 = read_u16_be(tmp);
         t0 &= 0xF;
         t0 <<= 4;
         t2 = t1 & 0x7C00;
         t3 = t1 & 0x03E0;
         t2 <<= 17; // 0x11
         t3 <<= 14; // 0xe
         t2 |= t3;
         t3 = t1 & 0x1F;
         t3 <<= 11; // 0xb
         t2 |= t3;
         t2 |= t0;
         write_u32_be(out, t2);
         out += 4;
         length -= 2;
         len += 4;
      } else {
         t1 = t0 & 0x1F;
         t0 &= 0x7FE0;
         t0 >>= 4;
         length -= 2;
         tmp = out + t0; // t2
         while (t1 != 0) {
            t3 = read_u32_be(tmp); //t2
            tmp += 4; // t2
            t1 -= 1;
            write_u32_be(out, t3);
            out += 4;
            len += 4;
         }
      }
   }
   return len;
}

// 802A5A2C (06126C)
int decode_block3(u8 *in, int length, int type, u8 *out)
{
   u16 t0, t1, t3;
   u8 *t2;
   int len = 0;
   while (length != 0) {
      t0 = read_u16_be(in);
      in += 2;
      if ((0x8000 & t0) == 0) {
         t1 = t0 >> 8;
         t1 <<= 1;
         *out = (u8)t1; // sb
         t1 = t0 & 0xFF;
         t1 <<= 1;
         *(out+1) = (u8)t1; // sb
         out += 2;
         length -= 2;
         len += 2;
      } else {
         t1 = t0 & 0x1F;
         t0 &= 0x7FFF;
         t0 >>= 5;
         length -= 2;
         t2 = out - t0;
         while (t1 != 0) {
            t3 = read_u16_be(t2);
            t2 += 2;
            t1 -= 1;
            write_u16_be(out, t3);
            out += 2;
            len += 2;
         }
      }
   }
   return len;
}

// 802A5958 (061198)
int decode_block6(u8 *in, int length, int type, u8 *out)
{
   u16 t0, t1, t3;
   int len = 0;
// .Lproc_802A5958_20: # 802A5978
   while (length != 0) {
      t0 = read_u16_be(in);
      in += 2;
      if ((0x8000 & t0) == 0) {
         u16 t2;
         t1 = t0 >> 8;
         t2 = t1 & 0x38;
         t1 = t1 & 0x07;
         t2 <<= 2;
         t1 <<= 1;
         t1 |= t2;
         *out = t1; // sb
         t1 = t0 & 0xFF;
         t2 = t1 & 0x38;
         t1 = t1 & 0x07;
         t2 <<= 2;
         t1 <<= 1;
         t1 |= t2;
         *(out+1) = t1;
         out += 2;
         length -= 2;
         len += 2;
      } else {
         u8 *t2;
         t1 = t0 & 0x1F;
         t0 = t0 & 0x7FFF;
         t0 >>= 5;
         length -= 2;
         t2 = out - t0;
         while (t1 != 0) {
            t3 = read_u32_be(t2);
            t2 += 2;
            t1 -= 1;
            write_u32_be(out, t3);
            out += 2;
            len += 2;
         }
      }
   }
   return len;
}

// 802A57DC (06101C)
int proc_802A57DC(block_t *a0, u8 **copy)
{
   u8 *src;
   u8 *other;
   u32 len;
   u32 type;
   int v0 = -1;

   len = a0->w4;
   *copy = malloc(100*len);
   src = a0->w0;
   // TODO: need to figure out where this is set for decoders 4 and 5
   other = calloc(100, len);

   type = a0->w8;
   switch (type) {
      // a0 - input buffer
      // a1 - input length
      // a2 - type (always unused)
      // a3 - output buffer
      // t4 - blocks 4 & 5 reference t4 which is set to FP
      case 0: v0 = decode_block0(src, len, type, *copy); break;
      case 1: v0 = decode_block1(src, len, type, *copy); break;
      case 2: v0 = decode_block2(src, len, type, *copy); break;
      case 4: v0 = decode_block4(src, len, type, *copy, other); break;
      case 5: v0 = decode_block5(src, len, type, *copy, other); break;
      case 3: v0 = decode_block3(src, len, type, *copy); break;
      case 6: v0 = decode_block6(src, len, type, *copy); break;
      default: printf("Need type %d\n", type); break;
   }
   free(other);
   return v0;
}

int main(int argc, char *argv[])
{
#define ROM_OFFSET 0x4CE0
   block_t block;
   char out_fname[512];
   unsigned char *data;
   long size;
   int out_size;
   u32 off;
   u8 *out;

   if (argc < 2) return 1;

   // read in Blast Corps ROM
   size = read_file(argv[1], &data);

   // loop through from 0x4CE0 to +0x5E90
   for (off = ROM_OFFSET; off < ROM_OFFSET+0x5E98; off += 8) {
      u32 start = read_u32_be(&data[off]);
      u16 len   = read_u16_be(&data[off+4]);
      u16 type  = read_u16_be(&data[off+6]);
      assert(size >= start);
      if (len > 0) {
         block.w0 = &data[start+ROM_OFFSET];
         block.w4 = len;
         block.w8 = type;
         printf("%X (%X) %X %d\n", start, start+ROM_OFFSET, len, type);
         out_size = proc_802A57DC(&block, &out);
         sprintf(out_fname, "%s.%06X.%d.decoded",
               argv[1], start, type);
         printf("writing %s: %04X -> %04X\n", out_fname, len, out_size);
         write_file(out_fname, out, out_size);
      }
   }

   free(data);

   return 0;
}
