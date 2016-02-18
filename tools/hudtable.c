#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../utils.h"

#define NUM_COLS 4
#define NUM_ROWS (0x80/NUM_COLS)

char lookupchar(int hex)
{
   if (hex >= ' ' && hex <= '~') {
      return (char)hex;
   }
   return ' ';
}

int convertchar(int ascii)
{
   if (ascii >= 0x41 && ascii < 0x5B) {
      return ascii - 0x37;
   } else if (ascii >= 0x61 && ascii < 0x7B) {
      return ascii - 0x57;
   } else if (ascii >= 0x30 && ascii < 0x3A) {
      return ascii - 0x30;
   } else {
      switch (ascii) {
         case 0x20: return -1; break;
         case 0x21: return 0x24; break;
         case 0x23: return 0x25; break;
         case 0x3F: return 0x26; break;
         case 0x26: return 0x27; break;
         case 0x25: return 0x28; break;
         case 0x2A: return 0x32; break;
         case 0x2B: return 0x33; break;
         case 0x2C: return 0x34; break;
         case 0x2D: return 0x35; break;
         case 0x2E: return 0x36; break;
         case 0x2F: return 0x37; break;
      }
   }
   return -1;
}

int main(int argc, char *argv[])
{
   unsigned char *data;
   long len = read_file("sm64.split/bin/font_graphics.bin", &data);
   unsigned int r, c, hexchar;
   int convchar;
   unsigned int seg;
   unsigned int offset;
   char printchar;
   char printstr[8];

   for (c = 0; c < NUM_COLS - 1; c++) {
      printf("^  Hex  ^  A  ^  Index  ^  Seg Addr  ^  T  ");
   }
   printf("^\n");
   for (r = 0; r < 0x80/NUM_COLS; r++) {
      for (c = 1; c < NUM_COLS; c++) {
         hexchar = r + NUM_ROWS*c;
         convchar = convertchar(hexchar);
         if (convchar == -1) {
            seg = 0;
         } else {
            seg = read_u32_be(&data[0x7700 + 4*convchar]);
         }
         offset = 0xFFFFFF & seg;
         printchar = lookupchar(hexchar);
         if (printchar == '|' || printchar == '^') {
            sprintf(printstr, "%%%%%c%%%%", printchar);
         } else {
            sprintf(printstr, "%c", printchar);
         }
         printf("|  %02X  |  %s  | ", hexchar, printstr);
         if (seg == 0) {
            if (convchar == -1) {
               printf("  -  |  -  | ");
            } else {
               printf("  %02X  |  %08X  | ", convchar, seg);
            }
         } else {
            printf("  %02X  |  %08X  |  {{http://queueram.com/n64/sm64/textures/textures/font_graphics.0x%05X.png}}  ", convchar, seg, offset);
         }
      }
      printf("|\n");
   }
   free(data);
   return 0;
}
