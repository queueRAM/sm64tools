#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <capstone/capstone.h>

#include "config.h"
#include "libmio0.h"
#include "n64graphics.h"
#include "utils.h"

// extract opcode from instruction MSB
#define OPCODE(VAL_) ((VAL_) & 0xFC)
#define LUI_REG(VAL_) ((VAL_) & 0x1F)

const char asm_header[] = 
   ".set noat      # allow manual use of $at\n"
   ".set noreorder # don't insert nops after branches\n"
   "\n"
   ".global _start\n"
   "\n"
   "_start:\n";

static int lookup_start(split_section *sections, int section_count, unsigned int address)
{
   int i;
   // TODO: binary search
   for (i = 0; i < section_count; i++) {
      if (sections[i].start == address) {
         return i;
      }
   }
   return -1;
}

static int lookup_end(split_section *sections, int section_count, unsigned int address)
{
   int i;
   // TODO: binary search
   for (i = 0; i < section_count; i++) {
      if (sections[i].end == address) {
         return i;
      }
   }
   return -1;
}

#define MAX_LOCALS 32
typedef struct _local_label
{
   unsigned int offsets[MAX_LOCALS];
   int count;
} local_label;

static int find_local(local_label *locals, unsigned int offset)
{
   int i;
   for (i = 0; i < locals->count; i++) {
      if (locals->offsets[i] == offset) {
         return i;
      }
   }
   return -1;
}

static void add_local(local_label *locals, unsigned int offset)
{
   if (find_local(locals, offset) >= 0) {
      return;
   }
   locals->offsets[locals->count] = offset;
   locals->count++;
}

static void print_spaces(FILE *fp, int count)
{
   int i;
   for (i = 0; i < count; i++) {
      fputc(' ', fp);
   }
}

static void write_behavior(FILE *out, unsigned char *data, split_section *sections, int section_count, int s)
{
   unsigned int a;
   unsigned int len;
   split_section *sec;
   (void)section_count;
   sec = &sections[s];
   a = sec->start;
   while (a < sec->end) {
      switch (data[a]) {
         case 0x0C:
         case 0x2A:
         case 0x02:
         case 0x23:
         case 0x14:
         case 0x2F:
         case 0x04:
         case 0x27:
            len = 8;
            break;
         case 0x1C:
         case 0x2B:
         case 0x2C:
         case 0x29:
            len = 12;
            break;
         case 0x30:
            len = 20;
            break;
         default:
            len = 4;
            break;
      }
      fprintf(out, ".byte ");
      fprint_hex_source(out, &data[a], len);
      fprintf(out, "\n");
      a += len;
   }
}

static void write_level(FILE *out, unsigned char *data, split_section *sections, int section_count, int s)
{
   char start_label[128];
   char end_label[128];
   char dst_label[128];
   split_section *sec;
   unsigned int ptr_start;
   unsigned int ptr_end;
   unsigned int dst;
   unsigned int a;
   int indent;
   int i;

   sec = &sections[s];

   a = sec->start;
   while (a < sec->end) {
      // length = 0 ends level script
      if (data[a+1] == 0) {
         break;
      }
      switch (data[a]) {
         case 0x00: // load and jump from ROM into a RAM segment
         case 0x17: // copy uncompressed data from ROM to a RAM segment
         case 0x18: // decompress MIO0 data from ROM and copy it into a RAM segment
         case 0x1A: // decompress MIO0 data from ROM and copy it into a RAM segment (for texture only segments?)
            ptr_start = read_u32_be(&data[a+4]);
            ptr_end = read_u32_be(&data[a+8]);
            i = lookup_start(sections, section_count, ptr_start);
            if (i < 0) {
               sprintf(start_label, "0x%08X", ptr_start);
            } else {
               strcpy(start_label, sections[i].label);
            }
            i = lookup_end(sections, section_count, ptr_end);
            if (i < 0) {
               sprintf(end_label, "0x%08X", ptr_end);
            } else {
               sprintf(end_label, "%s_end", sections[i].label);
            }
            fprintf(out, ".word 0x");
            for (i = 0; i < 4; i++) {
               fprintf(out, "%02X", data[a+i]);
            }
            fprintf(out, ", %s, %s", start_label, end_label);
            for (i = 12; i < data[a+1]; i++) {
               if ((i & 0x3) == 0) {
                  fprintf(out, ", 0x");
               }
               fprintf(out, "%02X", data[a+i]);
            }
            fprintf(out, "\n");
            break;
         case 0x16: // load ASM into RAM
            dst       = read_u32_be(&data[a+0x4]);
            ptr_start = read_u32_be(&data[a+0x8]);
            ptr_end   = read_u32_be(&data[a+0xc]);
            i = lookup_start(sections, section_count, dst);
            if (i < 0) {
               sprintf(dst_label, "0x%08X", dst);
            } else {
               strcpy(dst_label, sections[i].label);
            }
            i = lookup_start(sections, section_count, ptr_start);
            if (i < 0) {
               sprintf(start_label, "0x%08X", ptr_start);
            } else {
               strcpy(start_label, sections[i].label);
            }
            i = lookup_end(sections, section_count, ptr_end);
            if (i < 0) {
               sprintf(end_label, "0x%08X", ptr_end);
            } else {
               sprintf(end_label, "%s_end", sections[i].label);
            }
            fprintf(out, ".word 0x");
            for (i = 0; i < 4; i++) {
               fprintf(out, "%02X", data[a+i]);
            }
            fprintf(out, ", %s, %s, %s\n", dst_label, start_label, end_label);
            break;
         default:
            fprintf(out, ".byte ");
            fprint_hex_source(out, &data[a], data[a+1]);
            fprintf(out, "\n");
            break;
      }
      a += data[a+1];
   }
   // align to next 16-byte boundary
   if (a & 0x0F) {
      fprintf(out, "# begin %s alignment 0x%X\n", sec->label, a);
      fprintf(out, ".byte ");
      fprint_hex_source(out, &data[a], ALIGN(a, 16) - a);
      fprintf(out, "\n");
      a = ALIGN(a, 16);
   }
   fprintf(out, "# begin %s geo 0x%X\n", sec->label, a);
   // remaining is geo layout script
   indent = 0;
   while (a < sec->end) {
      switch (data[a]) {
         case 0x00: // end
         case 0x01:
         case 0x03:
         case 0x04:
         case 0x05:
         case 0x09: // undocumented?
         case 0x0B:
         case 0x0C:
         case 0x17:
         case 0x20:
            i = 4;
            break;
         case 0x02:
         case 0x0D:
         case 0x0E:
         case 0x14:
         case 0x15:
         case 0x16:
         case 0x18:
         case 0x19: // undocumented?
         case 0x1D:
            i = 8;
            break;
         case 0x08:
         case 0x11: // this is a guess, looking at 0x26A170
         case 0x13:
         case 0x1C:
            i = 12;
            break;
         case 0x10:
            i = 16;
            break;
         case 0x0F: // Kaze has 8
            i = 20;
            break;
         case 0x0A:
            i = 8;
            if (data[a+1]) {
               i += 4;
            }
            break;
         default:
            i = 4;
            fprintf(stderr, "WHY? %06X %2X\n", a, data[a]);
      }
      if (data[a] == 0x05 && indent > 1) {
         indent -= 2;
      }
      fprintf(out, ".byte ");
      print_spaces(out, indent);
      fprint_hex_source(out, &data[a], i);
      fprintf(out, "\n");
      // TODO: remove this debug
      if (sec->start == 0x269EA0) {
         printf("%06X: ", a);
         print_spaces(stdout, indent);
         printf("[ ");
         print_hex(&data[a], i);
         printf("]\n");
      }
      if (data[a] == 0x04) {
         indent += 2;
      }
      a += i;
   }
}

static void split_file(unsigned char *data, unsigned int length, rom_config *config)
{
#define GEN_DIR     "gen"
#define MAKEFILENAME GEN_DIR "/Makefile.gen"
#define BIN_DIR      GEN_DIR "/bin"
#define MIO0_DIR     GEN_DIR "/bin"
#define TEXTURE_DIR  GEN_DIR "/textures"
#define LEVEL_DIR    GEN_DIR "/levels"

   char asmfilename[512];
   char outfilename[512];
   char outfilepath[512];
   char mio0filename[512];
   char start_label[256];
   char end_label[256];
   char maketmp[256];
   char *makeheader_mio0;
   char *makeheader_level;
   FILE *fasm;
   FILE *fmake;
   int i, j, s;
   unsigned int w, h;
   unsigned int last_end = 0;
   unsigned int ptr_start, ptr_end;
   csh handle;
   cs_insn *insn;
   int count;
   int level_alloc;
   split_section *sections = config->sections;

   // create directories
   make_dir(GEN_DIR);
   make_dir(BIN_DIR);
   make_dir(MIO0_DIR);
   make_dir(TEXTURE_DIR);
   make_dir(LEVEL_DIR);

   // open main assembly file and write header
   sprintf(asmfilename, "%s/%s.s", GEN_DIR, config->basename);
   fasm = fopen(asmfilename, "w");
   if (fasm == NULL) {
      ERROR("Error opening %s\n", asmfilename);
      exit(3);
   }
   fprintf(fasm, asm_header);

   // open capstone disassembler
   if (cs_open(CS_ARCH_MIPS, CS_MODE_MIPS64 + CS_MODE_BIG_ENDIAN, &handle) != CS_ERR_OK) {
      ERROR("Error initializing disassembler\n");
      exit(3);
   }
   cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
   cs_option(handle, CS_OPT_SKIPDATA, CS_OPT_ON);

   for (s = 0; s < config->section_count; s++) {
      split_section *sec = &sections[s];

      // error checking
      if (sec->start >= length || sec->end > length) {
         fprintf(stderr, "Error: section past end: 0x%X, 0x%X (%s) > 0x%X\n",
               sec->start, sec->end, sec->label ? sec->label : "", length);
         exit(4);
      }

      // fill gaps between regions
      if (sec->start != last_end) {
         sprintf(outfilename, "%s/%s.%06X.bin", BIN_DIR, config->basename, last_end);
         write_file(outfilename, &data[last_end], sec->start - last_end);
         fprintf(fasm, "L%06X:\n", last_end);
         fprintf(fasm, ".incbin \"%s\"\n", outfilename);
      }

      switch (sec->type)
      {
         case TYPE_HEADER:
            fprintf(fasm, ".section \"header\"\n"
                          ".byte  0x%02X", data[sec->start]);
            for (i = 1; i < 4; i++) {
               fprintf(fasm, ", 0x%02X", data[sec->start + i]);
            }
            fprintf(fasm, " # PI BSD Domain 1 register\n");
            fprintf(fasm, ".word  0x%08X # clock rate setting\n", read_u32_be(&data[sec->start + 0x4]));
            fprintf(fasm, ".word  0x%08X # entry point\n", read_u32_be(&data[sec->start + 0x8]));
            fprintf(fasm, ".word  0x%08X # release\n", read_u32_be(&data[sec->start + 0xc]));
            fprintf(fasm, ".word  0x%08X # checksum1\n", read_u32_be(&data[sec->start + 0x10]));
            fprintf(fasm, ".word  0x%08X # checksum2\n", read_u32_be(&data[sec->start + 0x14]));
            fprintf(fasm, ".word  0x%08X # unknown\n", read_u32_be(&data[sec->start + 0x18]));
            fprintf(fasm, ".word  0x%08X # unknown\n", read_u32_be(&data[sec->start + 0x1C]));
            fprintf(fasm, ".ascii \"");
            fwrite(&data[sec->start + 0x20], 1, 20, fasm);
            fprintf(fasm, "\" # ROM name: 20 bytes\n");
            fprintf(fasm, ".word  0x%08X # unknown\n", read_u32_be(&data[sec->start + 0x34]));
            fprintf(fasm, ".word  0x%08X # cartridge\n", read_u32_be(&data[sec->start + 0x38]));
            fprintf(fasm, ".ascii \"");
            fwrite(&data[sec->start + 0x3C], 1, 2, fasm);
            fprintf(fasm, "\"       # cartridge ID\n");
            fprintf(fasm, ".ascii \"");
            fwrite(&data[sec->start + 0x3E], 1, 1, fasm);
            fprintf(fasm, "\"        # country\n");
            fprintf(fasm, ".byte  0x%02X       # version\n\n", data[sec->start + 0x3F]);
            break;
         case TYPE_BIN:
            if (sec->label == NULL || sec->label[0] == '\0') {
               sprintf(outfilename, "%s/%s.%06X.bin", BIN_DIR, config->basename, sec->start);
            } else {
               sprintf(outfilename, "%s/%s.%06X.%s.bin", BIN_DIR, config->basename, sec->start, sec->label);
            }
            write_file(outfilename, &data[sec->start], sec->end - sec->start);
            if (sec->label == NULL || sec->label[0] == '\0' || strcmp(sec->label, "header") != 0) {
               if (sec->label == NULL || sec->label[0] == '\0') {
                  sprintf(start_label, "L%06X", sec->start);
               } else {
                  strcpy(start_label, sec->label);
               }
               fprintf(fasm, "%s:\n", start_label);
               fprintf(fasm, ".incbin \"%s\"\n", outfilename);
               fprintf(fasm, "%s_end:\n", start_label);
            }
            break;
         case TYPE_MIO0:
            // fill previous MIO0 blocks
            fprintf(fasm, ".space 0x%05x, 0x01 # %s\n", sec->end - sec->start, sec->label);
            break;
         case TYPE_PTR:
            ptr_start = read_u32_be(&data[sec->start]);
            ptr_end = read_u32_be(&data[sec->start + 4]);
            i = lookup_start(sections, config->section_count, ptr_start);
            if (i < 0) {
               sprintf(start_label, "L%06X", ptr_start);
            } else {
               strcpy(start_label, sections[i].label);
            }
            i = lookup_end(sections, config->section_count, ptr_end);
            if (i < 0) {
               sprintf(end_label, "L%06X", ptr_end);
            } else {
               sprintf(end_label, "%s_end", sections[i].label);
            }
            fprintf(fasm, ".word %s, %s\n", start_label, end_label);
            break;
         case TYPE_LA:
            // TODO: more intelligent register alignment
            // TODO: move into TYPE_ASM
            if (OPCODE(data[sec->start])   == 0x3C && OPCODE(data[sec->start+4])  == 0x3C &&
                OPCODE(data[sec->start+8]) == 0x24 && OPCODE(data[sec->start+12]) == 0x24) {
               unsigned short addr_low, addr_high;
               unsigned int reg_start, reg_end;
               // reconstruct address
               addr_high = read_u16_be(&data[sec->start + 0x2]);
               addr_low = read_u16_be(&data[sec->start + 0xe]);
               // ADDIU sign extends which causes the encoded high val to be +1 if low MSb is set
               if (addr_low & 0x8000) {
                  addr_high--;
               }
               ptr_start = (addr_high << 16) | addr_low;
               // reconstruct end address
               addr_high = read_u16_be(&data[sec->start + 0x6]);
               addr_low = read_u16_be(&data[sec->start + 0xa]);
               // ADDIU sign extends which causes the encoded high val to be +1 if low MSb is set
               if (addr_low & 0x8000) {
                  addr_high--;
               }
               ptr_end = (addr_high << 16) | addr_low;
               i = lookup_start(sections, config->section_count, ptr_start);
               if (i < 0) {
                  sprintf(start_label, "L%06X", ptr_start);
               } else {
                  strcpy(start_label, sections[i].label);
               }
               i = lookup_end(sections, config->section_count, ptr_end);
               if (i < 0) {
                  sprintf(end_label, "L%06X", ptr_end);
               } else {
                  sprintf(end_label, "%s_end", sections[i].label);
               }
               reg_start = LUI_REG(data[sec->start+1]);
               reg_end = LUI_REG(data[sec->start+5]);
               fprintf(fasm, "  la    $%s, %s\n", cs_reg_name(handle, reg_start + MIPS_REG_0), start_label);
               fprintf(fasm, "  la    $%s, %s\n", cs_reg_name(handle, reg_end + MIPS_REG_0), end_label);
            } else {
               fprintf(stderr, "Error: 0x%X does not appear to be an LA\n", sec->start);
            }
            break;
         case TYPE_ASM:
            if (sec->label == NULL || sec->label[0] == '\0') {
               sprintf(start_label, "L%06X", sec->start);
            } else {
               strcpy(start_label, sec->label);
            }
            //fprintf(fasm, "\n.org 0x%x", sec->start);
            fprintf(fasm, "\n%s:\n", start_label);
            count = cs_disasm(handle, &data[sec->start], sec->end - sec->start, sec->start, 0, &insn);
            if (count > 0) {
               local_label locals;
               const char *spaces[] = {"      ", "     ", "    ", "   ", "  ", " "};
               int o;
               locals.count = 0;
               // first find referenced local labels
               for (i = 0; i < count; i++) {
                  if (cs_insn_group(handle, &insn[i], MIPS_GRP_JUMP)) {
                     cs_mips *mips = &insn[i].detail->mips;
                     for (o = 0; o < mips->op_count; o++) {
                        if (mips->operands[o].type == MIPS_OP_IMM) {
                           unsigned int sec_offset = (unsigned int)mips->operands[o].imm - sec->start;
                           add_local(&locals, sec_offset);
                           if (locals.count > MAX_LOCALS) {
                              ERROR("Need more then %d locals in %s\n", MAX_LOCALS, sec->label);
                           }
                        }
                     }
                  }
               }

               for (i = 0; i < count; i++) {
                  // handle redirect jump instruction immediates
                  // TODO: memory operations
                  // TODO: pseudo-instruction detection: li, la, bgt, blt
                  int inslen;
                  int ll = find_local(&locals, i*4);
                  if (ll >= 0) {
                     fprintf(fasm, "L%s_0x%X:\n", start_label, i*4);
                  }
                  inslen = MIN(strlen(insn[i].mnemonic), DIM(spaces) - 1);
                  fprintf(fasm, "  %s%s", insn[i].mnemonic, spaces[inslen]);
                  if (cs_insn_group(handle, &insn[i], MIPS_GRP_JUMP)) {
                     cs_mips *mips = &insn[i].detail->mips;
                     for (o = 0; o < mips->op_count; o++) {
                        if (o > 0) {
                           fprintf(fasm, ", ");
                        }
                        switch (mips->operands[o].type) {
                           case MIPS_OP_REG:
                              fprintf(fasm, "$%s", cs_reg_name(handle, mips->operands[o].reg));
                              break;
                           case MIPS_OP_IMM:
                           {
                              unsigned int sec_offset = (unsigned int)mips->operands[o].imm - sec->start;
                              fprintf(fasm, "L%s_0x%X", start_label, sec_offset);
                              break;
                           }
                           default:
                              break;
                        }
                     }
                  } else if (insn[i].id == MIPS_INS_JAL) {
                     char sym[128];
                     unsigned int addr;
                     unsigned int ram_to_rom = config->ram_offset & 0xFFFFFF;
                     int n;
                     cs_mips *mips = &insn[i].detail->mips;
                     addr = (unsigned int)mips->operands[0].imm;
                     n = lookup_start(sections, config->section_count, addr - ram_to_rom);
                     // TODO: move this to the config file
                     if (n < 0) {
                        ram_to_rom = 0x80283280 & 0xFFFFFF;
                        n = lookup_start(sections, config->section_count, addr - ram_to_rom);
                     }
                     if (n >= 0) {
                        sprintf(sym, "%s # 0x%X", sections[n].label, addr);
                     } else {
                        sprintf(sym, "0x%X # TODO: needs label", addr); // TODO: should this be addr + 0x80000000?
                     }
                     fprintf(fasm, "%s", sym);
                  } else if (insn[i].id == MIPS_INS_MTC0 || insn[i].id == MIPS_INS_MFC0) {
                     // workaround bug in capstone/LLVM
                     unsigned char *in = &data[sec->start + i*4];
                     unsigned char rd;
                     cs_mips *mips = &insn[i].detail->mips;
                     // 31-24 23-16 15-8 7-0
                     // 0     1     2    3
                     //       31-26  25-21 20-16 15-11 10-0
                     // mfc0: 010000 00000   rt    rd  00000000000
                     // mtc0: 010000 00100   rt    rd  00000000000
                     // rt = in[1] & 0x1F;
                     rd = (in[2] & 0xF8) >> 3;
                     fprintf(fasm, "$%s, $%d # ", cs_reg_name(handle, mips->operands[0].reg), rd);
                     fprintf(fasm, "%02X%02X%02X%02X", in[0], in[1], in[2], in[3]);
                     fprintf(fasm, " %s", insn[i].op_str);
                  } else {
                     fprintf(fasm, "%s", insn[i].op_str);
                  }
                  fprintf(fasm, "\n");
               }
               cs_free(insn, count);
            } else {
               fprintf(stderr, "ERROR: Failed to disassemble given code!\n");
            }
            fprintf(fasm, "# end %s\n\n", start_label);
            break;
         case TYPE_LEVEL:
            // TODO: some level scripts can't be relocated yet
            if ((strcmp(sec->label, "game_over_level") == 0) ||
                (strcmp(sec->label, "menu_power_level") == 0) ||
                (strcmp(sec->label, "main_level_scripts") == 0)) {
               fprintf(fasm, "\n.global %s\n", sec->label);
               fprintf(fasm, "\n.global %s_end\n", sec->label);
               fprintf(fasm, "%s: # 0x%X\n", sec->label, sec->start);
               write_level(fasm, data, sections, config->section_count, s);
               fprintf(fasm, "%s_end:\n", sec->label);
            } else {
               fprintf(fasm, ".space 0x%05x, 0x01 # %s\n", sec->end - sec->start, sec->label);
            }
            break;
         case TYPE_BEHAVIOR:
            // behaviors are done below
            fprintf(fasm, ".space 0x%05x, 0x01 # %s\n", sec->end - sec->start, sec->label);
            break;
      }
      last_end = sec->end;
   }

   // put MIO0 in separate data section and generate Makefile
   // allocate some space for the .bin makefile targets
   count = 1024; // header space
   level_alloc = 1024;
   i = strlen(" \\\n$(MIO0_DIR)/");
   j = strlen(" \\\n$(LEVEL_DIR)/");
   for (s = 0; s < config->section_count; s++) {
      split_section *sec = &sections[s];
      switch (sec->type) {
         case TYPE_MIO0:
            count += i + strlen(sec->label) + 4;
            break;
         case TYPE_LEVEL:
            level_alloc += j + strlen(sec->label) + 4;
            break;
         default:
            break;
      }
   }

   makeheader_mio0 = malloc(count);
   sprintf(makeheader_mio0, "MIO0_FILES =");

   makeheader_level = malloc(level_alloc);
   sprintf(makeheader_level, "LEVEL_FILES =");

   fmake = fopen(MAKEFILENAME, "w");
   fprintf(fmake, "MIO0_DIR = %s\n\n", MIO0_DIR);
   fprintf(fmake, "TEXTURE_DIR = %s\n\n", TEXTURE_DIR);
   fprintf(fmake, "LEVEL_DIR = %s\n\n", LEVEL_DIR);

   fprintf(fasm, "\n.section \"mio0\"\n");
   for (s = 0; s < config->section_count; s++) {
      split_section *sec = &sections[s];
      switch (sec->type) {
         case TYPE_MIO0:
         {
            char binfilename[512];
            if (sec->label == NULL || sec->label[0] == '\0') {
               sprintf(outfilename, "%06X.mio0", sec->start);
            } else {
               sprintf(outfilename, "%s.mio0", sec->label);
            }
            sprintf(mio0filename, "%s/%s", MIO0_DIR, outfilename);
            write_file(mio0filename, &data[sec->start], sec->end - sec->start);
            if (sec->label == NULL || sec->label[0] == '\0') {
               sprintf(start_label, "L%06X", sec->start);
            } else {
               strcpy(start_label, sec->label);
            }
            fprintf(fasm, ".align 4, 0x01\n");
            fprintf(fasm, ".global %s\n", start_label);
            fprintf(fasm, "%s:\n", start_label);
            fprintf(fasm, ".incbin \"%s\"\n", mio0filename);
            fprintf(fasm, "%s_end:\n", start_label);
            // append to Makefile
            sprintf(maketmp, " \\\n$(MIO0_DIR)/%s", outfilename);
            strcat(makeheader_mio0, maketmp);
            if (sec->label == NULL || sec->label[0] == '\0') {
               sprintf(binfilename, "%s/%06X.bin", MIO0_DIR, sec->start);
            } else {
               sprintf(binfilename, "%s/%s.bin", MIO0_DIR, sec->label);
            }

            // extract MIO0 data
            mio0_decode_file(mio0filename, 0, binfilename);

            // extract texture data
            if (sec->extra) {
               texture *texts = sec->extra;
               int t;
               unsigned int offset = 0;
               sprintf(outfilepath, "%s/%s", TEXTURE_DIR, sec->label);
               make_dir(outfilepath);
               if (sec->label == NULL || sec->label[0] == '\0') {
                  fprintf(fmake, "$(MIO0_DIR)/%06X.bin:", sec->start);
               } else {
                  fprintf(fmake, "$(MIO0_DIR)/%s.bin: ", sec->label);
               }
               printf("Extracting textures from %s\n", sec->label);
               for (t = 0; t < sec->extra_len; t++) {
                  w = texts[t].width;
                  h = texts[t].height;
                  offset = texts[t].offset;
                  switch (texts[t].format) {
                     case FORMAT_IA:
                     {
                        rgba *img = file2ia(binfilename, offset, w, h, texts[t].depth);
                        if (img) {
                           sprintf(outfilepath, "%s/%s/0x%05X.ia%d.png", TEXTURE_DIR, sec->label, offset, texts[t].depth);
                           rgba2png(img, w, h, outfilepath);
                           free(img);
                           fprintf(fmake, " %s", outfilepath);
                        }
                        break;
                     }
                     case FORMAT_RGBA:
                     {
                        rgba *img = file2rgba(binfilename, offset, w, h);
                        if (img) {
                           sprintf(outfilepath, "%s/%s/0x%05X.png", TEXTURE_DIR, sec->label, offset);
                           rgba2png(img, w, h, outfilepath);
                           free(img);
                           fprintf(fmake, " %s", outfilepath);
                        }
                        break;
                     }
                  }
               }
               fprintf(fmake, "\n\t$(N64GRAPHICS) $@ $^\n\n");
            }
#if GENERATE_ALL_PNG
            sprintf(outfilepath, "%s/%s", TEXTURE_DIR, sec->label);
            make_dir(outfilepath);
            w = 32;
            h = filesize(binfilename) / (w * 2);
            rgba *img = file2rgba(binfilename, 0, w, h);
            if (img) {
               sprintf(outfilepath, "%s/%s/ALL.png", TEXTURE_DIR, sec->label);
               rgba2png(img, w, h, outfilepath);
               free(img);
               img = NULL;
            }
#endif // GENERATE_ALL_PNG
            // touch bin, then mio0 files so 'make' doesn't rebuild them right away
            touch_file(binfilename);
            touch_file(mio0filename);
            break;
         }
         case TYPE_LEVEL:
            // TODO: some level scripts can't be relocated yet
            if ((strcmp(sec->label, "game_over_level") != 0) &&
                (strcmp(sec->label, "menu_power_level") != 0) &&
                (strcmp(sec->label, "main_level_scripts") != 0)) {
               FILE *flevel;
               char levelfilename[512];
               if (sec->label == NULL || sec->label[0] == '\0') {
                  sprintf(levelfilename, "%06X.s", sec->start);
               } else {
                  sprintf(levelfilename, "%s.s", sec->label);
               }
               sprintf(outfilename, "%s/%s", LEVEL_DIR, levelfilename);

               // decode and write level data out
               flevel = fopen(outfilename, "w");
               if (flevel == NULL) {
                  perror(outfilename);
                  exit(1);
               }

               write_level(flevel, data, sections, config->section_count, s);

               fclose(flevel);

               if (sec->label == NULL || sec->label[0] == '\0') {
                  sprintf(start_label, "L%06X", sec->start);
               } else {
                  strcpy(start_label, sec->label);
               }
               fprintf(fasm, ".align 4, 0x01\n");
               fprintf(fasm, ".global %s\n", start_label);
               fprintf(fasm, "%s:\n", start_label);
               fprintf(fasm, ".include \"%s\"\n", outfilename);
               fprintf(fasm, "%s_end:\n", start_label);
               // append to Makefile
               sprintf(maketmp, " \\\n$(LEVEL_DIR)/%s", levelfilename);
               strcat(makeheader_level, maketmp);
            }
            break;
         case TYPE_BEHAVIOR:
            fprintf(fasm, "\n.global %s\n", sec->label);
            fprintf(fasm, "\n.global %s_end\n", sec->label);
            fprintf(fasm, "%s: # 0x%X\n", sec->label, sec->start);
            write_behavior(fasm, data, sections, config->section_count, s);
            fprintf(fasm, "%s_end:\n", sec->label);
            break;
         default:
            break;
      }
   }
   fprintf(fmake, "\n\n%s", makeheader_mio0);
   fprintf(fmake, "\n\n%s", makeheader_level);

   // cleanup
   free(makeheader_mio0);
   free(makeheader_level);
   fclose(fmake);
   fclose(fasm);
   cs_close(&handle);
}

static void print_usage(void)
{
   ERROR("n64split ROM CONFIG\n");
}

int main(int argc, char *argv[])
{
   rom_config config;
   long len;
   unsigned char *data;
   int ret_val;

   if (argc < 3) {
      print_usage();
      return 1;
   }

   len = read_file(argv[1], &data);

   if (len <= 0) {
      return 2;
   }

   ret_val = parse_config_file(argv[2], &config);
   if (ret_val != 0) {
      return 2;
   }
   if (validate_config(&config, len)) {
      return 3;
   }

   config.basename = "sm64";

   split_file(data, len, &config);

   return 0;
}

