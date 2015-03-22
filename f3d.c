#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"

#define read_u24_be(buf) (unsigned int)(((buf)[0] << 16) + ((buf)[1] << 8) + ((buf)[2]))

#define F3D_MOVEMEM    0x03
#define F3D_VTX        0x04
#define F3D_DL         0x05
#define F3D_QUAD       0xB5
#define F3D_CLRGEOMODE 0xB6
#define F3D_SETGEOMODE 0xB7
#define F3D_ENDDL      0xB8
#define F3D_TEXTURE    0xBB
#define F3D_TRI1       0xBF
#define G_SETTILESIZE  0xF2
#define G_LOADBLOCK    0xF3
#define G_SETTILE      0xF5
#define G_SETFOGCOLOR  0xF8
#define G_SETENVCOLOR  0xFB
#define G_SETCOMBINE   0xFC
#define G_SETTIMG      0xFD

void get_mode_string(unsigned char *data, char *description)
{
   unsigned int val = read_u32_be(&data[4]);
   switch (val) {
      case 0x00022000: sprintf(description, "vertex RGB, no culling"); break;
      case 0x00020000: sprintf(description, "vertex RGB, culling"); break;
      case 0x00000000: sprintf(description, "no vertex RGB, culling"); break;
      case 0x00002200: sprintf(description, "no vertex RGB, no culling"); break;
      default: sprintf(description, "unknown"); break;
   }
}

void print_f3d(unsigned char *data)
{
   char description[64];
   unsigned char bank;
   unsigned char bank2;
   unsigned int address;
   unsigned int val;
   switch (data[0]) {
      case F3D_MOVEMEM:
         switch (data[1]) {
            case 0x86: sprintf(description, "light"); break;
            case 0x88: sprintf(description, "dark "); break;
            default:   sprintf(description, "unkwn"); break;
         }
         bank = data[4];
         address = read_u24_be(&data[5]);
         printf("%14s %s %02X %06X\n", "F3D_MOVEMEM", description, bank, address);
         break;
      case F3D_VTX:
         bank  = data[1];
         val = read_u24_be(&data[2]) / 0x10;
         bank2 = data[4];
         address = read_u24_be(&data[5]);
         printf("%14s %02X %06X %02X %06X\n", "F3D_VTX", bank, val, bank2, address);
         break;
      case F3D_DL:
         bank = data[4];
         address = read_u24_be(&data[5]);
         printf("%14s %02X %06X\n", "F3D_DL", bank, address);
         break;
      case F3D_QUAD:
      {
         unsigned char vertex[6];
         vertex[0] = data[1] / 0x0A;
         vertex[1] = data[2] / 0x0A;
         vertex[2] = data[3] / 0x0A;
         // data[6] unused
         vertex[3] = data[5] / 0x0A;
         vertex[4] = data[6] / 0x0A;
         vertex[5] = data[7] / 0x0A;
         printf("%14s %3d %3d %3d %3d %3d %3d\n", "F3D_QUAD",
               vertex[0], vertex[1], vertex[2],
               vertex[3], vertex[4], vertex[5]);
         break;
      }
      case F3D_CLRGEOMODE:
         get_mode_string(data, description);
         printf("%14s %s\n", "F3D_CLRGEOMODE", description);
         break;
      case F3D_SETGEOMODE:
         get_mode_string(data, description);
         printf("%14s %s\n", "F3D_SETGEOMODE", description);
         break;
      case F3D_ENDDL:
         printf("%14s\n", "F3D_ENDL");
         break;
      case F3D_TEXTURE:
         sprintf(description, "unknown");
         switch (data[3]) {
            case 0x00:
               val = read_u32_be(&data[4]);
               if (val == 0xFFFFFFFF) {
                  sprintf(description, "end, reset scale to 0");
               }
               break;
            case 0x01:
               val = read_u32_be(&data[4]);
               if (val == 0xFFFFFFFF) {
                  sprintf(description, "start, set scale to 1");
               } else if (val == 0x0F8007C0) {
                  sprintf(description, "start environment mapping");
               }
               break;
         }
         printf("%14s %s\n", "F3D_TEXTURE", description);
         break;
      case F3D_TRI1:
      {
         unsigned char vertex[3];
         vertex[0] = data[5] / 0x0A;
         vertex[1] = data[6] / 0x0A;
         vertex[2] = data[7] / 0x0A;
         printf("%14s %3d %3d %3d\n", "F3D_TRI1", vertex[0], vertex[1], vertex[2]);
         break;
      }
      case G_SETTILESIZE:
      {
         unsigned short width, height;
         width  = (((data[5] << 8) | (data[6] & 0xF0)) >> 6) + 1;
         height = (((data[6] & 0x0F) << 8 | data[7]) >> 2) + 1;
         printf("%14s %2d %2d\n", "G_SETTILESIZE", width, height);
         break;
      }
      case G_LOADBLOCK:
         val = read_u32_be(&data[4]);
         switch (val) {
            case 0x077FF100: sprintf(description, "RGBA 32x64 or 64x32"); break;
            case 0x073FF100: sprintf(description, "RGBA 32x32"); break;
            default: sprintf(description, "unknown"); break;
         }
         printf("%14s %s\n", "G_LOADBLOCK", description);
         break;
      case G_SETTILE:
      {
         struct {unsigned char data[7]; char *description;} table[] = 
         {
            {{0x10, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00}, "normal RGBA"},
            {{0x70, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00}, "grayscale"},
            {{0x10, 0x10, 0x00, 0x07, 0x01, 0x40, 0x50}, "normal 32x32"},
            {{0x10, 0x20, 0x00, 0x07, 0x01, 0x40, 0x60}, "normal 64x32"},
            {{0x70, 0x10, 0x00, 0x07, 0x01, 0x40, 0x50}, "grayscale 32x32"},
         };
         unsigned i;
         sprintf(description, "unknown");
         for (i = 0; i < DIM(table); i++) {
            if (!memcmp(table[i].data, &data[1], 7)) {
               strcpy(description, table[i].description);
            }
         }
         printf("%14s %s\n", "G_SETTILE", description);
         break;
      }
      case G_SETFOGCOLOR:
         printf("%14s %3d, %3d, %3d, %3d\n", "G_SETFOGCOLOR", data[4], data[5], data[6], data[7]);
         break;
      case G_SETENVCOLOR:
         printf("%14s %3d, %3d, %3d, %3d\n", "G_SETENVCOLOR", data[4], data[5], data[6], data[7]);
         break;
      case G_SETCOMBINE:
      {
         struct {unsigned char data[7]; char *description;} table[] = 
         {
            {{0x12, 0x7F, 0xFF, 0xFF, 0xFF, 0xF8, 0x38}, "solid RGBA"},
            {{0x12, 0x18, 0x24, 0xFF, 0x33, 0xFF, 0xFF}, "alpha RGBA"},
         };
         unsigned i;
         sprintf(description, "unknown");
         for (i = 0; i < DIM(table); i++) {
            if (!memcmp(table[i].data, &data[1], 7)) {
               strcpy(description, table[i].description);
            }
         }
         printf("%14s %s\n", "G_SETCOMBINE", description);
         break;
      }
      case G_SETTIMG:
         bank = data[4];
         address = read_u24_be(&data[5]);
         printf("%14s %02X %06X\n", "G_SETTIMG", bank, address);
         break;
      default:
         //printf("Unkown %02X\n", data[0]);
         break;
   }
}

int main(int argc, char *argv[])
{
   unsigned char *data;
   long size;
   int i;
   int f;
   if (argc < 2) {
      ERROR("Usage: f3d FILE [FILES...]\n");
      return 1;
   }

   for (f = 1; f < argc; f++) {
      if (argc > 2) {
         printf("%s:\n", argv[f]);
      }
      size = read_file(argv[f], &data);
      if (size < 0) {
         return 2;
      }

      for (i = 0; i < size; i += 8) {
         print_f3d(&data[i]);
      }

      free(data);
   }

   return 0;
}
