#include <stdlib.h>

#include "utils.h"

// 802A5E10 (061650)
// just a memcpy from a0 to a3
int decode_block0(unsigned char *in, int length, unsigned char *out)
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
int decode_block1(unsigned char *in, int length, unsigned char *out)
{
   unsigned short t0, t1, t3;
   unsigned char *t2;
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
int decode_block2(unsigned char *in, int length, unsigned char *out)
{
   unsigned char *look;
   unsigned short t0;
   unsigned int t1, t2, t3;
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
int decode_block4(unsigned char *in, int length, unsigned char *out, unsigned char *lut)
{
   unsigned char *look;
   unsigned int t3;
   unsigned short t0, t1, t2;
   int len = 0;
   while (length != 0) {
      t0 = read_u16_be(in);
      in += 2;
      if ((t0 & 0x8000) == 0) {
         t1 = t0 >> 8;
         t2 = t1 & 0xFE;
         look = lut + t2; // t2 += t4; // t4 set in proc_802A57DC: lw    $t4, 0xc($a0)
         t2 = read_u16_be(look);
         t1 &= 1;
         t2 <<= 1;
         t1 |= t2;
         write_u16_be(out, t1);
         out += 2;
         t1 = t0 & 0xFE;
         look = lut + t1;
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
int decode_block5(unsigned char *in, int length, unsigned char *out, unsigned char *lut)
{
   unsigned char *tmp;
   unsigned short t0, t1;
   unsigned int t2, t3;
   int len = 0;
   while (length != 0) {
      t0 = read_u16_be(in);
      in += 2;
      if ((t0 & 0x8000) == 0) { // bltz
         t1 = t0 >> 4;
         t1 = t1 << 1;
         tmp = t1 + lut; // t1 += t4
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
         tmp = out - t0; // t2
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
int decode_block3(unsigned char *in, int length, unsigned char *out)
{
   unsigned short t0, t1, t3;
   unsigned char *t2;
   int len = 0;
   while (length != 0) {
      t0 = read_u16_be(in);
      in += 2;
      if ((0x8000 & t0) == 0) {
         t1 = t0 >> 8;
         t1 <<= 1;
         *out = (unsigned char)t1; // sb
         t1 = t0 & 0xFF;
         t1 <<= 1;
         *(out+1) = (unsigned char)t1; // sb
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
int decode_block6(unsigned char *in, int length, unsigned char *out)
{
   unsigned short t0, t1, t3;
   int len = 0;
// .Lproc_802A5958_20: # 802A5978
   while (length != 0) {
      t0 = read_u16_be(in);
      in += 2;
      if ((0x8000 & t0) == 0) {
         unsigned short t2;
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
         unsigned char *t2;
         t1 = t0 & 0x1F;
         t0 = t0 & 0x7FFF;
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

int blast_decode_file(char *in_filename, int type, char *out_filename, unsigned char *lut)
{
   unsigned char *in_buf = NULL;
   unsigned char *out_buf = NULL;
   int in_len;
   int write_len;
   int out_len = 0;
   int ret_val = 0;

   in_len = read_file(in_filename, &in_buf);
   if (in_len <= 0) {
      return 1;
   }

   // estimate worst case size
   out_buf = malloc(100*in_len);
   if (out_buf == NULL) {
      ret_val = 2;
      goto free_all;
   }

   switch (type) {
      // a0 - input buffer
      // a1 - input length
      // a2 - type (always unused)
      // a3 - output buffer
      // t4 - blocks 4 & 5 reference t4 which is set to FP
      case 0: out_len = decode_block0(in_buf, in_len, out_buf); break;
      case 1: out_len = decode_block1(in_buf, in_len, out_buf); break;
      case 2: out_len = decode_block2(in_buf, in_len, out_buf); break;
      // TODO: need to figure out where last param is set for decoders 4 and 5
      case 4: out_len = decode_block4(in_buf, in_len, out_buf, lut); break;
      case 5: out_len = decode_block5(in_buf, in_len, out_buf, lut); break;
      case 3: out_len = decode_block3(in_buf, in_len, out_buf); break;
      case 6: out_len = decode_block6(in_buf, in_len, out_buf); break;
      default: ERROR("Unknown Blast type %d\n", type); break;
   }

   write_len = write_file(out_filename, out_buf, out_len);
   if (write_len != out_len) {
      ret_val = 2;
   }

free_all:
   if (out_buf) {
      free(out_buf);
   }
   if (in_buf) {
      free(in_buf);
   }

   return ret_val;
}

#ifdef BLAST_STANDALONE
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "n64graphics.h"

typedef struct
{
   unsigned char *w0; // source ptr
   unsigned int w4;   // length
   unsigned int w8;   // type
   unsigned int wC;
} block_t;

// 802A57DC (06101C)
// a0 is only real parameters in ROM
int proc_802A57DC(block_t *a0, unsigned char **copy, unsigned char *rom)
{
   unsigned char *src;
   unsigned int len;
   unsigned int type;
   int v0 = -1;

   len = a0->w4;
   *copy = malloc(100*len);
   src = a0->w0;

   type = a0->w8;
   switch (type) {
      // a0 - input buffer
      // a1 - input length
      // a2 - type (always unused)
      // a3 - output buffer
      // t4 - blocks 4 & 5 reference t4 which is set to FP
      case 0: v0 = decode_block0(src, len, *copy); break;
      case 1: v0 = decode_block1(src, len, *copy); break;
      case 2: v0 = decode_block2(src, len, *copy); break;
      // TODO: need to figure out where last param is set for decoders 4 and 5
      case 4: v0 = decode_block4(src, len, *copy, &rom[0x047480]); break;
      //case 5: v0 = decode_block5(src, len, *copy, &rom[0x0998E0]); break;
      case 5: v0 = decode_block5(src, len, *copy, &rom[0x152970]); break;
      //case 5: v0 = decode_block5(src, len, *copy, &rom[0x1E2C00]); break;
      case 3: v0 = decode_block3(src, len, *copy); break;
      case 6: v0 = decode_block6(src, len, *copy); break;
      default: printf("Need type %d\n", type); break;
   }
   return v0;
}

static void convert_to_png(char *fname, unsigned short len, unsigned short type)
{
   char pngname[512];
   int height, width, depth;
   rgba *rimg;
   ia   *img;
   generate_filename(fname, pngname, "png");
   switch (type) {
      case 0:
         // TODO: memcpy, no info
         break;
      case 1:
         // guess at dims
         switch (len) {
            case 16:   width = 4;  height = 2;  break;
            case 512:  width = 16; height = 16; break;
            case 1*KB: width = 16; height = 32; break;
            case 2*KB: width = 32; height = 32; break;
            case 4*KB: width = 64; height = 32; break;
            case 8*KB: width = 64; height = 64; break;
            default:   width = 32; height = len/width/2; break;
         }
         // RGBA16
         rimg = file2rgba(fname, 0, width, height, 16);
         if (rimg) rgba2png(rimg, width, height, pngname);
         break;
      case 2:
         // guess at dims
         switch (len) {
            case 1*KB: width = 16; height = 16; break;
            case 2*KB: width = 16; height = 32; break;
            case 4*KB: width = 32; height = 32; break;
            case 8*KB: width = 64; height = 32; break;
            default: width = 32; height = len/width/4; break;
         }
         // RGBA32
         rimg = file2rgba(fname, 0, width, height, 32);
         if (rimg) rgba2png(rimg, width, height, pngname);
         break;
      case 3:
         // guess at dims
         switch (len) {
            case 1*KB: width = 32; height = 32; break;
            case 2*KB: width = 32; height = 64; break;
            case 4*KB: width = 64; height = 64; break;
            default: width = 32; height = len/width; break;
         }
         // IA8
         img = file2ia(fname, 0, width, height, 8);
         if (img) ia2png(img, width, height, pngname);
         break;
      case 4:
         // guess at dims
         switch (len) {
            case 1*KB: width = 32; height = 16; break;
            case 2*KB: width = 32; height = 32; break;
            case 4*KB: width = 32; height = 64; break;
            case 8*KB: width = 64; height = 64; break;
            default: width = 32; height = len/width/2; break;
         }
         // IA16
         img = file2ia(fname, 0, width, height, 16);
         if (img) ia2png(img, width, height, pngname);
         break;
      case 5:
         // guess at dims
         switch (len) {
            case 1*KB: width = 16; height = 16; break;
            case 2*KB: width = 32; height = 16; break;
            case 4*KB: width = 32; height = 32; break;
            case 8*KB: width = 64; height = 32; break;
            default: width = 32; height = len/width/2; break;
         }
         // RGBA32
         rimg = file2rgba(fname, 0, width, height, 32);
         if (rimg) rgba2png(rimg, width, height, pngname);
         break;
      case 6:
         // guess at dims
         depth = 8;
         width = 16;
         height = (len*8/depth)/width;
         // IA8
         img = file2ia(fname, 0, width, height, depth);
         if (img) ia2png(img, width, height, pngname);
         break;
   }
}

int main(int argc, char *argv[])
{
#define ROM_OFFSET 0x4CE0
#define END_OFFSET 0xCCE0
   block_t block;
   char out_fname[512];
   unsigned char *data;
   long size;
   int out_size;
   unsigned int off;
   unsigned char *out;
   int width, height, depth;
   char *format;

   if (argc < 2) return 1;

   // read in Blast Corps ROM
   size = read_file(argv[1], &data);

   // loop through from 0x4CE0 to 0xCCE0
   for (off = ROM_OFFSET; off < END_OFFSET; off += 8) {
      unsigned int start = read_u32_be(&data[off]);
      unsigned short len   = read_u16_be(&data[off+4]);
      unsigned short type  = read_u16_be(&data[off+6]);
      assert(size >= start);
      // TODO: there are large sections of len=0, possibly LUTs for 4 & 5?
      if (len > 0) {
         block.w0 = &data[start+ROM_OFFSET];
         block.w4 = len;
         block.w8 = type;
         //printf("%X (%X) %X %d\n", start, start+ROM_OFFSET, len, type);
         out_size = proc_802A57DC(&block, &out, data);
         sprintf(out_fname, "%s.%06X.%d.bin",
               argv[1], start, type);
         //printf("writing %s: %04X -> %04X\n", out_fname, len, out_size);
         depth = 0;
         switch (type) {
            case 0:
               // TODO: memcpy, no info
               break;
            case 1:
               // guess at dims
               switch (out_size) {
                  case 16:   width = 4;  height = 2;  break;
                  case 512:  width = 16; height = 16; break;
                  case 1*KB: width = 16; height = 32; break;
                  case 2*KB: width = 32; height = 32; break;
                  case 4*KB: width = 64; height = 32; break;
                  case 8*KB: width = 64; height = 64; break;
                  case 3200: width = 40; height = 40; break;
                  default:   width = 32; height = out_size/width/2; break;
               }
               format = "\"rgba\"";
               depth = 16;
               break;
            case 2:
               // guess at dims
               switch (out_size) {
                  case 256:  width = 8;  height = 8;  break;
                  case 512:  width = 8;  height = 16; break;
                  case 1*KB: width = 16; height = 16; break;
                  case 2*KB: width = 16; height = 32; break;
                  case 4*KB: width = 32; height = 32; break;
                  case 8*KB: width = 64; height = 32; break;
                  default: width = 32; height = out_size/width/4; break;
               }
               format = "\"rgba\"";
               depth = 32;
               break;
            case 3:
               // guess at dims
               switch (out_size) {
                  case 1*KB: width = 32; height = 32; break;
                  case 2*KB: width = 32; height = 64; break;
                  case 4*KB: width = 64; height = 64; break;
                  default: width = 32; height = out_size/width; break;
               }
               format = "\"ia\"";
               depth = 8;
               break;
            case 4:
               // guess at dims
               switch (out_size) {
                  case 1*KB: width = 16; height = 32; break;
                  case 2*KB: width = 32; height = 32; break;
                  case 4*KB: width = 32; height = 64; break;
                  case 8*KB: width = 64; height = 64; break;
                  default: width = 32; height = out_size/width/2; break;
               }
               format = "\"ia\"";
               depth = 16;
               break;
            case 5:
               // guess at dims
               switch (out_size) {
                  case 1*KB: width = 16; height = 16; break;
                  case 2*KB: width = 32; height = 16; break;
                  case 4*KB: width = 32; height = 32; break;
                  case 8*KB: width = 64; height = 32; break;
                  default: width = 32; height = out_size/width/4; break;
               }
               format = "\"rgba\"";
               depth = 32;
               break;
            case 6:
               // guess at dims
               depth = 8;
               width = 16;
               height = (out_size*8/depth)/width;
               format = "\"ia\"";
               break;
         }
         if (depth) {
            if (width == 0 || height == 0) {
               ERROR("Error: %d x %d for %X at %X type %d\n", width, height, out_size, start+ROM_OFFSET, type);
            }
            printf("   (0x%06X, 0x%06X, \"blast\", %d, ((0x0, %6s, %2d, %2d, %2d))),\n",
                  start+ROM_OFFSET, start+ROM_OFFSET+len, type, format, depth, width, height);
         }
         write_file(out_fname, out, out_size);
         // attempt to convert to PNG
         convert_to_png(out_fname, out_size, type);
      }
   }

   free(data);

   return 0;
}

#endif // BLAST_STANDALONE
