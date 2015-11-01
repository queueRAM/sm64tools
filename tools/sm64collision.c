#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils.h"

#define SM64COLLISION_VERSION "0.1"

typedef struct
{
   char *in_filename;
   unsigned offset;
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
   0xF3A0, // offset for castle grounds
   0,      // shift X
   0,      // shift Y
   0,      // shift Z
};

static void print_usage(void)
{
   ERROR("Usage: sm64collision [-o OFFSET] [-x X] [-y Y] [-z Z] [-v] FILE\n"
         "\n"
         "sm64collision v" SM64COLLISION_VERSION ": Super Mario 64 collision decoder\n"
         "\n"
         "Optional arguments:\n"
         " -o OFFSET    offset to pull collision data from in bytes (default: 0x%X)\n"
         " -x X         amount to shift X values (default: %d)\n"
         " -y Y         amount to shift Y values (default: %d)\n"
         " -z Z         amount to shift Z values (default: %d)\n"
         " -v           verbose progress output\n"
         "\n"
         "File arguments:\n"
         " FILE        input ROM file\n",
         default_config.offset, default_config.x, default_config.y, default_config.z);
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
   unsigned i;
   unsigned vcount;
   unsigned tcount;
   if (data[0] != 0x00 || data[1] != 0x40) {
      ERROR("Unknown data: %08X\n", read_u32_be(data));
      return;
   }
   vcount = read_u16_be(&data[2]);
   INFO("Loading %u vertices\n", vcount);
   col->verts = malloc(vcount * sizeof(*col->verts));
   // load vetices
   for (i = 0; i < vcount; i++) {
      col->verts[i].x = read_s16_be(&data[4+i*6]);
      col->verts[i].y = read_s16_be(&data[4+i*6+2]);
      col->verts[i].z = read_s16_be(&data[4+i*6+4]);
   }
   tcount = read_u16_be(&data[4+vcount*6+2]);
   INFO("Loading %u triangles\n", tcount);
   col->tris = malloc(tcount * sizeof(*col->tris));
   // load triangles
   for (i = 0; i < tcount; i++) {
      col->tris[i].vidx[0] = read_u16_be(&data[4+vcount*6+i*6]);
      col->tris[i].vidx[1] = read_u16_be(&data[4+vcount*6+i*6+2]);
      col->tris[i].vidx[2] = read_u16_be(&data[4+vcount*6+i*6+4]);
      INFO("[%3d] = {%d, %d, %d} {%d %d %d} {%d %d %d}\n", i, 
      col->verts[col->tris[i].vidx[0]].x,
      col->verts[col->tris[i].vidx[0]].y,
      col->verts[col->tris[i].vidx[0]].z,
      col->verts[col->tris[i].vidx[1]].x,
      col->verts[col->tris[i].vidx[1]].y,
      col->verts[col->tris[i].vidx[1]].z,
      col->verts[col->tris[i].vidx[2]].x,
      col->verts[col->tris[i].vidx[2]].y,
      col->verts[col->tris[i].vidx[2]].z
      );
   }
   col->vcount = vcount;
   col->tcount = tcount;
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

   // cleanup
   free(in_buf);

   return EXIT_SUCCESS;
}
