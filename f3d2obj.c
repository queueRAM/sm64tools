#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libblast.h"
#include "n64graphics.h"
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
   unsigned int offsets[0x100];
   unsigned offset_count;
   char *blast_corps_rom;
   char *out_dir;
   unsigned translate[3];
   float scale;
   int v_idx_offset;
} arg_config;

typedef enum
{
   IMG_FORMAT_RGBA,
   IMG_FORMAT_YUV,
   IMG_FORMAT_CI,
   IMG_FORMAT_IA,
   IMG_FORMAT_I,
} img_format;

static arg_config default_config =
{
   {0},
   0,
   NULL,
   NULL,
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
static unsigned int seg_lengths[MAX_SEGMENTS] = {0};

// RSP vertex buffer
static vertex vertex_buffer[16];
static unsigned int material = 0;

// display list stack
static unsigned int dl_stack[0x100];
static int stack_idx = 0;

// OBJ vertices
static vertex *obj_vertices = NULL;
static int obj_vert_allocated = 0;
static int obj_vert_count = 0;

// textures needed
typedef struct
{
   unsigned int address;
   int width;
   int height;
   img_format format;
   int depth;
} texture;
static texture *textures = NULL;
static int texture_allocated = 0;
static int texture_count = 0;
// current texture info
static texture tile = {
   0xFFFFFFFF,
   -1,
   -1,
   IMG_FORMAT_RGBA,
   -1
};

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
   for (i = 0; i < count; i++) {
      if (i + index < DIM(vertex_buffer)) {
         read_vertex(&data[offset + i*16], &vertex_buffer[i+index], translate);
         vertex_buffer[i+index].obj_idx = obj_vert_count;
         if (obj_vert_count + 1 > obj_vert_allocated) {
            obj_vert_allocated *= 2;
            INFO("realloc obj_vertices to %d\n", obj_vert_allocated);
            obj_vertices = realloc(obj_vertices, obj_vert_allocated * sizeof(*obj_vertices));
         }
         obj_vertices[obj_vert_count] = vertex_buffer[i+index];
         obj_vert_count++;
      } else {
         ERROR("%u + %u >= " SIZE_T_FORMAT "\n", i, index, DIM(vertex_buffer));
      }
   }
}

static void add_texture(texture const * const tex)
{
   int i;
   for (i = 0; i < texture_count; i++) {
      if (textures[i].address == tex->address) return;
   }
   if (texture_count >= texture_allocated) {
      texture_allocated *= 2;
      INFO("realloc textures to %d\n", texture_allocated);
      textures = realloc(textures, texture_allocated * sizeof(*textures));
   }
   textures[texture_count] = *tex;
   texture_count++;
}

static void generate_material_file(arg_config *config, char *mtl_filename, char *texture_dir)
{
   char texture_path[FILENAME_MAX];
   char texture_filename[32];
   FILE *fmtl;
   rgba *rgba_img;
   ia   *ia_img;
   unsigned char *img_raw = NULL;
   unsigned char *rom = NULL;
   long rom_size;
   unsigned int segment;
   unsigned int offset;
   int i;
   if (config->blast_corps_rom != NULL) {
      rom_size = read_file(config->blast_corps_rom, &rom);
      if (rom_size < 0) {
         perror("Error opening ROM file");
         exit(EXIT_FAILURE);
      }
      img_raw = malloc(4*256*256);
   }
   fmtl = fopen(mtl_filename, "w");
   if (fmtl) {
      for (i = 0; i < texture_count; i++) {
         texture *t = &textures[i];
         rgba_img = NULL;
         ia_img = NULL;
         sprintf(texture_filename, "%08X.png", t->address);
         sprintf(texture_path, "%s/%s", texture_dir, texture_filename);
         fprintf(fmtl, "newmtl M%08X\n", t->address);
         // TODO: are these good values?
         fprintf(fmtl,
            "Ka 1.0 1.0 1.0\n" // ambiant color
            "Kd 1.0 1.0 1.0\n" // diffuse color
            "Ks 0.4 0.4 0.4\n" // specular color
            "Ns 0\n"           // specular exponent
            "d 1\n"            // dissolved
            "Tr 1\n");         // inverted
         fprintf(fmtl, "map_Kd textures/%s\n\n", texture_filename);
         if (config->blast_corps_rom == NULL) {
            segment = (t->address >> 24) & 0xFF;
            offset = t->address & 0xFFFFFF;
            if (seg_data[segment] == NULL || seg_lengths[segment] < offset) {
               ERROR("Error reading texture seg address 0x%08X\n", t->address);
               exit(EXIT_FAILURE);
            }
            img_raw = &seg_data[segment][offset];
            INFO("Decoding texture %08X %dx%d\n", t->address, t->width, t->height);
            switch (t->format) {
               case IMG_FORMAT_RGBA:
                  rgba_img = raw2rgba(img_raw, t->width, t->height, t->depth);
                  break;
               case IMG_FORMAT_IA:
                  ia_img = raw2ia(img_raw, t->width, t->height, t->depth);
                  break;
               default:
                  ERROR("Need format %d depth %d\n", t->format, t->depth);
                  break;
            }
         } else {
#define TEXTURE_LUT 0x4CE0
            unsigned int lut_addr = TEXTURE_LUT + 8 * t->address;
            unsigned int rom_addr = read_u32_be(&rom[lut_addr]) + TEXTURE_LUT;
            int length = read_u16_be(&rom[lut_addr + 4]);
            int text_type = read_u16_be(&rom[lut_addr + 6]);
            int retval = 0;
            INFO("Decoding texture %06X->%06X (%d x %d) type %d\n",
                  t->address, rom_addr, t->width, t->height, text_type);
            switch (text_type) {
               case 0: retval = decode_block0(&rom[rom_addr], length, img_raw); break;
               case 1: retval = decode_block1(&rom[rom_addr], length, img_raw); break;
               case 2: retval = decode_block2(&rom[rom_addr], length, img_raw); break;
               case 3: retval = decode_block3(&rom[rom_addr], length, img_raw); break;
               case 6: retval = decode_block6(&rom[rom_addr], length, img_raw); break;
               default:
                  ERROR("Blast Corps texture %d not supported for %X->%X\n",
                        text_type, t->address, rom_addr);
                  exit(EXIT_FAILURE);
                  break;
            }
            if (retval > 0) {
               switch (text_type) {
                  case 0: // IA8
                     ia_img = raw2ia(img_raw, t->width, t->height, 8);
                     break;
                  case 1: // RGBA16
                     rgba_img = raw2rgba(img_raw, t->width, t->height, 16);
                     break;
                  case 2: // RGBA32
                     rgba_img = raw2rgba(img_raw, t->width, t->height, 32);
                     break;
                  case 3: // IA8
                     ia_img = raw2ia(img_raw, t->width, t->height, 8);
                     break;
                  case 6: // IA8
                     ia_img = raw2ia(img_raw, t->width, t->height, 8);
                     break;
                  default:
                     ERROR("Blast Corps texture %d not supported for %X->%X\n",
                           text_type, t->address, rom_addr);
                     //exit(EXIT_FAILURE);
                     break;
               }
            }
         }
         if (rgba_img != NULL) {
            int ret = rgba2png(texture_path, rgba_img, t->width, t->height);
            if (ret != 1) {
               ERROR("Error writing to %s: %d\n", texture_filename, ret);
            }
         }
         if (ia_img != NULL) {
            int ret = ia2png(texture_path, ia_img, t->width, t->height);
            if (ret != 1) {
               ERROR("Error writing to %s: %d\n", texture_filename, ret);
            }
         }
      }
   }
   if (rom != NULL) {
      free(rom);
      free(img_raw);
   }
}

static int print_f3d(FILE *fout, unsigned int *dl_addr, arg_config *config)
{
   char description[64];
   char tmp[8];
   unsigned char *data;
   unsigned int seg_address;
   unsigned int seg_offset;
   unsigned int bank;
   int done = 0;
   unsigned int i;
   unsigned int dl_seg;
   dl_seg = (*dl_addr >> 24) & 0xFF;
   seg_offset = *dl_addr & 0xFFFFFF;
   if (seg_data[dl_seg] == NULL || seg_lengths[dl_seg] < seg_offset) {
      ERROR("Error reading seg address 0x%08X\n", *dl_addr);
      return 1;
   }
   data = &seg_data[dl_seg][seg_offset];
   // default description is raw bytes
   description[0] = '\0';
   for (i = 0; i < 8; i++) {
      sprintf(tmp, "%02X ", data[i]);
      strcat(description, tmp);
   }
   INFO("%02X%06X: ", dl_seg, seg_offset);
   switch (data[0]) {
      case F3D_MOVEMEM:
         switch (data[1]) {
            case 0x86: sprintf(description, "light"); break;
            case 0x88: sprintf(description, "dark "); break;
         }
         seg_address = read_u32_be(&data[4]);
         bank = data[4];
         seg_offset = seg_address & 0x00FFFFFF;
         INFO("%14s %s %08X\n", "F3D_MOVEMEM", description, seg_address);
         if (seg_data[bank] == NULL) {
            ERROR("Tried to F3D_MOVEMEM from bank %02X %06X\n", bank, seg_offset);
            fprintf(fout, "# F3D_MOVEMEM %02X %02X%02X %02X %06X\n", data[1], data[2], data[3], bank, seg_offset);
         } else {
            float r, g, b;
            r = (float)seg_data[bank][seg_offset] / 255.0f;
            g = (float)seg_data[bank][seg_offset+1] / 255.0f;
            b = (float)seg_data[bank][seg_offset+2] / 255.0f;
            if (data[1] == 0x86) {
               fprintf(fout, "# newmtl M%08X\n", seg_address);
               fprintf(fout, "# Ka %f %f %f\n", r, g, b);
               material = seg_address;
            } else if (data[1] == 0x88) {
               fprintf(fout, "# Kd %f %f %f\n\n", r, g, b);
               fprintf(fout, "# mtllib materials.mtl\n");
               fprintf(fout, "# usemtl M%08X\n", material);
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
         INFO("%14s %u %u %08X (%02X %06X)\n", "F3D_VTX", count, index, seg_address, bank, seg_offset);
         if (seg_data[bank] == NULL) {
            ERROR("Tried to load %d verts from bank %02X %06X\n", count, bank, seg_offset);
         } else {
            float uScale = 32.0f * tile.width;
            float vScale = 32.0f * tile.height;
            load_vertices(seg_data[bank], seg_offset, index, count, config->translate);
            for (i = 0; i < count; i++) {
               fprintf(fout, "v %f %f %f\n",
                     ((float)vertex_buffer[i+index].x) * config->scale,
                     ((float)vertex_buffer[i+index].y) * config->scale,
                     ((float)vertex_buffer[i+index].z) * config->scale);
            }
            for (i = 0; i < count; i++) {
               // invert vertical direction so all textures look upright
               fprintf(fout, "vt %f %f\n",
                     ((float)vertex_buffer[i+index].u) / uScale,
                     -((float)vertex_buffer[i+index].v) / vScale);
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
         seg_address = read_u32_be(&data[4]);
         INFO("%14s %08X\n", "F3D_DL", seg_address);
         // push current address on stack and set new address
         dl_stack[stack_idx] = *dl_addr;
         stack_idx++;
         *dl_addr = seg_address - 8; // subtract 8 since for loop will increment by 8
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
         INFO("%14s %3d %3d %3d %3d %3d %3d\n", "F3D_QUAD",
               vertex[0], vertex[1], vertex[2],
               vertex[3], vertex[4], vertex[5]);
         fprintf(fout, "# %14s %3d %3d %3d %3d %3d %3d", "F3D_QUAD",
               vertex[0], vertex[1], vertex[2],
               vertex[3], vertex[4], vertex[5]);
         break;
      }
      case F3D_CLRGEOMODE:
         get_mode_string(data, description);
         INFO("%14s %s\n", "F3D_CLRGEOMODE", description);
         break;
      case F3D_SETGEOMODE:
         get_mode_string(data, description);
         INFO("%14s %s\n", "F3D_SETGEOMODE", description);
         break;
      case F3D_ENDDL:
         INFO("%14s %s\n", "F3D_ENDL", description);
         // pop stack and or set done
         if (stack_idx == 0) {
            done = 1;
         } else {
            stack_idx--;
            *dl_addr = dl_stack[stack_idx];
         }
         break;
      case F3D_TEXTURE:
      {
         unsigned int val;
         // reset texture
         tile.address = 0xFFFFFFFF;
         tile.width = -1;
         tile.height = -1;
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
         INFO("%14s %s\n", "F3D_TEXTURE", description);
         break;
      }
      case F3D_TRI1:
      {
         unsigned char vertex[3];
         int idx[3];
         vertex[0] = data[5] / 0x0A;
         vertex[1] = data[6] / 0x0A;
         vertex[2] = data[7] / 0x0A;
         INFO("%14s %3d %3d %3d\n", "F3D_TRI1", vertex[0], vertex[1], vertex[2]);
         idx[0] = vertex_buffer[vertex[0]].obj_idx + config->v_idx_offset;
         idx[1] = vertex_buffer[vertex[1]].obj_idx + config->v_idx_offset;
         idx[2] = vertex_buffer[vertex[2]].obj_idx + config->v_idx_offset;
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
         INFO("%14s %2d %2d\n", "G_SETTILESIZE", width, height);
         tile.width = width;
         tile.height = height;
         if (tile.address != 0xFFFFFFFF) {
            add_texture(&tile);
         }
         break;
      }
      case G_LOADBLOCK:
      {
         unsigned w0 = read_u32_be(data);
         unsigned w1 = read_u32_be(&data[4]);
         unsigned uls = (w0 >> 12) & 0x3FF;
         unsigned ult = w0 & 0x3FF;
         unsigned lrs = (w1 >> 12) & 0x3FF;
         unsigned dxt = w1 & 0x3FF;
         INFO("%14s %03X %03X %03X %u\n", "G_LOADBLOCK", uls, ult, lrs, dxt);
         break;
      }
      case G_SETTILE:
      {
         const char * fmt_table[] =
         {
            "RGBA", "YUV", "CI", "IA", "I"
         };
         unsigned format = (data[1] >> 5) & 0x7; // bits 21-23
         unsigned size = (data[1] >> 3) & 0x3; // bits 19-20
         tile.format = (img_format) format;
         switch (size) {
            case 0: tile.depth = 4; break;
            case 1: tile.depth = 8; break;
            case 2: tile.depth = 16; break;
            case 3: tile.depth = 32; break;
            default: ERROR("Unknown depth: %d\n", size);
         }
         if (format < DIM(fmt_table)) {
            sprintf(description, "%s %d", fmt_table[format], tile.depth);
         }
         INFO("%14s %s\n", "G_SETTILE", description);
         break;
      }
      case G_SETFOGCOLOR:
         INFO("%14s %3d, %3d, %3d, %3d\n", "G_SETFOGCOLOR", data[4], data[5], data[6], data[7]);
         break;
      case G_SETENVCOLOR:
         INFO("%14s %3d, %3d, %3d, %3d\n", "G_SETENVCOLOR", data[4], data[5], data[6], data[7]);
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
         INFO("%14s %s\n", "G_SETCOMBINE", description);
         break;
      }
      case G_SETTIMG:
         seg_address = read_u32_be(&data[4]);
         bank = data[4];
         seg_offset = seg_address & 0x00FFFFFF;
         INFO("%14s %08X\n", "G_SETTIMG", seg_address);
         fprintf(fout, "\ng s%08X_%08X\n", *dl_addr, seg_address);
         fprintf(fout, "usemtl M%08X\n", seg_address);
         tile.address = seg_address;
         if (tile.width != -1) {
            add_texture(&tile);
         }
         break;
      default:
         INFO("%14s %s\n", "Unknown", description);
         break;
   }
   return done;
}

static void print_usage(void)
{
   ERROR("Usage: f3d2obj [-0/-F FILE] [-d DIR] [-i NUM] [-s SCALE] [-v] [-x/y/z OFF] SEG_ADDR...\n"
         "\n"
         "f3d2obj v" F3D2OBJ_VERSION ": Fast3D display list to Wavefront .obj converter\n"
         "\n"
         "Optional arguments:\n"
         " -0/-F FILE   load FILE into segment specified by flag (0 through F)\n"
         " -b ROM       use Blast Corps mode specifying ROM to load textures\n"
         " -d DIR       directory to output (default: SEGMENT_ADDR.model)\n"
         " -i NUM       starting vertex index offset (default: %d)\n"
         " -s SCALE     scale all values by this factor (float)\n"
         " -v           verbose output\n"
         " -x X         offset to add to all X values before scaling\n"
         " -y Y         offset to add to all Y values before scaling\n"
         " -z Z         offset to add to all Z values before scaling\n"
         "\n"
         "Input arguments:\n"
         " SEG_ADDR     segment addresses to start decoding from\n",
         default_config.v_idx_offset);
   exit(1);
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], arg_config *config)
{
   int i;
   int seg;
   if (argc < 2) {
      print_usage();
      exit(1);
   }
   for (i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         // assign segment files
         if (argv[i][1] >= '0' && argv[i][1] <= '9') {
            seg = argv[i][1] - '0';
            if (++i >= argc) {
               print_usage();
            }
            seg_files[seg] = argv[i];
         } else if (argv[i][1] >= 'A' && argv[i][1] <= 'F') {
            seg = argv[i][1] - 'A' + 0xA;
            if (++i >= argc) {
               print_usage();
            }
            seg_files[seg] = argv[i];
         } else {
            switch (argv[i][1]) {
               case 'b':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->blast_corps_rom = argv[i];
                  break;
               case 'd':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->out_dir = argv[i];
                  break;
               case 'i':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->v_idx_offset = strtoul(argv[i], NULL, 0);
                  break;
               case 's':
                  if (++i >= argc) {
                     print_usage();
                  }
                  config->scale = strtof(argv[i], NULL);
                  break;
               case 'v':
                  g_verbosity = 1;
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
         config->offsets[config->offset_count] = strtoul(argv[i], NULL, 0);
         config->offset_count++;
      }
   }
   if (config->offset_count < 1) {
      print_usage();
   }
}

int main(int argc, char *argv[])
{
   char out_dir[FILENAME_MAX];
   char texture_dir[FILENAME_MAX];
   char out_filename[FILENAME_MAX];
   char mtl_filename[FILENAME_MAX];
   arg_config config;
   FILE *fout;
   long size;
   int done;
   unsigned s;
   unsigned int seg_addr;

   // get configuration from arguments
   config = default_config;
   parse_arguments(argc, argv, &config);

   // make basedir
   if (config.out_dir == NULL) {
      sprintf(out_dir, "%08X.out", config.offsets[0]);
   } else {
      strcpy(out_dir, config.out_dir);
   }
   make_dir(out_dir);

   // make texture dir
   sprintf(texture_dir, "%s/textures", out_dir);
   make_dir(texture_dir);

   sprintf(out_filename, "%s/model.obj", out_dir);
   fout = fopen(out_filename, "w");
   if (fout == NULL) {
      perror("Error opening output file");
      return EXIT_FAILURE;
   }
   sprintf(mtl_filename, "%s/material.mtl", out_dir);

   // open segment files
   for (s = 0; s < DIM(seg_files); s++) {
      if (seg_files[s] != NULL) {
         size = read_file(seg_files[s], &seg_data[s]);
         if (size < 0) {
            perror("Error opening input file");
            return EXIT_FAILURE;
         }
         seg_lengths[s] = size;
      }
   }

   // init data
   obj_vert_allocated = 1024;
   obj_vertices = malloc(obj_vert_allocated * sizeof(*obj_vertices));
   texture_allocated = 256;
   textures = malloc(texture_allocated * sizeof(*textures));

   // generate .obj file
   fprintf(fout, "mtllib material.mtl\n\n");
   for (s = 0; s < config.offset_count; s++)
   {
      done = 0;
      for (seg_addr = config.offsets[s]; !done; seg_addr += 8) {
         done = print_f3d(fout, &seg_addr, &config);
      }
   }

   // generate .mtl file
   generate_material_file(&config, mtl_filename, texture_dir);

   fclose(fout);

   return 0;
}
