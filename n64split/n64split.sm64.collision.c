#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <zlib.h>

#include "config.h"
#include "libblast.h"
#include "libmio0.h"
#include "libsfx.h"
#include "mipsdisasm.h"
#include "n64graphics.h"
#include "strutils.h"
#include "utils.h"

#define N64SPLIT_VERSION "0.4a"

#define GLOBALS_FILE "globals.inc"
#define MACROS_FILE "macros.inc"
#define MUSIC_SUBDIR "music"


typedef struct
{
  unsigned type;
  char *name;
} terrain_t;

static const terrain_t terrain_table[] =
{
   {0x0000, "normal"},
   {0x0001, "lethal_lava"},
   {0x0005, "hang"},
   {0x000A, "deathfloor"},
   {0x000E, "water_currents"},
   {0x0012, "void"},
   {0x0013, "very_slippery"},
   {0x0014, "slippery"},
   {0x0015, "climbable"},
   {0x0028, "wall"},
   {0x0029, "grass"},
   {0x002A, "unclimbable"},
   {0x002C, "windy"},
   {0x002E, "icy"},
   {0x0030, "flat"},
   {0x0036, "snowy"},
   {0x0037, "snowy2"},
   {0x0076, "fence"},
   {0x007B, "vanishing_wall"},
   {0x00FD, "pool_warp"},
};

char *terrain2str(unsigned type)
{
   unsigned i;
   static char retval[16];
   if (0x1B <= type && type <= 0x1E) {
      sprintf(retval, "switch%02X", type);
      return retval;
   } else if (0xA6 <= type && type <= 0xCF) {
      sprintf(retval, "paintingf%02X", type);
      return retval;
   } else if (0xD3 <= type && type <= 0xF8) {
      sprintf(retval, "paintingb%02X", type);
      return retval;
   }
   for (i = 0; i < DIM(terrain_table); i++) {
      if (terrain_table[i].type == type) {
         return terrain_table[i].name;
      }
   }
   sprintf(retval, "%02X", type);
   return retval;
}



int collision2obj(char *binfilename, unsigned int binoffset, char *objfilename, char *name, float scale)
{
   unsigned char *data;
   FILE *fobj;
   long in_size;
   unsigned vcount;
   unsigned tcount;
   unsigned cur_tcount;
   unsigned terrain;
   unsigned v_per_t;
   unsigned processing;
   unsigned offset;
   unsigned i;
   unsigned vidx[3];
   short x, y, z;
   int ret_len = 0;

   fobj = fopen(objfilename, "w");
   if (fobj == NULL) {
      ERROR("Error opening \"%s\" for writing\n", objfilename);
      exit(EXIT_FAILURE);
   }

   in_size = read_file(binfilename, &data);
   if (in_size <= 0) {
      ERROR("Error reading input file \"%s\"\n", binfilename);
      exit(EXIT_FAILURE);
   }

   offset = binoffset;
   if (data[offset] != 0x00 || data[offset+1] != 0x40) {
      ERROR("Unknown collision data %s.%X: %08X\n", name, offset, read_u32_be(data));
      return 0;
   }

   fprintf(fobj, "# collision model generated from n64split v%s\n"
                 "# level %s %05X\n"
                 "\n"
                 "mtllib collision.mtl\n\n", N64SPLIT_VERSION, name, binoffset);
   vcount = read_u16_be(&data[offset+2]);
   INFO("Loading %u vertices\n", vcount);
   offset += 4;
   for (i = 0; i < vcount; i++) {
      x = read_s16_be(&data[offset + i*6]);
      y = read_s16_be(&data[offset + i*6+2]);
      z = read_s16_be(&data[offset + i*6+4]);
      fprintf(fobj, "v %f %f %f\n", (float)x/scale, (float)y/scale, (float)z/scale);
   }
   offset += vcount*6;
   tcount = 0;
   processing = 1;
   while (processing) {
      terrain = read_u16_be(&data[offset]);
      cur_tcount = read_u16_be(&data[offset+2]);
      // 0041 indicates the end, followed by 0042 or 0043
      if (terrain == 0x41 || terrain > 0xFF) {
         processing = 0;
         break;
      }
      switch (terrain) {
         case 0x0E:
         case 0x2C:
         case 0x24:
         case 0x25:
         case 0x27:
         case 0x2D:
            v_per_t = 4;
            break;
         default:
            v_per_t = 3;
            break;
      }
      fprintf(fobj, "\ng %s_%05X_%s\n", name, binoffset, terrain2str(terrain));
      fprintf(fobj, "usemtl %s\n", terrain2str(terrain));

      INFO("Loading %u triangles of terrain %02X\n", cur_tcount, terrain);
      offset += 4;
      for (i = 0; i < cur_tcount; i++) {
         vidx[0] = read_u16_be(&data[offset + i*v_per_t*2]);
         vidx[1] = read_u16_be(&data[offset + i*v_per_t*2+2]);
         vidx[2] = read_u16_be(&data[offset + i*v_per_t*2+4]);
         fprintf(fobj, "f %d %d %d\n", vidx[0]+1, vidx[1]+1, vidx[2]+1);
      }
      tcount += cur_tcount;
      offset += cur_tcount*v_per_t*2;
   }

   fclose(fobj);
   free(data);

   ret_len = offset - binoffset;
   return ret_len;
}