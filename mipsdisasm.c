#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <capstone/capstone.h>

#include "config.h"
#include "mipsdisasm.h"
#include "utils.h"

#define MIPSDISASM_VERSION "0.2"

#define DEFAULT_CONFIG "configs/sm64.u.config"

static int known_index(rom_config *config, unsigned int ram_addr)
{
   int i;
   for (i = 0; i < config->label_count; i++) {
      if (ram_addr == config->labels[i].ram_addr) {
         return i;
      }
   }
   return -1;
}

static int find_local(local_table *locals, unsigned int offset)
{
   int i;
   for (i = 0; i < locals->count; i++) {
      if (locals->offsets[i] == offset) {
         return i;
      }
   }
   return -1;
}

static void add_local(local_table *locals, unsigned int offset)
{
   if (find_local(locals, offset) >= 0) {
      return;
   }
   locals->offsets[locals->count] = offset;
   locals->count++;
}

static int cmp_local(const void *a, const void *b)
{
   const unsigned int *uia = a;
   const unsigned int *uib = b;
   return (*uia - *uib);
}

static void add_proc(proc_table *procs, unsigned int start, unsigned int end)
{
   int i;
   int warning = -1;
   for (i = 0; i < procs->count; i++) {
      if (procs->procedures[i].start == start) {
         return;
      } else if (start > procs->procedures[i].start && start < procs->procedures[i].end) {
         warning = i;
      }
   }
   // TODO: workaround for __osExceptionHandler (80327650) and inner asm routines
   if (warning >= 0 && procs->procedures[warning].start != 0x80327650) {
      ERROR("Warning: %X in middle of proc %X - %X\n", start, procs->procedures[warning].start, procs->procedures[warning].end);
   }
   procs->procedures[procs->count].start = start;
   procs->procedures[procs->count].end = end;
   procs->count++;
}

// collect JALs and local labels for a given procedure
static void collect_proc_jals(unsigned char *data, long datalen, proc_table *ptbl, int p, rom_config *config)
{
#define MAX_LOCALS 256
#define MAX_BYTES_PER_CALL 1024
   unsigned int local_offsets[MAX_LOCALS];
   local_table locals;
   csh handle;
   cs_insn *insn;
   unsigned int ram_address;
   unsigned int rom_offset;
   unsigned int alloc_size;
   unsigned int last_label;
   unsigned int processed;
   int remaining;
   int cur_amount;
   int count;
   int disassembling;
   procedure *proc;

   // open capstone disassembler
   if (cs_open(CS_ARCH_MIPS, CS_MODE_MIPS64 + CS_MODE_BIG_ENDIAN, &handle) != CS_ERR_OK) {
      ERROR("Error initializing disassembler\n");
      exit(EXIT_FAILURE);
   }
   cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
   cs_option(handle, CS_OPT_SKIPDATA, CS_OPT_ON);

   proc = &ptbl->procedures[p];
   ram_address = proc->start;
   rom_offset = ram_to_rom(config, ram_address);

   // find referenced JAL, local labels, and find end marked by last JR or ERET
   locals.offsets = local_offsets;
   locals.count = 0;

   // don't try to disassemble if not in ROM bounds
   if (rom_offset < datalen) {
      disassembling = 1;
   } else {
      ERROR("Error: unknown ROM mapping for proc %08X (tried %X)\n", ram_address, rom_offset);
      disassembling = 0;
   }
   processed = 0;
   remaining = 0;
   last_label = 0;
   while (disassembling) {
      cur_amount = MIN(MAX_BYTES_PER_CALL, datalen - rom_offset - processed);
      count = cs_disasm(handle, &data[rom_offset + processed], cur_amount, ram_address + processed, 0, &insn);
      if (count > 0) {
         int i, o;
         for (i = 0; i < count && disassembling; i++) {
            if (cs_insn_group(handle, &insn[i], MIPS_GRP_JUMP)) {
               // all branches and jumps
               cs_mips *mips = &insn[i].detail->mips;
               for (o = 0; o < mips->op_count; o++) {
                  if (mips->operands[o].type == MIPS_OP_IMM)
                  {
                     unsigned int sec_offset = (unsigned int)mips->operands[o].imm - ram_address;
                     add_local(&locals, sec_offset);
                     if (sec_offset > last_label) {
                        last_label = sec_offset;
                     }
                     if (locals.count > MAX_LOCALS) {
                        ERROR("Need more than %d locals for %08x\n", MAX_LOCALS, proc->start);
                     }
                  }
               }
            } else if (insn[i].id == MIPS_INS_JAL || insn[i].id == MIPS_INS_BAL) {
               unsigned int addr;
               cs_mips *mips = &insn[i].detail->mips;
               addr = (unsigned int)mips->operands[0].imm;
               add_proc(ptbl, addr, 0);
            }

            // if we encounter B, JR, or ERET past last local label, end disassembly
            if (processed >= last_label) {
               switch (insn[i].id) {
                  case MIPS_INS_B:    remaining = 2; break; // a positive branch would update last_label above
                  case MIPS_INS_JR:   remaining = 2; break;
                  case MIPS_INS_ERET: remaining = 1; break;
                  default: break;
               }
            }

            // manually set end
            if (proc->end) {
               if (proc->end == ram_address + processed + 4) {
                  disassembling = 0;
               }
            } else {
               // automatically find terminating instruction
               if (remaining > 0) {
                  remaining--;
                  if (remaining == 0) {
                     disassembling = 0;
                  }
               }
            }
            processed += 4;
         }
         cs_free(insn, count);
      } else {
         fprintf(stderr, "ERROR: Failed to disassemble given code!\n");
         disassembling = 0;
      }
   }

   // sort labels
   qsort(locals.offsets, locals.count, sizeof(locals.offsets[0]), cmp_local);
   // copy labels to procedure
   alloc_size = locals.count * sizeof(*locals.offsets);
   proc->locals.offsets = malloc(alloc_size);
   memcpy(proc->locals.offsets, locals.offsets, alloc_size);
   proc->locals.count = locals.count;
   proc->end = ram_address + processed;

   cs_close(&handle);
}

// call this function with at least one procedure in procs
static void collect_jals(unsigned char *data, long datalen, proc_table *procs, rom_config *config)
{
   int proc_idx;
   for (proc_idx = 0; proc_idx < procs->count; proc_idx++) {
      collect_proc_jals(data, datalen, procs, proc_idx, config);
   }
}

static int proc_cmp(const void *a, const void *b)
{
   const procedure *proca = a;
   const procedure *procb = b;
   return (proca->start - procb->start);
}

// interpret MIPS pseudoinstructions
// returns number of instructions consumed
// TODO: memory operations
// TODO: pseudo-instruction detection: li, la (more cases), bgt, blt
static int pseudoins_detected(FILE *out, csh handle, cs_insn *insn, int count, rom_config *config)
{
   int retVal = 0;
   int i;
   int luis;
   if (count >= 2) {
      /* lui   $a1, 0x11
       * lui   $a2, 0x11
       * addiu $a2, $a2, 0x4750
       * addiu $a1, $a1, -0x75c0
       * lui   $t6, 0xa00
       * lui   $t7, 0x4c0
       * ori   $t7, $t7, 0x280
       * ori   $t6, $t6, 0x740
       */
      // count leading LUIs
      luis = 0;
      while (luis < count/2 && insn[luis].id == MIPS_INS_LUI) {
         luis++;
      }
      // first check for LA wrapped around JAL (LUI/JAL/ADDIU)
      if (luis == 1 && count >= 3) {
         unsigned int reg = insn[0].detail->mips.operands[0].reg;
         if (insn[1].id == MIPS_INS_JAL && insn[2].id == MIPS_INS_ADDIU) {
            // ensure addiu uses same register
            if (insn[2].detail->mips.operands[0].reg == reg &&
                insn[2].detail->mips.operands[1].reg == reg) {
               char label[128];
               const char *reg_name;
               unsigned int jal_addr;
               unsigned int addr;
               unsigned int lui_imm;
               unsigned int addiu_imm;
               int known_idx;
               lui_imm = (unsigned int)insn[0].detail->mips.operands[1].imm;
               addiu_imm = (unsigned int)insn[2].detail->mips.operands[2].imm;
               jal_addr = (unsigned int)insn[1].detail->mips.operands[0].imm;
               addr = ((lui_imm << 16) + addiu_imm);
               fill_addr_label(config, addr, label, -1);
               reg_name = cs_reg_name(handle, reg);
               fprintf(out, "  lui   $%s, %%hi(%s) # 0x%X\n", reg_name, label, lui_imm);
               known_idx = known_index(config, jal_addr);
               if (known_idx < 0) {
                  fprintf(out, "  jal   proc_%08X\n", jal_addr);
               } else {
                  fprintf(out, "  jal   %s\n", config->labels[known_idx].name);
               }
               fprintf(out, "  addiu $%s, $%s, %%lo(%s) # 0x%X", reg_name, reg_name, label, addiu_imm & 0xFFFF);
               return 3; // consume LUI/JAL/ADDIU
            }
         }
      }
      if (luis > 0 && 2*luis <= count) {
         char label[128];
         unsigned int addr[16]; // some sane max
         // verify trailing ADDIUs, ORIs, or Loads
         for (i = 0; i < luis; i++) {
            int rev = 2*luis - i - 1;
            unsigned int reg = insn[i].detail->mips.operands[0].reg;
            switch(insn[rev].id) {
               case MIPS_INS_ADDIU:
               case MIPS_INS_ORI:
                  if (reg == insn[rev].detail->mips.operands[0].reg) {
                     addr[i] = (unsigned int)((insn[i].detail->mips.operands[1].imm << 16)
                                             + insn[rev].detail->mips.operands[2].imm);
                  } else {
                     return 0;
                  }
                  break;
               case MIPS_INS_LW:
               case MIPS_INS_LH:
               case MIPS_INS_LHU:
               case MIPS_INS_LB:
               case MIPS_INS_LBU:
                  if (reg == insn[rev].detail->mips.operands[0].reg &&
                      reg == insn[rev].detail->mips.operands[1].mem.base) {
                     addr[i] = (unsigned int)((insn[i].detail->mips.operands[1].imm << 16)
                                             + insn[rev].detail->mips.operands[1].mem.disp);
                  } else {
                     return 0;
                  }
                  break;
               default:
                  return 0;
            }
         }
         for (i = 0; i < luis; i++) {
            int rev = 2*luis - i - 1;
            int type;
            // TODO: this isn't very smart to guess start/end based on register
            switch (insn[i].detail->mips.operands[0].reg) {
               case MIPS_REG_A1: type = fill_addr_label(config, addr[i], label, 0); break;
               case MIPS_REG_A2: type = fill_addr_label(config, addr[i], label, 1); break;
               default:          type = fill_addr_label(config, addr[i], label, -1); break;
            }
            // looks like all the ADDIU cases are addresses and ORI are immediates
            if (type == 1) {
               fprintf(out, "  la    $%s, %s # %s %s/%s %s",
                     cs_reg_name(handle, insn[i].detail->mips.operands[0].reg),
                     label, insn[i].mnemonic, insn[i].op_str, insn[rev].mnemonic, insn[rev].op_str);
            } else if (MIPS_INS_ORI == insn[rev].id) {
               fprintf(out, "  li    $%s, %s # %s %s/%s %s",
                     cs_reg_name(handle, insn[i].detail->mips.operands[0].reg),
                     label, insn[i].mnemonic, insn[i].op_str, insn[rev].mnemonic, insn[rev].op_str);
            } else if (MIPS_INS_ADDIU == insn[rev].id) {
               fprintf(out, "  la    $%s, %s # %s %s/%s %s",
                     cs_reg_name(handle, insn[i].detail->mips.operands[0].reg),
                     label, insn[i].mnemonic, insn[i].op_str, insn[rev].mnemonic, insn[rev].op_str);
            } else {
               fprintf(out, "  %-5s $%s, 0x%x # %s %s/%s %s", insn[rev].mnemonic,
                     cs_reg_name(handle, insn[i].detail->mips.operands[0].reg),
                     addr[i], insn[i].mnemonic, insn[i].op_str, insn[rev].mnemonic, insn[rev].op_str);
            }
            if (i < (luis-1)) {
               fprintf(out, "\n");
            }
         }
         retVal = 2*luis;
      }
   }
   return retVal;
}

// TODO: the hint is generated based on register a1/a2 = begin/end
int fill_addr_label(rom_config *config, unsigned int addr, char *label, int hint)
{
   int i;
   // check for RAM addresses
   if (addr >= 0x80000000) {
      // first check RAM blocks
      for (i = 0; i < config->ram_count; i++) {
         if (config->ram_table[3*i] == addr) {
            sprintf(label, "0x%X", addr);
            return 0;
         }
      }
      // second check RAM labels
      for (i = 0; i < config->label_count; i++) {
         if (config->labels[i].ram_addr == addr) {
            sprintf(label, "%s", config->labels[i].name);
            return 0;
         }
      }
   }
   if (addr >= 0x13000000 && addr < 0x14000000) {
      split_section *sec_beh = NULL;
      for (i = 0; i < config->section_count; i++) {
         if (config->sections[i].type == TYPE_BEHAVIOR) {
            sec_beh = &config->sections[i];
            break;
         }
      }
      if (sec_beh) {
         behavior *beh = sec_beh->extra;
         unsigned int offset = addr & 0xFFFFFF;
         for (i = 0; i < sec_beh->extra_len; i++) {
            if (offset == beh[i].offset) {
               sprintf(label, "%s", beh[i].name);
               return 1;
            }
         }
      }
   }
   // check for ROM offsets
   switch (hint) {
      case 0:
         for (i = 0; i < config->section_count; i++) {
            // TODO: hack until mario_animation gets moved or AT() is used
            if (config->sections[i].start == addr && addr != 0x4EC000) {
               if (config->sections[i].label[0] != '\0') {
                  sprintf(label, "%s", config->sections[i].label);
                  return 0;
               }
            }
         }
         break;
      case 1:
         for (i = 0; i < config->section_count; i++) {
            if (config->sections[i].end == addr) {
               if (config->sections[i].label[0] != '\0') {
                  sprintf(label, "%s_end", config->sections[i].label);
                  return 0;
               }
            }
         }
         break;
      default:
         break;
   }
   sprintf(label, "0x%08X", addr);
   return 0;
}

unsigned int disassemble_proc(FILE *out, unsigned char *data, long datalen, procedure *proc, rom_config *config)
{
   char sec_name[64];
   csh handle;
   cs_insn *insn;
   unsigned int ram_address;
   unsigned int rom_offset;
   unsigned int length;
   unsigned int processed;
   unsigned int cur_amount;
   int count;
   int disassembling;
   int known_idx;

   // open capstone disassembler
   if (cs_open(CS_ARCH_MIPS, CS_MODE_MIPS64 + CS_MODE_BIG_ENDIAN, &handle) != CS_ERR_OK) {
      ERROR("Error initializing disassembler\n");
      exit(EXIT_FAILURE);
   }
   cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
   cs_option(handle, CS_OPT_SKIPDATA, CS_OPT_ON);

   ram_address = proc->start;
   rom_offset = ram_to_rom(config, ram_address);
   length = proc->end - proc->start;

   // construct procedure label
   known_idx = known_index(config, ram_address);
   if (known_idx < 0) {
      sprintf(sec_name, "proc_%08X", ram_address);
   } else {
      sprintf(sec_name, "%s", config->labels[known_idx].name);
   }

   // perform disassembly
   disassembling = 1;
   processed = 0;
   fprintf(out, "\n%s: # begin %08X (%06X)\n", sec_name, ram_address, rom_offset);
   while (disassembling) {
      int consumed;
      cur_amount = MIN(MAX_BYTES_PER_CALL, length - processed);
      cur_amount = MIN(cur_amount, datalen - rom_offset - processed);
      count = cs_disasm(handle, &data[rom_offset + processed], cur_amount, ram_address + processed, 0, &insn);
      if (count > 0) {
         int i, o;

         for (i = 0; i < count && disassembling; i++) {
            // handle redirect jump instruction immediates
            int ll;
            // TODO: workaround for __osEnqueueThread, __osPopThread, __osDispatchThread
            switch (ram_address + processed) {
               case 0x80327B98: fprintf(out, "# end __osExceptionHandler\n\n"
                                             "proc_80327B98: # begin 80327B98 (0E2B98)\n"); break;
               case 0x80327C4C: fprintf(out, "# end proc_80327B98\n\n"); break;
               case 0x80327C80: fprintf(out, "\n__osEnqueueAndYield: # begin 80327C80 (0E2C80)\n"); break;
               case 0x80327D10: fprintf(out, "# end __osEnqueueAndYield\n\n"
                                             "__osEnqueueThread: # begin 80327D10 (0E2D10)\n"); break;
               case 0x80327D58: fprintf(out, "# end __osEnqueueThread\n\n"
                                             "__osPopThread: # begin 80327D58 (0E2D58)\n"); break;
               case 0x80327D68: fprintf(out, "# end __osPopThread\n\n"
                                             "__osDispatchThread: # begin 80327D68 (0E2D68)\n"); break;
               default: break;
            }
            ll = find_local(&proc->locals, processed);
            if (ll >= 0) {
               fprintf(out, ".L%s_%X: # %X\n", sec_name, processed, ram_address + processed);
            }
            consumed = pseudoins_detected(out, handle, &insn[i], count-1, config);
            if (consumed > 0) {
               i += (consumed-1);
               processed += (consumed-1) * 4;
            } else if (insn[i].id == MIPS_INS_INVALID) {
               unsigned char *in = &data[rom_offset+processed];
               fprintf(out, ".byte 0x%02X, 0x%02X, 0x%02X, 0x%02X # Invalid: %X",
                     in[0], in[1], in[2], in[3], ram_address + processed);
            } else {
               fprintf(out, "  %-5s ", insn[i].mnemonic);
               if (cs_insn_group(handle, &insn[i], MIPS_GRP_JUMP)) {
                  cs_mips *mips = &insn[i].detail->mips;
                  for (o = 0; o < mips->op_count; o++) {
                     if (o > 0) {
                        fprintf(out, ", ");
                     }
                     switch (mips->operands[o].type) {
                        case MIPS_OP_REG:
                           fprintf(out, "$%s", cs_reg_name(handle, mips->operands[o].reg));
                           break;
                        case MIPS_OP_IMM:
                        {
                           unsigned int sec_offset = (unsigned int)mips->operands[o].imm - ram_address;
                           fprintf(out, ".L%s_%X", sec_name, sec_offset);
                           break;
                        }
                        default:
                           break;
                     }
                  }
               } else if (insn[i].id == MIPS_INS_JAL || insn[i].id == MIPS_INS_BAL) {
                  unsigned int addr;
                  cs_mips *mips = &insn[i].detail->mips;
                  addr = (unsigned int)mips->operands[0].imm;
                  known_idx = known_index(config, addr);
                  if (known_idx < 0) {
                     fprintf(out, "proc_%08X", addr);
                  } else {
                     fprintf(out, "%s", config->labels[known_idx].name);
                  }
               } else if (insn[i].id == MIPS_INS_MTC0 || insn[i].id == MIPS_INS_MFC0) {
                  // workaround bug in capstone/LLVM
                  unsigned char *in = &data[rom_offset+processed];
                  unsigned char rd;
                  cs_mips *mips = &insn[i].detail->mips;
                  // 31-24 23-16 15-8 7-0
                  // 0     1     2    3
                  //       31-26  25-21 20-16 15-11 10-0
                  // mfc0: 010000 00000   rt    rd  00000000000
                  // mtc0: 010000 00100   rt    rd  00000000000
                  // rt = in[1] & 0x1F;
                  rd = (in[2] & 0xF8) >> 3;
                  fprintf(out, "$%s, $%d # ", cs_reg_name(handle, mips->operands[0].reg), rd);
                  fprintf(out, "%02X%02X%02X%02X", in[0], in[1], in[2], in[3]);
                  fprintf(out, " %s", insn[i].op_str);
               } else if (insn[i].id == MIPS_INS_SW || insn[i].id == MIPS_INS_SH ||
                          insn[i].id == MIPS_INS_SB || insn[i].id == MIPS_INS_SWC1) {
                  unsigned int reg = insn[i].detail->mips.operands[1].mem.base;
                  unsigned int sw_imm = (unsigned int)insn[i].detail->mips.operands[1].mem.disp;
                  int lui;
                  consumed = 0;
                  // don't attempt to compute addresses for zero offset
                  if (sw_imm != 0x0) {
                     for (lui = i - 1; lui >= 0; lui--) {
                        if (insn[lui].id == MIPS_INS_LUI) {
                           unsigned int lui_reg = insn[lui].detail->mips.operands[0].reg;
                           if (reg == lui_reg) {
                              char label[256];
                              unsigned int lui_imm = (unsigned int)insn[lui].detail->mips.operands[1].imm;
                              unsigned int addr = ((lui_imm << 16) + sw_imm);
                              fill_addr_label(config, addr, label, -1);
                              fprintf(out, "$%s, %%lo(%s)($%s) # ",
                                    cs_reg_name(handle, insn[i].detail->mips.operands[0].reg),
                                    label, cs_reg_name(handle, reg));
                              consumed = 1;
                              break;
                           }
                        } else if (insn[lui].id == MIPS_INS_LW) {
                           unsigned int lw_reg = insn[lui].detail->mips.operands[0].reg;
                           if (reg == lw_reg) {
                              break;
                           }
                        }
                     }
                  }
                  fprintf(out, "%s", insn[i].op_str);
               } else {
                  fprintf(out, "%s", insn[i].op_str);
               }
            }
            fprintf(out, "\n");
            processed += 4;
         }
         cs_free(insn, count);
      } else {
         fprintf(stderr, "ERROR: Failed to disassemble given code!\n");
         disassembling = 0;
      }

      if (processed >= length) {
         disassembling = 0;
      }
   }

   fprintf(out, "# end %s\n", sec_name);

   cs_close(&handle);

   return ram_address + processed;
}

unsigned int ram_to_rom(rom_config *config, unsigned int ram_addr)
{
   int i;
   for (i = 0; i < config->ram_count; i++) {
      if (ram_addr >= config->ram_table[3*i] && ram_addr <= config->ram_table[3*i+1]) {
         return ram_addr - config->ram_table[3*i+2];
      }
   }
   return ram_addr;
}

unsigned int rom_to_ram(rom_config *config, unsigned int rom_addr)
{
   int i;
   for (i = 0; i < config->ram_count; i++) {
      unsigned int offset = config->ram_table[3*i+2];
      unsigned int start = config->ram_table[3*i] - offset;
      unsigned int end   = config->ram_table[3*i+1] - offset;
      if (rom_addr >= start && rom_addr <= end) {
         return rom_addr + offset;
      }
   }
   return rom_addr;
}

void mipsdisasm_add_procs(proc_table *procs, rom_config *config, long file_len)
{
   unsigned int ram_address;
   unsigned int rom_offset;
   unsigned int end_address;
   int i;
   for (i = 0; i < config->label_count; i++) {
      ram_address = config->labels[i].ram_addr;
      end_address = config->labels[i].end_addr;
      rom_offset = ram_to_rom(config, ram_address);
      if (rom_offset < file_len) {
         add_proc(procs, ram_address, end_address);
      }
   }
}

void mipsdisasm_pass1(unsigned char *data, long datalen, proc_table *procs, rom_config *config)
{
   // collect all JALs
   collect_jals(data, datalen, procs, config);

   // sort procedures
   qsort(procs->procedures, procs->count, sizeof(procs->procedures[0]), proc_cmp);
}

void mipsdisasm_pass2(FILE *out, unsigned char *data, long datalen, proc_table *procs, rom_config *config)
{
   // disassemble all the procedures
   unsigned int ram_address;
   unsigned int rom_offset;
   unsigned int last_end = 0xFFFFFFFF;
   int proc_idx = 0;
   while (proc_idx < procs->count) {
      ram_address = procs->procedures[proc_idx].start;
      rom_offset = ram_to_rom(config, ram_address);
      assert(rom_offset < datalen);
      if (ram_address > last_end) {
         fprintf(out, "\n# missing section %X-%X (%06X-%06X) [%X]\n",
               last_end, ram_address, ram_to_rom(config, last_end), ram_to_rom(config, ram_address), ram_address - last_end);
      }
      disassemble_proc(out, data, datalen, &procs->procedures[proc_idx], config);
      last_end = procs->procedures[proc_idx].end;
      proc_idx++;
   }
}

const char *disasm_get_version(void)
{
   static char version[16];
   int major, minor;
   (void)cs_version(&major, &minor);
   sprintf(version, "%d.%d", major, minor);
   return version;
}

#ifdef MIPSDISASM_STANDALONE
typedef struct
{
   unsigned int *offsets;
   int offset_count;
   char *input_file;
   char *output_file;
   char *config_file;
} arg_config;

static arg_config default_args =
{
   NULL,
   0,
   NULL,
   NULL,
   DEFAULT_CONFIG
};

static void generate_report(proc_table *procs, rom_config *config)
{
   unsigned int cur_start;
   unsigned int last_end;
   int i;
   cur_start = procs->procedures[0].start;
   last_end = procs->procedures[0].end;
   for (i = 1; i < procs->count; i++) {
      if (procs->procedures[i].start > last_end + 0x1000) {
         INFO("   (0x%06X, 0x%06X, \"asm\"), // %08X - %08X\n",
               ram_to_rom(config, cur_start), ram_to_rom(config, last_end), cur_start, last_end);
         cur_start = procs->procedures[i].start;
      }
      last_end = procs->procedures[i].end;
   }
   INFO("   (0x%06X, 0x%06X, \"asm\"), // %08X - %08X\n",
         ram_to_rom(config, cur_start), ram_to_rom(config, last_end), cur_start, last_end);
}

static void print_usage(void)
{
   ERROR("Usage: mipsdisasm [-c CONFIG] [-o OUTPUT] [-v] ROM [ADDRESSES]\n"
         "\n"
         "mipsdisasm v" MIPSDISASM_VERSION ": Recursive MIPS disassembler\n"
         "\n"
         "Optional arguments:\n"
         " -c CONFIG    ROM configuration file (default: %s)\n"
         " -o OUTPUT    output filename (default: stdout)\n"
         " -v           verbose progress output\n"
         "\n"
         "Arguments:\n"
         " FILE         input binary file to disassemble\n"
         " [ADDRESSES]  optional list of RAM or ROM addresses (default: labels from config file)\n",
         default_args.config_file);
   exit(EXIT_FAILURE);
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], arg_config *config)
{
   int i;
   int file_count = 0;
   if (argc < 2) {
      print_usage();
      exit(1);
   }
   config->offsets = malloc(argc * sizeof(*config->offsets));
   config->offset_count = 0;
   for (i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case 'c':
               if (++i >= argc) {
                  print_usage();
               }
               config->config_file = argv[i];
               break;
            case 'o':
               if (++i >= argc) {
                  print_usage();
               }
               config->output_file = argv[i];
               break;
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
            config->offsets[config->offset_count] = strtoul(argv[i], NULL, 0);
            config->offset_count++;
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
   rom_config config;
   arg_config args;
   long file_len;
   unsigned char *data;
   unsigned int ram_address;
   unsigned int rom_offset;
   proc_table procs;
   FILE *out;
   int i;

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

   // parse config file
   INFO("Parsing config file '%s'\n", args.config_file);
   if (parse_config_file(args.config_file, &config)) {
      ERROR("Error parsing config file '%s'\n", args.config_file);
      return EXIT_FAILURE;
   }

   // add procedures from either command line offsets or from configuration labels
   memset(&procs, 0, sizeof(procs));
   procs.count = 0;
   if (args.offset_count > 0) {
      for (i = 0; i < args.offset_count; i++) {
         if (args.offsets[i] >= 0x80000000) {
            INFO("Adding RAM address 0x%X\n", args.offsets[i]);
            ram_address = args.offsets[i];
            rom_offset = ram_to_rom(&config, ram_address);
            if (ram_address == rom_offset) {
               ERROR("Warning: offset %08X not in RAM range\n", args.offsets[i]);
               return EXIT_FAILURE;
            }
         } else {
            INFO("Adding ROM address 0x%X\n", args.offsets[i]);
            rom_offset = args.offsets[i];
            ram_address = rom_to_ram(&config, rom_offset);
            if (rom_offset == ram_address) {
               ERROR("Warning: offset %08X not in ROM range\n", args.offsets[i]);
               return EXIT_FAILURE;
            }
         }
         add_proc(&procs, ram_address, 0);
      }
   } else {
      // populate procedure list from list of known addresses
      mipsdisasm_add_procs(&procs, &config, file_len);
   }

   // first pass, collect JALs, local labels, and find procedure ends
   mipsdisasm_pass1(data, file_len, &procs, &config);

   fprintf(out, ".set noat      # allow manual use of $at\n");
   fprintf(out, ".set noreorder # don't insert nops after branches\n\n");

   // second pass, disassemble all procedures
   mipsdisasm_pass2(out, data, file_len, &procs, &config);

   generate_report(&procs, &config);

   free(data);

   return EXIT_SUCCESS;
}

#endif // MIPSDISASM_STANDALONE
