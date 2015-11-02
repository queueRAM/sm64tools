#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils.h"

#define SM64COLLISION_VERSION "0.1"

typedef struct
{
   char *in_filename;
   char *out_filename;
   char *name;
   unsigned offset;
   unsigned scale;
   short x;
   short y;
   short z;
} arg_config;

typedef struct
{
   short x, y, z;
} vertex;

typedef struct
{
   unsigned vidx[3];
} triangle;

typedef struct
{
   vertex *verts;
   unsigned vcount;
   triangle *tris;
   unsigned tcount;
} collision;

// default configuration
static const arg_config default_config = 
{
   NULL,   // input filename
   NULL,   // output filename
   "collision", // model name
   0xF3A0, // offset for castle grounds
   4096,   // scale for OBJ export
   0,      // shift X
   0,      // shift Y
   0,      // shift Z
};

static void print_usage(void)
{
   ERROR("Usage: sm64collision [-o OFFSET] [-n NAME] [-s SCALE] [-x X] [-y Y] [-z Z] [-v] FILE [OUT_FILE]\n"
         "\n"
         "sm64collision v" SM64COLLISION_VERSION ": Super Mario 64 collision decoder\n"
         "\n"
         "Optional arguments:\n"
         " -o OFFSET    offset to pull collision data from in bytes (default: 0x%X)\n"
         " -n NAME      name specified in obj model (default: %s)\n"
         " -s SCALE     scale obj data by this amount (default: %d)\n"
         " -x X         amount to shift X values (default: %d)\n"
         " -y Y         amount to shift Y values (default: %d)\n"
         " -z Z         amount to shift Z values (default: %d)\n"
         " -v           verbose progress output\n"
         "\n"
         "File arguments:\n"
         " FILE        input binary file\n"
         " OUT_FILE    output OBJ file (default: no obj output)\n",
         default_config.offset, default_config.name, default_config.scale,
         default_config.x, default_config.y, default_config.z);
   exit(EXIT_FAILURE);
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], arg_config *args)
{
   int i;
   int file_count = 0;
   if (argc < 2) {
      print_usage();
   }
   for (i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case 'o':
               if (++i >= argc) {
                  print_usage();
               }
               args->offset = strtoul(argv[i], NULL, 0);
               break;
            case 'n':
               if (++i >= argc) {
                  print_usage();
               }
               args->name = argv[i];
               break;
            case 's':
               if (++i >= argc) {
                  print_usage();
               }
               args->scale = strtol(argv[i], NULL, 0);
               break;
            case 'x':
               if (++i >= argc) {
                  print_usage();
               }
               args->x = strtol(argv[i], NULL, 0);
               break;
            case 'y':
               if (++i >= argc) {
                  print_usage();
               }
               args->y = strtol(argv[i], NULL, 0);
               break;
            case 'z':
               if (++i >= argc) {
                  print_usage();
               }
               args->z = strtol(argv[i], NULL, 0);
               break;
            case 'v':
               g_verbosity = 1;
               break;
            default:
               print_usage();
               break;
         }
      } else {
         switch (file_count) {
            case 0:
               args->in_filename = argv[i];
               break;
            case 1:
               args->out_filename = argv[i];
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

void decode_collision(unsigned char *data, collision *col)
{
   unsigned vcount;
   unsigned tcount;
   unsigned cur_tcount;
   unsigned offset;
   unsigned terrain;
   unsigned v_per_t;
   unsigned processing;
   unsigned i;
   if (data[0] != 0x00 || data[1] != 0x40) {
      ERROR("Unknown data: %08X\n", read_u32_be(data));
      return;
   }
   vcount = read_u16_be(&data[2]);
   INFO("Loading %u vertices\n", vcount);
   col->verts = malloc(vcount * sizeof(*col->verts));
   // load vetices
   offset = 4;
   for (i = 0; i < vcount; i++) {
      col->verts[i].x = read_s16_be(&data[offset + i*6]);
      col->verts[i].y = read_s16_be(&data[offset + i*6+2]);
      col->verts[i].z = read_s16_be(&data[offset + i*6+4]);
   }
   offset += vcount*6;
   tcount = 0;
   col->tris = NULL;
   processing = 1;
   while (processing) {
      terrain = read_u16_be(&data[offset]);
      cur_tcount = read_u16_be(&data[offset+2]);
      if (terrain == 0x41 || terrain > 0xFF) {
         ERROR("terrain: %X, tcount: %X\n", terrain, cur_tcount);
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
      INFO("Loading %u triangles of terrain %X\n", cur_tcount, terrain);
      col->tris = realloc(col->tris, (tcount + cur_tcount) * sizeof(*col->tris));
      // load triangles
      offset += 4;
      for (i = 0; i < cur_tcount; i++) {
         col->tris[tcount+i].vidx[0] = read_u16_be(&data[offset + i*v_per_t*2]);
         col->tris[tcount+i].vidx[1] = read_u16_be(&data[offset + i*v_per_t*2+2]);
         col->tris[tcount+i].vidx[2] = read_u16_be(&data[offset + i*v_per_t*2+4]);
      }
      tcount += cur_tcount;
      offset += cur_tcount*v_per_t*2;
   }
   col->vcount = vcount;
   col->tcount = tcount;
}

static void generate_obj(char *filename, char *name, collision *col, unsigned scale)
{
   FILE *out;
   unsigned i;

   out = fopen(filename, "w");
   if (out == NULL) {
      ERROR("Error opening %s\n", filename);
      exit(EXIT_FAILURE);
   }

   fprintf(out, "g %s\n", name);

   for (i = 0; i < col->vcount; i++) {
      float x, y, z;
      x = (float)col->verts[i].x / (float)scale;
      y = (float)col->verts[i].y / (float)scale;
      z = (float)col->verts[i].z / (float)scale;
      fprintf(out, "v %f %f %f\n", x, y, z);
   }

   for (i = 0; i < col->tcount; i++) {
      fprintf(out, "f %d %d %d\n", col->tris[i].vidx[0]+1, col->tris[i].vidx[1]+1, col->tris[i].vidx[2]+1);
   }

   fclose(out);
}

int main(int argc, char *argv[])
{
   arg_config config;
   collision col;
   unsigned char *in_buf = NULL;
   long in_size;
   unsigned i;

   // get configuration from arguments
   config = default_config;
   parse_arguments(argc, argv, &config);

   // read input file into memory
   in_size = read_file(config.in_filename, &in_buf);
   if (in_size <= 0) {
      ERROR("Error reading input file \"%s\"\n", config.in_filename);
      exit(EXIT_FAILURE);
   }

   // decode collision vertices and triangles
   decode_collision(&in_buf[config.offset], &col);

   ERROR("Read: %d vertices, %d triangles\n", col.vcount, col.tcount);
   // output obj
   if (config.out_filename != NULL) {
      INFO("Generating OBJ file \"%s\"\n", config.out_filename);
      generate_obj(config.out_filename, config.name, &col, config.scale);
   }

   // if shifting values
   if (config.x || config.y || config.z) {
      INFO("Shifting vertices by: {%d %d %d}\n", config.x, config.y, config.z);
      for (i = 0; i < col.vcount; i++) {
         col.verts[i].x += config.x;
         col.verts[i].y += config.y;
         col.verts[i].z += config.z;
      }
      for (i = 0; i < col.vcount; i++) {
         write_u16_be(&in_buf[config.offset+4+i*6],   col.verts[i].x);
         write_u16_be(&in_buf[config.offset+4+i*6+2], col.verts[i].y);
         write_u16_be(&in_buf[config.offset+4+i*6+4], col.verts[i].z);
      }
      write_file(config.in_filename, in_buf, in_size);
   }

   // cleanup
   free(in_buf);

   return EXIT_SUCCESS;
}
