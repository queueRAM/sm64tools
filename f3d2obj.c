#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"

#define F3D2OBJ_VERSION "0.1"

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

typedef struct
{
   char *in_filename;
   char *out_filename;
   unsigned int offset;
   unsigned int length;
   unsigned translate[3];
   float scale;
   int v_idx_offset;
} arg_config;

static arg_config default_config =
{
   NULL,
   NULL,
   0,
   0,
   {0, 0, 0},
   1.0f,
   1
};


static int done = 0;

static void get_mode_string(unsigned char *data, char *description)
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

typedef struct
{
   int obj_idx;
   int x, y, z;
   int u, v;
   union
   {
      unsigned char rgb[3];
      signed char xyz[3];
   };
   unsigned char type; // rgb or xyz
   unsigned char a;
} vertex;

// segment data
#define MAX_SEGMENTS 0x10
static char *seg_files[MAX_SEGMENTS] = {0};
static unsigned char *seg_data[MAX_SEGMENTS] = {0};
static int seg_lengths[MAX_SEGMENTS] = {0};

// RSP vertex buffer
static vertex vertex_buffer[16];
static unsigned int material = 0;

// OBJ vertices
static vertex obj_vertices[1024];
static int obj_vert_count = 0;

int read_s16_be(unsigned char *buf)
{
   unsigned tmp = read_u16_be(buf);
   int ret;
   if (tmp > 0x7FFF) {
      ret = -((int)0x10000 - (int)tmp);
   } else {
      ret = (int)tmp;
   }
   return ret;
}

static void read_vertex(unsigned char *data, vertex *v, unsigned translate[])
{
   v->x = read_s16_be(&data[0x0]) + translate[0];
   v->y = read_s16_be(&data[0x2]) + translate[1];
   v->z = read_s16_be(&data[0x4]) + translate[2];
   // skip 6-7
   v->u = read_s16_be(&data[0x8]);
   v->v = read_s16_be(&data[0xA]);
   v->rgb[0] = data[0xC];
   v->rgb[1] = data[0xD];
   v->rgb[2] = data[0xE];
   v->a = data[0xF];
}

static void load_vertices(unsigned char *data, unsigned int offset, unsigned int index, unsigned int count, unsigned translate[])
{
   unsigned i;
   fprintf(stderr, "load: %X, %d\n", offset, count);
   for (i = 0; i < count; i++) {
      if (i + index < DIM(vertex_buffer)) {
         read_vertex(&data[offset + i*16], &vertex_buffer[i+index], translate);
         vertex_buffer[i+index].obj_idx = obj_vert_count;
         obj_vertices[obj_vert_count] = vertex_buffer[i+index];
         obj_vert_count++;
      } else {
         ERROR("%u + %u >= %lu\n", i, index, DIM(vertex_buffer));
      }
   }
}

static void print_f3d(FILE *fout, unsigned char *data, unsigned char *raw, arg_config *config)
{
   char description[64];
   char tmp[8];
   unsigned char bank;
   unsigned char offset;
   unsigned int address;
   unsigned int val;
   int i;
   // default description is raw bytes
   description[0] = '\0';
   for (i = 0; i < 8; i++) {
      sprintf(tmp, "%02X ", data[i]);
      strcat(description, tmp);
   }
   switch (data[0]) {
      case F3D_MOVEMEM:
         switch (data[1]) {
            case 0x86: sprintf(description, "light"); break;
            case 0x88: sprintf(description, "dark "); break;
         }
         bank = data[4];
         address = read_u24_be(&data[5]);
         if (bank == 0x07) {
            float r, g, b;
            r = (float)raw[address] / 255.0f;
            g = (float)raw[address+1] / 255.0f;
            b = (float)raw[address+2] / 255.0f;
            if (data[1] == 0x86) {
               fprintf(fout, "# newmtl M%06X\n", address);
               fprintf(fout, "# Ka %f %f %f\n", r, g, b);
               material = address;
            } else if (data[1] == 0x88) {
               fprintf(fout, "# Kd %f %f %f\n\n", r, g, b);
               fprintf(fout, "mtllib materials.mtl\n");
               fprintf(fout, "usemtl M%06X\n", material);
            }
         }
         break;
      case F3D_VTX:
      {
         unsigned int count = (data[1] >> 4) & 0xF;
         unsigned int index = (data[1]) & 0xF;
         //unsigned int length = read_u16_be(&data[2]);
         unsigned int segment_addr = read_u32_be(&data[4]);
         bank = data[4];
         offset = segment_addr & 0x00FFFFFF;
         if (seg_data[bank] == NULL) {
            ERROR("Tried to load %d verts from bank %02X %06X\n", count, bank, offset);
         } else {
            load_vertices(seg_data[bank], offset, index, count, config->translate);
            for (i = 0; i < count; i++) {
               fprintf(fout, "v %f %f %f\n",
                     ((float)vertex_buffer[i+index].x) * config->scale,
                     ((float)vertex_buffer[i+index].y) * config->scale,
                     ((float)vertex_buffer[i+index].z) * config->scale);
            }
            for (i = 0; i < count; i++) {
               fprintf(fout, "vt %f %f\n",
                     ((float)vertex_buffer[i+index].u) / 1024.0f,
                     ((float)vertex_buffer[i+index].v) / 1024.0f);
            }
            for (i = 0; i < count; i++) {
               fprintf(stderr, "vn %d %d %d\n", 
                     vertex_buffer[i+index].xyz[0],
                     vertex_buffer[i+index].xyz[1],
                     vertex_buffer[i+index].xyz[2]);
               fprintf(fout, "vn %f %f %f\n",
                     ((float)vertex_buffer[i+index].xyz[0]) / 127.0f,
                     ((float)vertex_buffer[i+index].xyz[1]) / 127.0f,
                     ((float)vertex_buffer[i+index].xyz[2]) / 127.0f);
            }
         }
         break;
      }
      case F3D_DL:
         bank = data[4];
         address = read_u24_be(&data[5]);
         fprintf(stderr, "%14s %02X %06X\n", "F3D_DL", bank, address);
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
         fprintf(fout, "%14s %3d %3d %3d %3d %3d %3d", "F3D_QUAD",
               vertex[0], vertex[1], vertex[2],
               vertex[3], vertex[4], vertex[5]);
         break;
      }
      case F3D_CLRGEOMODE:
         get_mode_string(data, description);
         fprintf(stderr, "%14s %s", "F3D_CLRGEOMODE\n", description);
         break;
      case F3D_SETGEOMODE:
         get_mode_string(data, description);
         fprintf(stderr, "%14s %s", "F3D_SETGEOMODE\n", description);
         break;
      case F3D_ENDDL:
         // fprintf(fout, "%14s %s", "F3D_ENDL", description);
         done = 1;
         break;
      case F3D_TEXTURE:
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
         fprintf(stderr, "%14s %s\n", "F3D_TEXTURE", description);
         break;
      case F3D_TRI1:
      {
         unsigned char vertex[3];
         int idx[3];
         vertex[0] = data[5] / 0x0A;
         vertex[1] = data[6] / 0x0A;
         vertex[2] = data[7] / 0x0A;
         idx[0] = vertex_buffer[vertex[0]].obj_idx;
         idx[1] = vertex_buffer[vertex[1]].obj_idx;
         idx[2] = vertex_buffer[vertex[2]].obj_idx;
         fprintf(fout, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
               idx[0], idx[0], idx[0],
               idx[1], idx[1], idx[1],
               idx[2], idx[2], idx[2]);
         break;
      }
      case G_SETTILESIZE:
      {
         unsigned short width, height;
         width  = (((data[5] << 8) | (data[6] & 0xF0)) >> 6) + 1;
         height = (((data[6] & 0x0F) << 8 | data[7]) >> 2) + 1;
         fprintf(stderr, "%14s %2d %2d", "G_SETTILESIZE\n", width, height);
         break;
      }
      case G_LOADBLOCK:
         val = read_u32_be(&data[4]);
         switch (val) {
            case 0x077FF100: sprintf(description, "RGBA 32x64 or 64x32"); break;
            case 0x073FF100: sprintf(description, "RGBA 32x32"); break;
         }
         fprintf(stderr, "%14s %s", "G_LOADBLOCK\n", description);
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
         for (i = 0; i < DIM(table); i++) {
            if (!memcmp(table[i].data, &data[1], 7)) {
               strcpy(description, table[i].description);
            }
         }
         fprintf(stderr, "%14s %s\n", "G_SETTILE", description);
         break;
      }
      case G_SETFOGCOLOR:
         fprintf(stderr, "%14s %3d, %3d, %3d, %3d\n", "G_SETFOGCOLOR", data[4], data[5], data[6], data[7]);
         break;
      case G_SETENVCOLOR:
         fprintf(stderr, "%14s %3d, %3d, %3d, %3d\n", "G_SETENVCOLOR", data[4], data[5], data[6], data[7]);
         break;
      case G_SETCOMBINE:
      {
         struct {unsigned char data[7]; char *description;} table[] = 
         {
            {{0x12, 0x7F, 0xFF, 0xFF, 0xFF, 0xF8, 0x38}, "solid RGBA"},
            {{0x12, 0x18, 0x24, 0xFF, 0x33, 0xFF, 0xFF}, "alpha RGBA"},
         };
         unsigned i;
         for (i = 0; i < DIM(table); i++) {
            if (!memcmp(table[i].data, &data[1], 7)) {
               strcpy(description, table[i].description);
            }
         }
         fprintf(stderr, "%14s %s\n", "G_SETCOMBINE", description);
         break;
      }
      case G_SETTIMG:
         bank = data[4];
         address = read_u24_be(&data[5]);
         fprintf(stderr, "%14s %02X %06X\n", "G_SETTIMG", bank, address);
         break;
      default:
         return;
   }
}

static void print_usage(void)
{
   ERROR("Usage: f3d2obj [-l LENGTH] [-o OFFSET] FILE\n"
         "\n"
         "f3d v" F3D2OBJ_VERSION ": N64 Fast3D display list decoder\n"
         "\n"
         "Optional arguments:\n"
         " -i NUM       starting vertex index offset (default: %d)\n"
         " -l LENGTH    length of data to decode in bytes (default: length of file)\n"
         " -o OFFSET    starting offset in FILE (default: %d)\n"
         " -x X         offset to add to all X values\n"
         " -y Y         offset to add to all Y values\n"
         " -z Z         offset to add to all Z values\n"
         " -s SCALE     scale all values by this factor\n"
         "\n"
         "File arguments:\n"
         " FILE        input file\n"
         " [OUTPUT]    output file (default: stdout)\n",
         default_config.v_idx_offset, default_config.offset);
   exit(1);
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], arg_config *config)
{
   int i;
   int seg;
   int file_count = 0;
   if (argc < 2) {
      print_usage();
      exit(1);
   }
   for (i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         // assign segment files
         if (argv[i][1] >= '0' && argv[i][1] <= '9') {
            seg = atoi(&argv[i][1]);
            if (++i >= argc) {
               print_usage();
            }
            seg_files[seg] = argv[i];
         } else {
            switch (argv[i][1]) {
               case 'i':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->v_idx_offset = strtoul(argv[i], NULL, 0);
                  break;
               case 'l':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->length = strtoul(argv[i], NULL, 0);
                  break;
               case 'o':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->offset = strtoul(argv[i], NULL, 0);
                  break;
               case 's':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->scale = strtof(argv[i], NULL);
                  break;
               case 'x':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->translate[0] = strtoul(argv[i], NULL, 0);
                  break;
               case 'y':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->translate[1] = strtoul(argv[i], NULL, 0);
                  break;
               case 'z':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->translate[2] = strtoul(argv[i], NULL, 0);
                  break;
               default:
                  print_usage();
                  break;
            }
         }
      } else {
         switch (file_count) {
            case 0:
               config->in_filename = argv[i];
               break;
            case 1:
               config->out_filename = argv[i];
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
   arg_config config;
   FILE *fout;
   unsigned char *data;
   long size;
   unsigned int i;

   // get configuration from arguments
   config = default_config;
   parse_arguments(argc, argv, &config);
   if (config.out_filename == NULL) {
      fout = stdout;
   } else {
      fout = fopen(config.out_filename, "w");
      if (fout == NULL) {
         perror("Error opening output file");
         return EXIT_FAILURE;
      }
   }
   
   // open segment files
   for (i = 0; i < DIM(seg_files); i++) {
      if (seg_files[i] != NULL) {
         size = read_file(seg_files[i], &seg_data[i]);
         if (size < 0) {
            perror("Error opening input file");
            return EXIT_FAILURE;
         }
         seg_lengths[i] = size;
      }
   }

   // operation
   size = read_file(config.in_filename, &data);
   if (size < 0) {
      perror("Error opening input file");
      return EXIT_FAILURE;
   }
   if (config.length == 0) {
      config.length = size - config.offset;
   }
   if (config.offset >= size) {
      ERROR("Error: offset greater than file size (%X > %X)\n",
            config.offset, (unsigned int)size);
      return EXIT_FAILURE;
   }
   if (config.offset + config.length > size) {
      ERROR("Warning: length goes beyond file size (%X > %X), truncating\n",
            config.offset + config.length, (unsigned int)size);
      config.length = size - config.offset;
   }

   for (i = config.offset; i < config.offset + config.length; i += 8) {
      print_f3d(fout, &data[i], data, &config);
      if (done) break;
   }

   free(data);
   if (fout != stdout) {
      fclose(fout);
   }

   return 0;
}
