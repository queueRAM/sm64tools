#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"

#define F3D2OBJ_VERSION "0.1"

#define F3D_MOVEMEM    0x03
#define F3D_VTX        0x04
#define F3D_DL         0x06
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
   char *mtl_filename;
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
   NULL,
   0,
   0,
   {0, 0, 0},
   1.0f,
   1
};

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

// textures needed
static unsigned int textures[4096] = {0};
static int texture_count = 0;

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
   INFO("load: %X, %d\n", offset, count);
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

static void add_texture(unsigned int offset)
{
   int i;
   for (i = 0; i < texture_count; i++) {
      if (textures[i] == offset) return;
   }
   textures[texture_count] = offset;
   texture_count++;
}

static void generate_material_file(char *fname)
{
   unsigned char *rom;
   FILE *fmtl;
   long rom_size;
   int i;
   fmtl = fopen(fname, "w");
   // TODO: this is specific to Blast Corps
   rom_size = read_file("bc.u.z64", &rom);
   if (rom_size < 0) {
      perror("Error opening input file");
      exit(EXIT_FAILURE);
   }
   if (fmtl) {
      for (i = 0; i < texture_count; i++) {
         // TODO: this is specific to Blast Corps
         unsigned int rom_addr = read_u32_be(&rom[0x4CE0+8*textures[i]]) + 0x4CE0;
         fprintf(fmtl, "newmtl M%06X\n", textures[i]);
         // TODO: are these good values?
         fprintf(fmtl,
         "Ka 1.0 1.0 1.0\n" // ambiant color
         "Kd 1.0 1.0 1.0\n" // diffuse color
         "Ks 0.4 0.4 0.4\n" // specular color
         "Ns 0\n"           // specular exponent
         "d 1\n"            // dissolved
         "Tr 1\n");         // inverted
         // TODO: this is specific to Blast Corps
         fprintf(fmtl, "map_Kd blast_corps.u.split/textures/%06X.0x00000.png\n\n", rom_addr);
      }
   }
   free(rom);
}

static int print_f3d(FILE *fout, unsigned char *data, unsigned char *raw, arg_config *config)
{
   char description[64];
   char tmp[8];
   unsigned int seg_address;
   unsigned int seg_offset;
   unsigned int bank;
   int done = 0;
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
         seg_address = read_u32_be(&data[4]);
         bank = data[4];
         seg_offset = seg_address & 0x00FFFFFF;
         INFO("%14s %s %08X", "F3D_MOVEMEM", description, seg_address);
         if (seg_data[bank] == NULL) {
            ERROR("Tried to F3D_MOVEMEM from bank %02X %06X\n", bank, seg_offset);
            fprintf(fout, "# F3D_MOVEMEM %02X %02X%02X %02X %06X\n", data[1], data[2], data[3], bank, seg_offset);
         } else {
            float r, g, b;
            r = (float)seg_data[bank][seg_offset] / 255.0f;
            g = (float)seg_data[bank][seg_offset+1] / 255.0f;
            b = (float)seg_data[bank][seg_offset+2] / 255.0f;
            if (data[1] == 0x86) {
               fprintf(fout, "# newmtl M%06X\n", seg_offset);
               fprintf(fout, "# Ka %f %f %f\n", r, g, b);
               material = seg_offset;
            } else if (data[1] == 0x88) {
               fprintf(fout, "# Kd %f %f %f\n\n", r, g, b);
               fprintf(fout, "mtllib materials.mtl\n");
               fprintf(fout, "usemtl M%06X\n", material);
            }
         }
         break;
      case F3D_VTX:
      {
         unsigned int count = ((data[1] >> 4) & 0xF) + 1;
         unsigned int index = (data[1]) & 0xF;
         //unsigned int length = read_u16_be(&data[2]);
         seg_address = read_u32_be(&data[4]);
         bank = data[4];
         seg_offset = seg_address & 0x00FFFFFF;
         INFO("%14s %u %u %08X (%02X %06X)", "F3D_VTX", count, index, seg_address, bank, seg_offset);
         fprintf(fout, "# F3D_VTX %u %u %08X (%02X %06X)\n", count, index, seg_address, bank, seg_offset);
         if (seg_data[bank] == NULL) {
            ERROR("Tried to load %d verts from bank %02X %06X\n", count, bank, seg_offset);
         } else {
            load_vertices(seg_data[bank], seg_offset, index, count, config->translate);
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
               fprintf(fout, "vn %f %f %f\n",
                     ((float)vertex_buffer[i+index].xyz[0]) / 127.0f,
                     ((float)vertex_buffer[i+index].xyz[1]) / 127.0f,
                     ((float)vertex_buffer[i+index].xyz[2]) / 127.0f);
            }
         }
         break;
      }
      case F3D_DL:
         // TODO: implement as jump
         seg_address = read_u32_be(&data[4]);
         INFO("%14s %08X", "F3D_DL", seg_address);
         break;
      case F3D_QUAD:
      {
         unsigned char vertex[6];
         vertex[0] = data[1] / 0x0A;
         vertex[1] = data[2] / 0x0A;
         vertex[2] = data[3] / 0x0A;
         // data[4] unused
         vertex[3] = data[5] / 0x0A;
         vertex[4] = data[6] / 0x0A;
         vertex[5] = data[7] / 0x0A;
         INFO("%14s %3d %3d %3d %3d %3d %3d", "F3D_QUAD",
               vertex[0], vertex[1], vertex[2],
               vertex[3], vertex[4], vertex[5]);
         fprintf(fout, "# %14s %3d %3d %3d %3d %3d %3d", "F3D_QUAD",
               vertex[0], vertex[1], vertex[2],
               vertex[3], vertex[4], vertex[5]);
         break;
      }
      case F3D_CLRGEOMODE:
         get_mode_string(data, description);
         INFO("%14s %s", "F3D_CLRGEOMODE", description);
         break;
      case F3D_SETGEOMODE:
         get_mode_string(data, description);
         INFO("%14s %s", "F3D_SETGEOMODE", description);
         break;
      case F3D_ENDDL:
         INFO("%14s %s", "F3D_ENDL", description);
         done = 1;
         break;
      case F3D_TEXTURE:
      {
         unsigned int val;
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
         INFO("%14s %s", "F3D_TEXTURE", description);
         break;
      }
      case F3D_TRI1:
      {
         unsigned char vertex[3];
         int idx[3];
         vertex[0] = data[5] / 0x0A;
         vertex[1] = data[6] / 0x0A;
         vertex[2] = data[7] / 0x0A;
         INFO("%14s %3d %3d %3d", "F3D_TRI1", vertex[0], vertex[1], vertex[2]);
         idx[0] = vertex_buffer[vertex[0]].obj_idx+1;
         idx[1] = vertex_buffer[vertex[1]].obj_idx+1;
         idx[2] = vertex_buffer[vertex[2]].obj_idx+1;
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
         INFO("%14s %2d %2d", "G_SETTILESIZE", width, height);
         break;
      }
      case G_LOADBLOCK:
      {
         unsigned int val = read_u32_be(&data[4]);
         switch (val) {
            case 0x077FF100: sprintf(description, "RGBA 32x64 or 64x32"); break;
            case 0x073FF100: sprintf(description, "RGBA 32x32"); break;
         }
         INFO("%14s %s", "G_LOADBLOCK", description);
         break;
      }
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
         INFO("%14s %s", "G_SETTILE", description);
         break;
      }
      case G_SETFOGCOLOR:
         INFO("%14s %3d, %3d, %3d, %3d", "G_SETFOGCOLOR", data[4], data[5], data[6], data[7]);
         break;
      case G_SETENVCOLOR:
         INFO("%14s %3d, %3d, %3d, %3d", "G_SETENVCOLOR", data[4], data[5], data[6], data[7]);
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
         INFO("%14s %s", "G_SETCOMBINE", description);
         break;
      }
      case G_SETTIMG:
         seg_address = read_u32_be(&data[4]);
         bank = data[4];
         seg_offset = seg_address & 0x00FFFFFF;
         INFO("%14s %02X %06X\n", "G_SETTIMG", bank, seg_offset);
         fprintf(fout, "\n# %s %02X %06X\n", "G_SETTIMG", bank, seg_offset);
         fprintf(fout, "usemtl M%06X\n", seg_offset);
         add_texture(seg_offset);
         break;
      default:
         INFO("%14s %s", "Unknown", description);
         break;
   }
   return done;
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
         " -m MTLFILE   material .mtl filename (default: FILE_OFFSET.mtl)\n"
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
               case 'm':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->mtl_filename = argv[i];
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
   char mtl_filename[FILENAME_MAX];
   arg_config config;
   FILE *fout;
   unsigned char *data;
   long size;
   int done;
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
   if (config.mtl_filename == NULL) {
      sprintf(mtl_filename, "%s_%04X.mtl", config.in_filename, config.offset);
   } else {
      strcpy(mtl_filename, config.mtl_filename);
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

   // read in file and error checking
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

   // generate .obj file
   fprintf(fout, "mtllib %s\n\n", mtl_filename);
   done = 0;
   for (i = config.offset; i < config.offset + config.length && !done; i += 8) {
      done = print_f3d(fout, &data[i], data, &config);
   }

   // generate .mtl file
   generate_material_file(mtl_filename);

   free(data);
   if (fout != stdout) {
      fclose(fout);
   }

   return 0;
}
