#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libmio0.h"
#include "libsm64.h"
#include "utils.h"

#define SM64COMPRESS_VERSION "0.2a"

#define MAX_REFS 64

typedef struct
{
   unsigned int level;  // original level script offset where referenced
   unsigned int offset; // offset within level script where referenced
   unsigned char type;  // command type: 0x1A, 0x18, or 0xFF, 0xFE, 0xFD for ASM
} block_ref;

typedef struct
{
   unsigned int old;          // starting offset in original ROM
   unsigned int old_end;      // ending offset in original ROM
   unsigned int new;          // starting offset in new ROM
   unsigned int new_end;      // ending offset in new ROM
   block_ref refs[MAX_REFS];  // references to this block
   int          ref_count;    // number of references
   char         compressible; // if block is not currenlty, but potentially compressible
   enum {
      BLOCK_LEVEL,
      BLOCK_MIO0,
      BLOCK_RAW
   } type;
} block;

typedef struct
{
   char *in_filename;
   char *out_filename;
   unsigned int alignment;
   char compress;
   char dump;
   char fix_f3d;
   char fix_geo;
} compress_config;

// default configuration
static const compress_config default_config = 
{
   NULL, // input filename
   NULL, // output filename
   16,   // block alignment
   0,    // compress all MIO0 blocks
   0,    // dump
   0,    // f3d
   0,    // geo
};

static void print_usage(void)
{
   ERROR("Usage: sm64compress [-a ALIGNMENT] [-c] [-d] [-f] [-g] [-v] FILE [OUT_FILE]\n"
         "\n"
         "sm64compress v" SM64COMPRESS_VERSION ": Super Mario 64 ROM compressor and fixer\n"
         "\n"
         "Optional arguments:\n"
         " -a ALIGNMENT byte boundary to align blocks (default: %d)\n"
         " -c           compress all 0x17 blocks using MIO0 (experimental)\n"
         " -d           dump blocks to 'dump' directory\n"
         " -f           fix F3D combine blending parameters\n"
         " -g           fix geo layout display list layers\n"
         " -v           verbose progress output\n"
         "\n"
         "File arguments:\n"
         " FILE         input ROM file\n"
         " OUT_FILE     output compressed ROM file (default: replaces input extension with .out.z64)\n",
         default_config.alignment);
   exit(1);
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], compress_config *config)
{
   int i;
   int file_count = 0;
   if (argc < 2) {
      print_usage();
      exit(1);
   }
   for (i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case 'a':
               if (++i >= argc) {
                  print_usage();
               }
               config->alignment = strtoul(argv[i], NULL, 0);
               if (!is_power2(config->alignment)) {
                  ERROR("Error: Alignment must be power of 2\n");
                  exit(2);
               }
               break;
            case 'c':
               config->compress = 1;
               break;
            case 'd':
               config->dump = 1;
               break;
            case 'f':
               config->fix_f3d = 1;
               break;
            case 'g':
               config->fix_geo = 1;
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

int compare_block(const void *a , const void *b)
{
    const block *blk_a = (const block*)a;
    const block *blk_b = (const block*)b;

    if(blk_a->old < blk_b->old) {
       return -1;
    } else if(blk_a->old > blk_b->old) {
       return 1;
    } else {
       return 0;
    }
}

static int find_block(block *blocks, int count, unsigned int offset)
{
   for (int i = 0; i < count; i++) {
      if (blocks[i].old == offset) {
         return i;
      }
   }
   return -1;
}

static void add_ref(block *blk, unsigned level_script, unsigned offset, unsigned char type)
{
   block_ref *ref = &blk->refs[blk->ref_count];
   ref->level = level_script;
   ref->offset = offset;
   ref->type = type;
   blk->ref_count++;
   if (blk->ref_count >= MAX_REFS) {
      ERROR("ref_count for %X = %d\n", blk->old, blk->ref_count);
   }
}

static int walk_scripts(block *blocks, int block_count, unsigned char *buf, unsigned int in_length, unsigned level_script, unsigned script_end)
{
   unsigned off = level_script;
   while (off < script_end) {
      unsigned cmd = read_u32_be(&buf[off]);
      switch (buf[off]) {
         case 0x00: // level script
         case 0x01: // level script
            if (buf[off+1] == 0x10) {
               unsigned block_off = read_u32_be(&buf[off+4]);
               unsigned block_end = read_u32_be(&buf[off+8]);
               if ((block_off & 0xFF000000) == (block_end & 0xFF000000) && buf[off+0x3] == buf[off+0xC]) {
                  INFO("%07X: %08X %08X %08X %08X\n", off, cmd, block_off, block_end, read_u32_be(&buf[off+0xC]));
                  int idx = find_block(blocks, block_count, block_off);
                  if (idx < 0) {
                     idx = block_count;
                     blocks[idx].old = block_off;
                     blocks[idx].old_end = block_end;
                     blocks[idx].type = BLOCK_LEVEL;
                     block_count++;
                     // recurse
                     block_count = walk_scripts(blocks, block_count, buf, in_length, block_off, block_end);
                  }
                  add_ref(&blocks[idx], level_script, off - level_script, buf[off]);
               }
            }
            break;
         case 0x17: // raw data
         case 0x18: // MIO0
         case 0x1A: // MIO0
            if (buf[off+1] == 0x0C) {
               unsigned block_off = read_u32_be(&buf[off+4]);
               unsigned block_end = read_u32_be(&buf[off+8]);
               if ((block_off & 0xFF000000) == (block_end & 0xFF000000)) {
                  INFO("%07X: %08X %08X %08X\n", off, cmd, block_off, block_end);
                  int idx = find_block(blocks, block_count, block_off);
                  if (idx < 0) {
                     idx = block_count;
                     blocks[idx].old = block_off;
                     blocks[idx].old_end = block_end;
                     switch (buf[off]) {
                        case 0x17: // raw data
                           blocks[idx].type = BLOCK_RAW;
                           blocks[idx].compressible = 1;
                           break;
                        case 0x18: // MIO0
                        case 0x1A: // MIO0
                           blocks[idx].type = BLOCK_MIO0;
                           break;
                     }
                     block_count++;
                  }
                  add_ref(&blocks[idx], level_script, off - level_script, buf[off]);
               }
            }
            break;
         default:
            break;
      }
      // could increment by buf[off+1] command length, but trying to be smart might miss things
      off += 0x4;
   }
   return block_count;
}

static int find_sequence_bank(block *blocks, int block_count, unsigned char *buf, unsigned buf_len)
{
   unsigned upper, lower;
   unsigned offset;
   unsigned end, cur;
   int count;
   
   upper = read_u16_be(&buf[0xD4714 + 2]);
   lower = read_u16_be(&buf[0xD471C + 2]);
   if (lower & 0x8000) {
      upper--;
   }
   offset = (upper << 16) + lower;

   // find end
   end = offset;
   count = read_u16_be(&buf[offset + 2]);
   for (int i = 0; i < count; i++) {
      // offset relative to sequence bank + length
      cur = offset + read_u32_be(&buf[offset + 8*i]) + read_u32_be(&buf[offset + 8*i + 4]);
      if (cur > end) {
         end = cur;
      }
   }

   // add sequence bank to table
   if (offset < buf_len && end < buf_len) {
      blocks[block_count].old     = offset;
      blocks[block_count].old_end = end;
      blocks[block_count].type    = BLOCK_RAW;
      add_ref(&blocks[block_count], 0, 0xD4714, 0xFE);
      // TODO: removed these because the code below handles all of them
      // add_ref(&blocks[block_count], 0, 0xD4768, 0xFE);
      // add_ref(&blocks[block_count], 0, 0xD4784, 0xFE);
      block_count++;
   }

   return block_count;
}

static int find_some_block(block *blocks, int block_count, unsigned char *buf, unsigned buf_len)
{
   unsigned upper, lower;
   unsigned offset, end;
   
   upper = read_u16_be(&buf[0x101BB0 + 2]);
   lower = read_u16_be(&buf[0x101BB4 + 2]);
   if (lower & 0x8000) {
      upper--;
   }
   offset = (upper << 16) + lower;
   upper = read_u16_be(&buf[0x101BB8 + 2]);
   lower = read_u16_be(&buf[0x101BC0 + 2]);
   if (lower & 0x8000) {
      upper--;
   }
   end = (upper << 16) + lower;

   // add this block to table
   if (offset < buf_len && end < buf_len) {
      blocks[block_count].old     = offset;
      blocks[block_count].old_end = end;
      blocks[block_count].type    = BLOCK_RAW;
      add_ref(&blocks[block_count], 0, 0x101BB0, 0xFD);
      block_count++;
   }

   return block_count;
}

// set different parameters for G_SETCOMBINE blending parameters
static void fix_f3d(unsigned char *buf, int len)
{
   static const unsigned char f3d_combine_old[] = {0xFC, 0x12, 0x7F, 0xFF, 0xFF, 0xFF, 0xF8, 0x38};
   static const unsigned char f3d_combine_new[] = {0xFC, 0x12, 0x18, 0x24, 0xFF, 0x33, 0xFF, 0xFF};
   for (int i = 0; i < len; i++) {
      if (!memcmp(&buf[i], f3d_combine_old, sizeof(f3d_combine_old))) {
         memcpy(&buf[i], f3d_combine_new, sizeof(f3d_combine_new));
      }
   }
}

// set geo layout drawing layer from 6 to 4
static void fix_geo(unsigned char *buf, int len)
{
   static const unsigned char geo_dl_old[] = {0x15, 0x06, 0x00, 0x00, 0x0E};
   static const unsigned char geo_dl_new[] = {0x15, 0x04, 0x00, 0x00, 0x0E};
   for (int i = 0; i < len; i++) {
      if (!memcmp(&buf[i], geo_dl_old, sizeof(geo_dl_old))) {
         memcpy(&buf[i], geo_dl_new, sizeof(geo_dl_new));
      }
   }
}

// find and compact/compress all MIO0 blocks
// config: configuration to determine alignment and compression
// in_buf: buffer containing entire contents of SM64 data in big endian
// length: length of in_buf and max size of out_buf
// out_buf: buffer containing extended SM64
// returns new size in out_buf, rounded up to nearest 4MB
static int sm64_compress_mio0(const compress_config *config,
                              unsigned char *in_buf,
                              unsigned int in_length,
                              unsigned char *out_buf)
{
#define MAX_BLOCKS 512
#define ENTRY_SCRIPT 0x108A10 // hard-coded level entry
#define SEGMENT2_ROM_OFFSET 0x800000
#define SEGMENT2_ROM_END    0x81BB64
   block block_table[MAX_BLOCKS];
   unsigned char *tmp_raw = NULL;
   unsigned char *tmp_cmp = NULL;
   int block_count = 0;
   int out_length;
   int cur_offset;

   memset(block_table, 0, sizeof(block_table));

   // hard code ASM pointer
   block_table[0].old     = SEGMENT2_ROM_OFFSET;
   block_table[0].old_end = SEGMENT2_ROM_END;
   block_table[0].type    = BLOCK_MIO0;
   add_ref(&block_table[0], 0, 0x3AC0, 0xFF);
   block_count++;

   // find blocks in level scripts
   block_count = walk_scripts(block_table, block_count, in_buf, in_length, ENTRY_SCRIPT, ENTRY_SCRIPT + 0x30);
   // find sequence bank block (usually 0x02F00000 or 0x03E00000)
   block_count = find_sequence_bank(block_table, block_count, in_buf, in_length);
   // some block (usually ROM 0x01200000) is DMAd to 0x80400000
   block_count = find_some_block(block_table, block_count, in_buf, in_length);
   printf("count: %d\n", block_count);

   // sort the blocks
   qsort(block_table, block_count, sizeof(block_table[0]), compare_block);

   // debug table
#if 0
   for (int i = 0; i < block_count; i++) {
      block *blk = &block_table[i];
      INFO("%08X %08X: %02X\n", blk->old, blk->old_end, blk->type);
      for (int j = 0; j < blk->ref_count; j++) {
         INFO("  %08X %08X: %02X\n", blk->refs[j].level, blk->refs[j].offset, blk->refs[j].type);
      }
   }
#endif

   // allocate 1MB buffer for MIO0 compression
   if (config->compress) {
      tmp_raw = malloc(512*KB);
      tmp_cmp = malloc(512*KB);
   }

#define EXT_ROM_OFFSET 0x800000
   cur_offset = EXT_ROM_OFFSET;
   for (int i = 0; i < block_count; i++) {
      block *blk = &block_table[i];
      // only relocate extended data
      if (blk->old < EXT_ROM_OFFSET) {
         blk->new = blk->old;
         blk->new_end = blk->old_end;
      } else {
         unsigned char *src;
         int src_len;
         int block_len = blk->old_end - blk->old;
         // implement fixes
         // TODO: this is liberally applied to all data
         // TODO: this assumes fake MIO0 headers
         if (config->fix_f3d) {
            fix_f3d(&in_buf[blk->old], block_len);
         }
         if (config->fix_geo) {
            fix_geo(&in_buf[blk->old], block_len);
         }
         if (config->compress && blk->type == BLOCK_MIO0) {
            // decompress to remove fake header and recompress
            int raw_len = mio0_decode(&in_buf[blk->old], tmp_raw, NULL);
            int cmp_len = mio0_encode(tmp_raw, raw_len, tmp_cmp);
            src = tmp_cmp;
            src_len = cmp_len;
            INFO("Compressed %08X[%06X=%06X] => %08X[%06X]\n", blk->old, block_len, raw_len, cur_offset, cmp_len);
         } else if(config->compress && blk->compressible) {
            // compress blocks that don't have a fake header and are compressible
            int cmp_len = mio0_encode(&in_buf[blk->old], block_len, tmp_cmp);
            src = tmp_cmp;
            src_len = cmp_len;
            INFO("Compressed %08X[%06X] => %08X[%06X]\n", blk->old, block_len, cur_offset, cmp_len);
            for (int r = 0; r < blk->ref_count; r++) {
               if (blk->refs[r].type == 0x17) {
                  blk->refs[r].type = 0x18;
               } else {
                  ERROR("Block %08X ref %X:%X type = %02X\n", blk->old, blk->refs[r].level, blk->refs[r].offset, blk->refs[r].type);
               }
            }
         } else {
            src = &in_buf[blk->old];
            src_len = block_len;
         }
         if (src_len < 0) {
            ERROR("%d: old: %X %X cur: %X %X\n", i, blk->old, blk->old_end, cur_offset, src_len);
         }
         // copy new data
         memcpy(&out_buf[cur_offset], src, src_len);
         // assign new offsets
         blk->new = cur_offset;
         cur_offset = ALIGN(cur_offset + src_len, config->alignment);
         blk->new_end = cur_offset;
      }
   }

   // update references
   for (int i = 0; i < block_count; i++) {
      block *blk = &block_table[i];
      // only relocate extended data
      if (blk->old != blk->new || blk->old_end != blk->new_end) {
         for (int r = 0; r < blk->ref_count; r++) {
            if (blk->refs[r].type == 0xFF) {
               unsigned offset = blk->refs[r].offset;
               unsigned addr_low = blk->new & 0xFFFF;
               unsigned addr_high = (blk->new >> 16) & 0xFFFF;
               INFO("Updating ASM @ %08X: %08X %08X %08X %08X\n", offset,
                     read_u32_be(&out_buf[offset + 0]), read_u32_be(&out_buf[offset + 4]),
                     read_u32_be(&out_buf[offset + 8]), read_u32_be(&out_buf[offset + 0xC]));
               // ADDIU sign extends which causes the summed high to be 1 less if low MSb is set
               if (addr_low & 0x8000) {
                  addr_high++;
               }
               write_u16_be(&out_buf[offset + 0x2], addr_high);
               write_u16_be(&out_buf[offset + 0xE], addr_low);

               addr_low = blk->new_end & 0xFFFF;
               addr_high = (blk->new_end >> 16) & 0xFFFF;
               if (addr_low & 0x8000) {
                  addr_high++;
               }
               write_u16_be(&out_buf[offset + 0x6], addr_high);
               write_u16_be(&out_buf[offset + 0xA], addr_low);
               INFO("Updated ASM  @ %08X: %08X %08X %08X %08X\n", offset,
                     read_u32_be(&out_buf[offset + 0]), read_u32_be(&out_buf[offset + 4]),
                     read_u32_be(&out_buf[offset + 8]), read_u32_be(&out_buf[offset + 0xC]));
            } else if (blk->refs[r].type == 0xFE) { // sequence bank
               unsigned addr_low = blk->new & 0xFFFF;
               unsigned addr_high = (blk->new >> 16) & 0xFFFF;
               // ADDIU sign extends which causes the summed high to be 1 less if low MSb is set
               if (addr_low & 0x8000) {
                  addr_high++;
               }
               INFO("Updating ASM @ %08X: %08X %08X\n", 0xD4784,
                     read_u32_be(&out_buf[0xD4784 + 0]), read_u32_be(&out_buf[0xD4784 + 4]));
               // 0D4714 80319714 3C04007B   lui   $a0, 0x7b
               // 0D4718 80319718 AC450000   sw    $a1, ($v0)
               // 0D471C 8031971C 24840860   addiu $a0, $a0, 0x860
               write_u16_be(&out_buf[0xD4714 + 0x2], addr_high);
               write_u16_be(&out_buf[0xD471C + 0x2], addr_low);
               // 0D4768 80319768 3C04007B   lui   $a0, 0x7b
               // 0D476C 8031976C AC620000   sw    $v0, ($v1)
               // 0D4770 80319770 24840860   addiu $a0, $a0, 0x860
               write_u16_be(&out_buf[0xD4768 + 0x2], addr_high);
               write_u16_be(&out_buf[0xD4770 + 0x2], addr_low);
               // 0D4784 80319784 3C05007B   lui   $a1, 0x7b
               // 0D4788 80319788 24A50860   addiu $a1, $a1, 0x860
               write_u16_be(&out_buf[0xD4784 + 0x2], addr_high);
               write_u16_be(&out_buf[0xD4788 + 0x2], addr_low);
               INFO("Updated ASM  @ %08X: %08X %08X\n", 0xD4784,
                     read_u32_be(&out_buf[0xD4784 + 0]), read_u32_be(&out_buf[0xD4784 + 4]));
            } else if (blk->refs[r].type == 0xFD) { // some other data
               unsigned addr_low = blk->new & 0xFFFF;
               unsigned addr_high = (blk->new >> 16) & 0xFFFF;
               INFO("Updating ASM @ %08X: %08X %08X %08X %08X %08X\n", 0x101BB0,
                     read_u32_be(&out_buf[0x101BB0 + 0]), read_u32_be(&out_buf[0x101BB0 + 4]),
                     read_u32_be(&out_buf[0x101BB0 + 8]), read_u32_be(&out_buf[0x101BB0 + 0xC]),
                     read_u32_be(&out_buf[0x101BB0 + 0x10]));
               // 101BB0 lui   $a1, 0x120
               // 101BB4 ori   $a1, $a1, 0x0
               write_u16_be(&out_buf[0x101BB0 + 0x2], addr_high);
               write_u16_be(&out_buf[0x101BB4 + 0x2], addr_low);

               addr_low = blk->new_end & 0xFFFF;
               addr_high = (blk->new_end >> 16) & 0xFFFF;
               // 101BB8 lui   $a2, 0x130
               // 101BBC jal   DmaCopy (0x278504)
               // 101BC0 ori   $a2, $a2, 0x0
               write_u16_be(&out_buf[0x101BB8 + 0x2], addr_high);
               write_u16_be(&out_buf[0x101BC0 + 0x2], addr_low);
               INFO("Updated ASM  @ %08X: %08X %08X %08X %08X %08X\n", 0x101BB0,
                     read_u32_be(&out_buf[0x101BB0 + 0]), read_u32_be(&out_buf[0x101BB0 + 4]),
                     read_u32_be(&out_buf[0x101BB0 + 8]), read_u32_be(&out_buf[0x101BB0 + 0xC]),
                     read_u32_be(&out_buf[0x101BB0 + 0x10]));
            } else {
               int level_idx = find_block(block_table, block_count, blk->refs[r].level);
               if (level_idx < 0) {
                  ERROR("Error: could not locate ref %08X in block %08X\n", blk->refs[r].level, blk->old);
               } else {
                  block *level = &block_table[level_idx];
                  unsigned offset = level->new + blk->refs[r].offset;
                  INFO("Updating @ %08X:%08X %02X 0C %02X %02X %08X-%08X to %02X 0C 00 %02X %08X-%08X\n", level->old, level->new,
                        out_buf[offset], out_buf[offset + 2], out_buf[offset + 3],
                        read_u32_be(&out_buf[offset + 4]), read_u32_be(&out_buf[offset + 8]),
                        blk->refs[r].type, out_buf[offset + 3], blk->new, blk->new_end);
                  out_buf[offset] = blk->refs[r].type;
                  // some commands have upper byte of segment set to 0x01
                  out_buf[offset + 2] = 0x00;
                  write_u32_be(&out_buf[offset + 4], blk->new);
                  write_u32_be(&out_buf[offset + 8], blk->new_end);
               }
            }
         }
      }
   }

   // TODO: figure out what is going on with custom level scripts 17 and 10 in RAM expansion
   // TODO: move allocation of memory pool to 0x80400000+ and revert audio code to use it?
   // detect audio patch and fix it
   if (out_buf[0xd48b6] == 0x80 && out_buf[0xd48b7] == 0x3D) {
      INFO("Moving sound allocation from 0x803D0000 to 0x805C0000\n");
      out_buf[0xd48b7] = 0x5C;
   }

#define DUMP_DIR "dump"
   if (config->dump) {
      make_dir(DUMP_DIR);
      for (int i = 0; i < block_count; i++) {
         char fname[FILENAME_MAX];
         block *blk = &block_table[i];
         sprintf(fname, "%s/%07X.%07X.old.bin", DUMP_DIR, blk->old, blk->old);
         (void)write_file(fname, &in_buf[blk->old], blk->old_end - blk->old);
         sprintf(fname, "%s/%07X.%07X.new.bin", DUMP_DIR, blk->old, blk->new);
         (void)write_file(fname, &out_buf[blk->new], blk->new_end - blk->new);
      }
   }

   if (tmp_raw != NULL) {
      free(tmp_raw);
      free(tmp_cmp);
   }

   // align output length to nearest MB
   out_length = ALIGN(cur_offset, 1*MB);

   return out_length;
}


int main(int argc, char *argv[])
{
   char out_filename[FILENAME_MAX];
   compress_config config;
   unsigned char *in_buf = NULL;
   unsigned char *out_buf = NULL;
   long in_size;
   long bytes_written;
   int out_size;

   // get configuration from arguments
   config = default_config;
   parse_arguments(argc, argv, &config);
   if (config.out_filename == NULL) {
      config.out_filename = out_filename;
      generate_filename(config.in_filename, config.out_filename, "out.z64");
   }

   // read input file into memory
   in_size = read_file(config.in_filename, &in_buf);
   if (in_size <= 0) {
      ERROR("Error reading input file \"%s\"\n", config.in_filename);
      exit(1);
   }

   // TODO: confirm valid SM64

   // allocate output memory
   out_buf = malloc(in_size);
   memset(out_buf, 0x01, in_size);

   // copy first 8MB from input to output
   memcpy(out_buf, in_buf, 8*MB);

   // compact the SM64 blocks and adjust pointers
   out_size = sm64_compress_mio0(&config, in_buf, in_size, out_buf);

   // update N64 header CRC
   sm64_update_checksums(out_buf);

   // write to output file
   bytes_written = write_file(config.out_filename, out_buf, out_size);
   if (bytes_written < out_size) {
      ERROR("Error writing bytes to output file \"%s\"\n", config.out_filename);
      exit(1);
   }

   printf("Size: %dMB -> %dMB\n", (int)in_size/(1*MB), (int)out_size/(1*MB));

   return 0;
}
