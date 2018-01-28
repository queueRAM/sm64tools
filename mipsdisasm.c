#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <capstone/capstone.h>

#include "mipsdisasm.h"
#include "utils.h"

#define MIPSDISASM_VERSION "0.2+"

// typedefs
typedef struct
{
   char name[60];
   unsigned int vaddr;
} asm_label;

typedef struct
{
   asm_label *labels;
   int alloc;
   int count;
} label_buf;

typedef struct
{
   // copied from cs_insn structure
   unsigned int id;
   uint8_t bytes[4];
   char op_str[32];
   char mnemonic[16];
   cs_mips_op operands[8];
   uint8_t op_count;
   // n64split-specific data
   int is_jump;
   int linked_insn;
   union
   {
      unsigned int linked_value;
      float linked_float;
   };
   int newline;
} disasm_data;

typedef struct _asm_block
{
   label_buf locals;
   disasm_data *instructions;
   int instruction_count;
   unsigned int offset;
   unsigned int length;
   unsigned int vaddr;
} asm_block;

// hidden disassembler state struct
typedef struct _disasm_state
{
   label_buf globals;

   asm_block *blocks;
   int block_alloc;
   int block_count;

   csh handle;

   asm_syntax syntax;
   int merge_pseudo;
} disasm_state;

// default label buffer allocate
static void labels_alloc(label_buf *buf)
{
   buf->count = 0;
   buf->alloc = 128;
   buf->labels = malloc(sizeof(*buf->labels) * buf->alloc);
}

static void labels_add(label_buf *buf, const char *name, unsigned int vaddr)
{
   if (buf->count >= buf->alloc) {
      buf->alloc *= 2;
      buf->labels = realloc(buf->labels, sizeof(*buf->labels) * buf->alloc);
   }
   asm_label *l = &buf->labels[buf->count];
   // if name is null, generate based on vaddr
   if (name == NULL) {
      sprintf(l->name, "L%08X", vaddr);
   } else {
      strcpy(l->name, name);
   }
   l->vaddr = vaddr;
   buf->count++;
}

static int label_cmp(const void *a, const void *b)
{
   const asm_label *ala = a;
   const asm_label *alb = b;
   // first sort by vaddr, then by name
   if (ala->vaddr > alb->vaddr) {
      return 1;
   } else if (alb->vaddr > ala->vaddr) {
      return -1;
   } else {
      return strcmp(ala->name, alb->name);
   }
}

static void labels_sort(label_buf *buf)
{
   qsort(buf->labels, buf->count, sizeof(buf->labels[0]), label_cmp);
}

// labels: label buffer to search in
// vaddr: virtual address to find
// returns index in buf->labels if found, -1 otherwise
static int labels_find(const label_buf *buf, unsigned int vaddr)
{
   for (int i = 0; i < buf->count; i++) {
      if (buf->labels[i].vaddr == vaddr) {
         return i;
      }
   }
   return -1;
}

// try to find a matching LUI for a given register
static void link_with_lui(disasm_state *state, int block_id, int offset, unsigned int reg, unsigned int mem_imm)
{
   asm_block *block = &state->blocks[block_id];
#define MAX_LOOKBACK 128
   disasm_data *insn = block->instructions;
   // don't attempt to compute addresses for zero offset
   if (mem_imm != 0x0) {
      // end search after some sane max number of instructions
      int end_search = MAX(0, offset - MAX_LOOKBACK);
      for (int search = offset - 1; search >= end_search; search--) {
         // use an `if` instead of `case` block to allow breaking out of the `for` loop
         if (insn[search].id == MIPS_INS_LUI) {
            unsigned int rd = insn[search].operands[0].reg;
            if (reg == rd) {
               unsigned int lui_imm = (unsigned int)insn[search].operands[1].imm;
               unsigned int addr = ((lui_imm << 16) + mem_imm);
               insn[search].linked_insn = offset;
               insn[search].linked_value = addr;
               insn[offset].linked_insn = search;
               insn[offset].linked_value = addr;
               // if not ORI, create global data label if one does not exist
               if (insn[offset].id != MIPS_INS_ORI) {
                  int label = labels_find(&state->globals, addr);
                  if (label < 0) {
                     char label_name[32];
                     sprintf(label_name, "D_%08X", addr);
                     labels_add(&state->globals, label_name, addr);
                  }
               }
               break;
            }
         } else if (insn[search].id == MIPS_INS_LW ||
                    insn[search].id == MIPS_INS_LD ||
                    insn[search].id == MIPS_INS_ADDIU ||
                    insn[search].id == MIPS_INS_ADDU ||
                    insn[search].id == MIPS_INS_ADD ||
                    insn[search].id == MIPS_INS_SUB ||
                    insn[search].id == MIPS_INS_SUBU) {
            unsigned int rd = insn[search].operands[0].reg;
            if (reg == rd) {
               // ignore: reg is pointer, offset is probably struct data member
               break;
            }
         } else if (insn[search].id == MIPS_INS_JR &&
               insn[search].operands[0].reg == MIPS_REG_RA) {
            // stop looking when previous `jr ra` is hit
            break;
         }
      }
   }
}

// disassemble a block of code and collect JALs and local labels
static void disassemble_block(unsigned char *data, unsigned int length, unsigned int vaddr, disasm_state *state, int block_id)
{
   asm_block *block = &state->blocks[block_id];

   // capstone structures require a lot of data, so only request a small block at a time and preserve the required data
   int remaining = length;
   int processed = 0;
   block->instruction_count = 0;
   block->instructions = calloc(length / 4, sizeof(*block->instructions));
   while (remaining > 0) {
      cs_insn *insn;
      int current_len = MIN(remaining, 1024);
      int count = cs_disasm(state->handle, &data[processed], current_len, vaddr + processed, 0, &insn);
      for (int i = 0; i < count; i++) {
         disasm_data *dis_insn = &block->instructions[block->instruction_count + i];
         dis_insn->id = insn[i].id;
         memcpy(dis_insn->bytes, insn[i].bytes, sizeof(dis_insn->bytes));
         strcpy(dis_insn->mnemonic, insn[i].mnemonic);
         strcpy(dis_insn->op_str,   insn[i].op_str);
         if (insn[i].detail != NULL && insn[i].detail->mips.op_count > 0) {
            dis_insn->op_count = insn[i].detail->mips.op_count;
            memcpy(dis_insn->operands, insn[i].detail->mips.operands, sizeof(dis_insn->operands));
         } else {
            dis_insn->op_count = 0;
         }
         dis_insn->is_jump = cs_insn_group(state->handle, &insn[i], MIPS_GRP_JUMP) || insn[i].id == MIPS_INS_JAL || insn[i].id == MIPS_INS_BAL;
      }
      cs_free(insn, count);
      block->instruction_count += count;
      processed += count * 4;
      remaining = length - processed;
   }

   if (block->instruction_count > 0) {
      disasm_data *insn = block->instructions;
      for (int i = 0; i < block->instruction_count; i++) {
         insn[i].linked_insn = -1;
         if (insn[i].is_jump) {
            // flag for newline two instructions after `jr ra` or `j`
            if ( ((insn[i].id == MIPS_INS_JR || insn[i].id == MIPS_INS_JALR) && insn[i].operands[0].reg == MIPS_REG_RA) ||
                   insn[i].id == MIPS_INS_J) {
               if (i + 2 < block->instruction_count) {
                   insn[i + 2].newline = 1;
               }
            }

            if (insn[i].id == MIPS_INS_JAL || insn[i].id == MIPS_INS_BAL || insn[i].id == MIPS_INS_J) {
               unsigned int jal_target  = (unsigned int)insn[i].operands[0].imm;
               // create label if one does not exist
               if (labels_find(&state->globals, jal_target) < 0) {
                  char label_name[32];
                  sprintf(label_name, "func_%08X", jal_target);
                  labels_add(&state->globals, label_name, jal_target);
               }
            } else {
               // all branches and jumps
               for (int o = 0; o < insn[i].op_count; o++) {
                  if (insn[i].operands[o].type == MIPS_OP_IMM) {
                     char label_name[32];
                     unsigned int branch_target = (unsigned int)insn[i].operands[o].imm;
                     // create label if one does not exist
                     int label = labels_find(&block->locals, branch_target);
                     if (label < 0) {
                        switch (state->syntax) {
                           case ASM_GAS:    sprintf(label_name, ".L%08X", branch_target); break;
                           case ASM_ARMIPS: sprintf(label_name, "@L%08X", branch_target); break;
                        }
                        labels_add(&block->locals, label_name, branch_target);
                     }
                  }
               }
            }
         }

         if (state->merge_pseudo) {
            switch (insn[i].id) {
               // find floating point LI
               case MIPS_INS_MTC1:
               {
                  unsigned int rt = insn[i].operands[0].reg;
                  for (int s = i - 1; s >= 0; s--) {
                     if (insn[s].id == MIPS_INS_LUI && insn[i].operands[0].reg == rt) {
                        float f;
                        uint32_t lui_imm = (uint32_t)insn[i].operands[1].imm;
                        lui_imm <<= 16;
                        memcpy(&f, &lui_imm, sizeof(f));
                        // link up the LUI with this instruction and the float
                        insn[s].linked_insn = i;
                        insn[s].linked_float = f;
                        // rewrite LUI instruction to be LI
                        insn[s].id = MIPS_INS_LI;
                        strcpy(insn[s].mnemonic, "li");
                        break;
                     } else if (insn[s].id == MIPS_INS_LW ||
                                insn[s].id == MIPS_INS_LD ||
                                insn[s].id == MIPS_INS_LH ||
                                insn[s].id == MIPS_INS_LHU ||
                                insn[s].id == MIPS_INS_LB ||
                                insn[s].id == MIPS_INS_LBU ||
                                insn[s].id == MIPS_INS_ADDIU ||
                                insn[s].id == MIPS_INS_ADD ||
                                insn[s].id == MIPS_INS_SUB ||
                                insn[s].id == MIPS_INS_SUBU) {
                        unsigned int rd = insn[s].operands[0].reg;
                        if (rt == rd) {
                           break;
                        }
                     } else if (insn[s].id == MIPS_INS_JR &&
                                insn[s].operands[0].reg == MIPS_REG_RA) {
                        // stop looking when previous `jr ra` is hit
                        break;
                     }
                  }
                  break;
               }
               case MIPS_INS_SD:
               case MIPS_INS_SW:
               case MIPS_INS_SH:
               case MIPS_INS_SB:
               case MIPS_INS_LB:
               case MIPS_INS_LBU:
               case MIPS_INS_LD:
               case MIPS_INS_LDL:
               case MIPS_INS_LDR:
               case MIPS_INS_LH:
               case MIPS_INS_LHU:
               case MIPS_INS_LW:
               case MIPS_INS_LWU:
               {
                  unsigned int mem_rs = insn[i].operands[1].mem.base;
                  unsigned int mem_imm = (unsigned int)insn[i].operands[1].mem.disp;
                  link_with_lui(state, block_id, i, mem_rs, mem_imm);
                  break;
               }
               case MIPS_INS_ADDIU:
               case MIPS_INS_ORI:
               {
                  unsigned int rd = insn[i].operands[0].reg;
                  unsigned int rs = insn[i].operands[1].reg;
                  int64_t imm = insn[i].operands[2].imm;
                  if (rs == MIPS_REG_ZERO) { // becomes LI
                     insn[i].id = MIPS_INS_LI;
                     strcpy(insn[i].mnemonic, "li");
                     // TODO: is there allocation for this?
                     sprintf(insn[i].op_str, "$%s, %ld", cs_reg_name(state->handle, rd), imm);
                  } else if (rd == rs) { // only look for LUI if rd and rs are the same
                     link_with_lui(state, block_id, i, rs, (unsigned int)imm);
                  }
                  break;
               }
            }
         }
      }
   } else {
      ERROR("Error: Failed to disassemble 0x%X bytes of code at 0x%08X\n", (unsigned int)length, vaddr);
   }
}

disasm_state *disasm_state_init(asm_syntax syntax, int merge_pseudo)
{
   disasm_state *state = malloc(sizeof(*state));
   labels_alloc(&state->globals);

   state->block_count = 0;
   state->block_alloc = 128;
   state->blocks = malloc(sizeof(*state->blocks) * state->block_alloc);

   state->syntax = syntax;
   state->merge_pseudo = merge_pseudo;

   // open capstone disassembler
   if (cs_open(CS_ARCH_MIPS, CS_MODE_MIPS64 + CS_MODE_BIG_ENDIAN, &state->handle) != CS_ERR_OK) {
      ERROR("Error initializing disassembler\n");
      exit(EXIT_FAILURE);
   }
   cs_option(state->handle, CS_OPT_DETAIL, CS_OPT_ON);
   cs_option(state->handle, CS_OPT_SKIPDATA, CS_OPT_ON);

   return state;
}

void disasm_state_free(disasm_state *state)
{
   if (state) {
      for (int i = 0; i < state->block_count; i++) {
         if (state->blocks[i].instructions) {
            free(state->blocks[i].instructions);
            state->blocks[i].instructions = NULL;
         }
      }
      if (state->blocks) {
         free(state->blocks);
         state->blocks = NULL;
      }
      cs_close(&state->handle);
   }
}

void disasm_label_add(disasm_state *state, const char *name, unsigned int vaddr)
{
   labels_add(&state->globals, name, vaddr);
}

int disasm_label_lookup(const disasm_state *state, unsigned int vaddr, char *name)
{
   int found = 0;
   int id = labels_find(&state->globals, vaddr);
   if (id >= 0) {
      strcpy(name, state->globals.labels[id].name);
      found = 1;
   }
   sprintf(name, "0x%08X", vaddr);
   return found;
}

void mipsdisasm_pass1(unsigned char *data, unsigned int offset, unsigned int length, unsigned int vaddr, disasm_state *state)
{
   if (state->block_count >= state->block_alloc) {
      state->block_alloc *= 2;
      state->blocks = realloc(state->blocks, sizeof(*state->blocks) * state->block_alloc);
   }
   asm_block *block = &state->blocks[state->block_count];
   labels_alloc(&block->locals);
   block->offset = offset;
   block->length = length;
   block->vaddr = vaddr;

   // collect all branch and jump targets
   disassemble_block(&data[offset], length, vaddr, state, state->block_count);

   // sort global and local labels
   labels_sort(&state->globals);
   labels_sort(&state->blocks[state->block_count].locals);
   state->block_count++;
}

void mipsdisasm_pass2(FILE *out, disasm_state *state, unsigned int offset)
{
   asm_block *block = NULL;
   unsigned int vaddr;
   int local_idx = 0;
   int global_idx = 0;
   int label;
   int indent = 0;
   // lookup block by offset
   for (int i = 0; i < state->block_count; i++) {
      if (state->blocks[i].offset == offset) {
         block = &state->blocks[i];
         break;
      }
   }
   if (!block) {
      ERROR("Could not find block offset 0x%X\n", offset);
      exit(1);
   }
   vaddr = block->vaddr;
   // skip labels before this section
   while ( (global_idx < state->globals.count) && (vaddr > state->globals.labels[global_idx].vaddr) ) {
      global_idx++;
   }
   while ( (local_idx < block->locals.count) && (vaddr > block->locals.labels[local_idx].vaddr) ) {
      local_idx++;
   }
   for (int i = 0; i < block->instruction_count; i++) {
      disasm_data *insn = &block->instructions[i];
      // newline between functions
      if (insn->newline) {
         fprintf(out, "\n");
      }
      // insert all global labels at this address
      while ( (global_idx < state->globals.count) && (vaddr == state->globals.labels[global_idx].vaddr) ) {
         fprintf(out, "%s:\n", state->globals.labels[global_idx].name);
         global_idx++;
      }
      // insert all local labels at this address
      while ( (local_idx < block->locals.count) && (vaddr == block->locals.labels[local_idx].vaddr) ) {
         fprintf(out, "%s:\n", block->locals.labels[local_idx].name);
         local_idx++;
      }
      fprintf(out, "/* %06X %08X %02X%02X%02X%02X */  ", offset, vaddr, insn->bytes[0], insn->bytes[1], insn->bytes[2], insn->bytes[3]);
      // indent the lines after a jump or branch
      if (indent) {
         indent = 0;
         fputc(' ', out);
      }
      if (insn->is_jump) {
         indent = 1;
         fprintf(out, "%-5s ", insn->mnemonic);
         if (insn->id == MIPS_INS_JAL || insn->id == MIPS_INS_BAL || insn->id == MIPS_INS_J) {
            unsigned int jal_target = (unsigned int)insn->operands[0].imm;
            label = labels_find(&state->globals, jal_target);
            if (label >= 0) {
               fprintf(out, "%s\n", state->globals.labels[label].name);
            } else {
               fprintf(out, "0x%08X\n", jal_target);
            }
         } else {
            for (int o = 0; o < insn->op_count; o++) {
               if (o > 0) {
                  fprintf(out, ", ");
               }
               switch (insn->operands[o].type) {
                  case MIPS_OP_REG:
                     fprintf(out, "$%s", cs_reg_name(state->handle, insn->operands[o].reg));
                     break;
                  case MIPS_OP_IMM:
                  {
                     unsigned int branch_target = (unsigned int)insn->operands[o].imm;
                     label = labels_find(&block->locals, branch_target);
                     if (label >= 0) {
                        fprintf(out, block->locals.labels[label].name);
                     } else {
                        fprintf(out, "0x%08X", branch_target);
                     }
                     break;
                  }
                  default:
                     break;
               }
            }
            fprintf(out, "\n");
         }
      } else if (insn->id == MIPS_INS_MTC0 || insn->id == MIPS_INS_MFC0) {
         // workaround bug in capstone/LLVM
         unsigned char rd;
         // 31-24 23-16 15-8 7-0
         // 0     1     2    3
         //       31-26  25-21 20-16 15-11 10-0
         // mfc0: 010000 00000   rt    rd  00000000000
         // mtc0: 010000 00100   rt    rd  00000000000
         //       010000 00100 00000 11101 000 0000 0000
         // rt = insn->bytes[1] & 0x1F;
         rd = (insn->bytes[2] & 0xF8) >> 3;
         fprintf(out, "%-5s $%s, $%d\n", insn->mnemonic,
                 cs_reg_name(state->handle, insn->operands[0].reg), rd);
      } else {
         int linked_insn = insn->linked_insn;
         if (linked_insn >= 0) {
            if (insn->id == MIPS_INS_LI) {
               // assume this is LUI converted to LI for matched MTC1
               fprintf(out, "%-5s ", insn->mnemonic);
               switch (state->syntax) {
                  case ASM_GAS:
                     fprintf(out, "$%s, 0x%04X0000 # %f\n",
                           cs_reg_name(state->handle, insn->operands[0].reg),
                           (unsigned int)insn->operands[1].imm,
                           insn->linked_float);
                     break;
                  case ASM_ARMIPS:
                     fprintf(out, "$%s, 0x%04X0000 // %f\n",
                           cs_reg_name(state->handle, insn->operands[0].reg),
                           (unsigned int)insn->operands[1].imm,
                           insn->linked_float);
                     break;
                  // TODO: this is ideal, but it doesn't work exactly for all floats since some emit imprecise float strings
                  /*
                     fprintf(out, "$%s, %f // 0x%04X\n",
                           cs_reg_name(state->handle, insn->operands[0].reg),
                           insn->linked_float,
                           (unsigned int)insn->operands[1].imm);
                     break;
                   */
               }
            } else if (insn->id == MIPS_INS_LUI) {
               label = labels_find(&state->globals, insn->linked_value);
               // assume matched LUI with ADDIU/LW/SW etc.
               switch (state->syntax) {
                  case ASM_GAS:
                     switch (block->instructions[linked_insn].id) {
                        case MIPS_INS_ADDIU:
                           fprintf(out, "%-5s $%s, %%hi(%s) # %s\n", insn->mnemonic,
                                 cs_reg_name(state->handle, insn->operands[0].reg),
                                 state->globals.labels[label].name, insn->op_str);
                           break;
                        case MIPS_INS_ORI:
                           fprintf(out, "%-5s $%s, (0x%08X >> 16) # %s %s\n", insn->mnemonic,
                                 cs_reg_name(state->handle, insn->operands[0].reg),
                                 insn->linked_value, insn->mnemonic, insn->op_str);
                           break;
                        default: // LW/SW/etc.
                           fprintf(out, "%-5s $%s, %%hi(%s) # %s\n", insn->mnemonic,
                                 cs_reg_name(state->handle, insn->operands[0].reg),
                                 state->globals.labels[label].name, insn->op_str);
                           break;
                     }
                     break;
                  case ASM_ARMIPS:
                     switch (block->instructions[linked_insn].id) {
                        case MIPS_INS_ADDIU:
                           fprintf(out, "%-5s $%s, %s // %s %s\n", "la.u",
                                 cs_reg_name(state->handle, insn->operands[0].reg),
                                 state->globals.labels[label].name,
                                 insn->mnemonic, insn->op_str);
                           break;
                        case MIPS_INS_ORI:
                           fprintf(out, "%-5s $%s, 0x%08X // %s %s\n", "li.u",
                                 cs_reg_name(state->handle, insn->operands[0].reg),
                                 insn->linked_value, insn->mnemonic, insn->op_str);
                           break;
                        default: // LW/SW/etc.
                           fprintf(out, "%-5s $%s, hi(%s) // %s\n", insn->mnemonic,
                                 cs_reg_name(state->handle, insn->operands[0].reg),
                                 state->globals.labels[label].name, insn->op_str);
                           break;
                     }
                     break;
               }
            } else if (insn->id == MIPS_INS_ADDIU) {
               label = labels_find(&state->globals, insn->linked_value);
               switch (state->syntax) {
                  case ASM_GAS:
                     fprintf(out, "%-5s $%s, %%lo(%s) # %s %s\n", insn->mnemonic,
                           cs_reg_name(state->handle, insn->operands[0].reg),
                           state->globals.labels[label].name,
                           insn->mnemonic, insn->op_str);
                     break;
                  case ASM_ARMIPS:
                     fprintf(out, "%-5s $%s, %s // %s %s\n", "la.l",
                           cs_reg_name(state->handle, insn->operands[0].reg),
                           state->globals.labels[label].name,
                           insn->mnemonic, insn->op_str);
                     break;
               }
            } else if (insn->id == MIPS_INS_ORI) {
               switch (state->syntax) {
                  case ASM_GAS:
                     fprintf(out, "%-5s $%s, (0x%08X & 0xFFFF) # %s %s\n", insn->mnemonic,
                           cs_reg_name(state->handle, insn->operands[0].reg),
                           insn->linked_value,
                           insn->mnemonic, insn->op_str);
                     break;
                  case ASM_ARMIPS:
                     fprintf(out, "%-5s $%s, 0x%08X // %s %s\n", "li.l",
                           cs_reg_name(state->handle, insn->operands[0].reg),
                           insn->linked_value,
                           insn->mnemonic, insn->op_str);
                     break;
               }
            } else {
               label = labels_find(&state->globals, insn->linked_value);
               fprintf(out, "%-5s $%s, %slo(%s)($%s)\n", insn->mnemonic,
                     cs_reg_name(state->handle, insn->operands[0].reg),
                     state->syntax == ASM_GAS ? "%" : "",
                     state->globals.labels[label].name,
                     cs_reg_name(state->handle, insn->operands[1].reg));
            }
         } else {
            fprintf(out, "%-5s %s\n", insn->mnemonic, insn->op_str);
         }
      }
      vaddr += 4;
      offset += 4;
   }
}

const char *disasm_get_version(void)
{
   static char version[32];
   int major, minor;
   (void)cs_version(&major, &minor);
   // TODO: manually keeping track of capstone revision number
   sprintf(version, "capstone %d.%d.4", major, minor);
   return version;
}

#ifdef MIPSDISASM_STANDALONE
typedef struct
{
   unsigned int start;
   unsigned int length;
   unsigned int vaddr;
} range;

typedef struct
{
   range *ranges;
   int range_count;
   unsigned int vaddr;
   char *input_file;
   char *output_file;
   int merge_pseudo;
   asm_syntax syntax;
} arg_config;

static arg_config default_args =
{
   NULL, // ranges
   0,    // range_count
   0x0,  // vaddr
   NULL, // input_file
   NULL, // output_file
   0,    // merge_pseudo
   ASM_GAS, // GNU as
};

static void print_usage(void)
{
   ERROR("Usage: mipsdisasm [-o OUTPUT] [-p] [-s ASSEMBLER] [-v] ROM [RANGES]\n"
         "\n"
         "mipsdisasm v" MIPSDISASM_VERSION ": MIPS disassembler\n"
         "\n"
         "Optional arguments:\n"
         " -o OUTPUT    output filename (default: stdout)\n"
         " -p           emit pseudoinstructions for related instructions\n"
         " -s SYNTAX    assembler syntax to use [gas, armips] (default: gas)\n"
         " -v           verbose progress output\n"
         "\n"
         "Arguments:\n"
         " FILE         input binary file to disassemble\n"
         " [RANGES]     optional list of ranges (default: entire input file)\n"
         "              format: <VAddr>:[<Start>-<End>] or <VAddr>:[<Start>+<Length>]\n"
         "              example: 0x80246000:0x1000-0x0E6258\n");
   exit(EXIT_FAILURE);
}

void range_parse(range *r, const char *arg)
{
   char *colon = strchr(arg, ':');
   r->vaddr = strtoul(arg, NULL, 0);
   if (colon) {
      char *minus = strchr(colon+1, '-');
      char *plus = strchr(colon+1, '+');
      r->start = strtoul(colon+1, NULL, 0);
      if (minus) {
         r->length = strtoul(minus+1, NULL, 0) - r->start;
      } else if (plus) {
         r->length = strtoul(plus+1, NULL, 0);
      }
   }
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], arg_config *config)
{
   int file_count = 0;
   if (argc < 2) {
      print_usage();
      exit(1);
   }
   config->ranges = malloc(argc / 2 * sizeof(*config->ranges));
   config->range_count = 0;
   for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case 'o':
               if (++i >= argc) {
                  print_usage();
               }
               config->output_file = argv[i];
               break;
            case 'p':
               config->merge_pseudo = 1;
               break;
            case 's':
            {
               if (++i >= argc) {
                  print_usage();
               }
               if ((0 == strcasecmp("gas", argv[i])) ||
                   (0 == strcasecmp("gnu", argv[i]))) {
                  config->syntax = ASM_GAS;
               } else if (0 == strcasecmp("armips", argv[i])) {
                  config->syntax = ASM_ARMIPS;
               } else {
                  print_usage();
               }
               break;
            }
            case 'v':
               g_verbosity = 1;
               break;
            default:
               print_usage();
               break;
         }
      } else {
         if (file_count == 0) {
            config->input_file = argv[i];
         } else {
            range_parse(&config->ranges[config->range_count], argv[i]);
            config->range_count++;
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
   arg_config args;
   long file_len;
   disasm_state *state;
   unsigned char *data;
   FILE *out;

   // load defaults and parse arguments
   out = stdout;
   args = default_args;
   parse_arguments(argc, argv, &args);

   // read input file
   INFO("Reading input file '%s'\n", args.input_file);
   file_len = read_file(args.input_file, &data);
   if (file_len <= 0) {
      ERROR("Error reading input file '%s'\n", args.input_file);
      return EXIT_FAILURE;
   }

   // if specified, open output file
   if (args.output_file != NULL) {
      INFO("Opening output file '%s'\n", args.output_file);
      out = fopen(args.output_file, "w");
      if (out == NULL) {
         ERROR("Error opening output file '%s'\n", args.output_file);
         return EXIT_FAILURE;
      }
   }

   // if no ranges specified or if only vaddr specified, add one of entire input file
   if (args.range_count < 1 || (args.range_count == 1 && args.ranges[0].length == 0)) {
      if (args.range_count < 1) {
         args.ranges[0].vaddr = 0;
      }
      args.ranges[0].start = 0;
      args.ranges[0].length = file_len;
      args.range_count = 1;
   }

   // assembler header output
   switch (args.syntax) {
      case ASM_GAS:
         fprintf(out, ".set noat      # allow manual use of $at\n");
         fprintf(out, ".set noreorder # don't insert nops after branches\n\n");
         break;
      case ASM_ARMIPS:
      {
         char output_binary[FILENAME_MAX];
         if (args.output_file == NULL) {
            strcpy(output_binary, "test.bin");
         } else {
            const char *base = basename(args.output_file);
            generate_filename(base, output_binary, "bin");
         }
         fprintf(out, ".n64\n");
         fprintf(out, ".create \"%s\", 0x%08X\n\n", output_binary, 0);
         break;
      }
      default:
         break;
   }

   state = disasm_state_init(args.syntax, args.merge_pseudo);

   // run first pass disassembler on each section
   for (int i = 0; i < args.range_count; i++) {
      range *r = &args.ranges[i];
      INFO("Disassembling range 0x%X-0x%X at 0x%08X\n", r->start, r->start + r->length, r->vaddr);

      (void)mipsdisasm_pass1(data, r->start, r->length, r->vaddr, state);
   }

   // output global labels not in asm sections
   if (args.syntax == ASM_ARMIPS) {
      for (int i = 0; i < state->globals.count; i++) {
         unsigned int vaddr = state->globals.labels[i].vaddr;
         int global_in_asm = 0;
         for (int j = 0; j < state->block_count; j++) {
            unsigned int block_vaddr = state->blocks[j].vaddr;
            if (vaddr >= block_vaddr && vaddr < block_vaddr + state->blocks[j].length) {
               global_in_asm = 1;
               break;
            }
         }
         if (!global_in_asm) {
            fprintf(out, ".definelabel %s, 0x%08X\n", state->globals.labels[i].name, vaddr);
         }
      }
   }
   fprintf(out, "\n");

   // output each section
   for (int i = 0; i < args.range_count; i++) {
      range *r = &args.ranges[i];
      if (args.syntax == ASM_ARMIPS) {
         fprintf(out, ".headersize 0x%08X\n\n", r->vaddr);
      }

      // second pass, generate output
      mipsdisasm_pass2(out, state, r->start);
   }

   disasm_state_free(state);

   // assembler footer output
   switch (args.syntax) {
      case ASM_ARMIPS:
         fprintf(out, "\n.close\n");
         break;
      default:
         break;
   }

   free(data);

   return EXIT_SUCCESS;
}

#endif // MIPSDISASM_STANDALONE
