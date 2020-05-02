#include "n64split.h"

void write_behavior(FILE *out, unsigned char *data, rom_config *config, int s, disasm_state *state)
{
   char label[128];
   unsigned int a, i;
   unsigned int len;
   unsigned int val;
   int beh_i;
   split_section *sec;
   split_section *beh;
   sec = &config->sections[s];
   beh = sec->children;
   a = sec->start;
   beh_i = 0;
   while (a < sec->end) {
      if (beh_i < sec->child_count) {
         unsigned int offset = a - sec->start;
         if (offset == beh[beh_i].start) {
            fprintf(out, "%s: # %04X\n", beh[beh_i].label, beh[beh_i].start);
            beh_i++;
         } else if (offset > beh[beh_i].start) {
            ERROR("Warning: skipped behavior %04X \"%s\"\n", beh[beh_i].start, beh[beh_i].label);
            beh_i++;
         }
      }
      switch (data[a]) {
         case 0x02:
         case 0x04:
         case 0x0C:
         case 0x13:
         case 0x14:
         case 0x15:
         case 0x16:
         case 0x17:
         case 0x23:
         case 0x27:
         case 0x2A:
         case 0x2E:
         case 0x2F:
         case 0x31:
         case 0x33:
         case 0x36:
         case 0x37:
            len = 8;
            break;
         case 0x1C:
         case 0x29:
         case 0x2B:
         case 0x2C:
            len = 12;
            break;
         case 0x30:
            len = 20;
            break;
         default:
            len = 4;
            break;
      }
      val = read_u32_be(&data[a]);
      fprintf(out, ".word 0x%08X", val);
      switch(data[a]) {
         case 0x0C: // behavior 0x0C is a function pointer
            val = read_u32_be(&data[a+4]);
            disasm_label_lookup(state, val, label);
            fprintf(out, ", %s\n", label);
            break;
         case 0x02: // jump to another behavior
         case 0x04: // jump to segmented address
         case 0x1C: // sub-objects
         case 0x29: // sub-objects
         case 0x2C: // sub-objects
            for (i = 4; i < len-4; i += 4) {
               val = read_u32_be(&data[a+i]);
               fprintf(out, ", 0x%08X", val);
            }
            val = read_u32_be(&data[a+len-4]);
            disasm_label_lookup(state, val, label);
            fprintf(out, ", %s\n", label);
            break;
         default:
            for (i = 4; i < len; i += 4) {
               val = read_u32_be(&data[a+i]);
               fprintf(out, ", 0x%08X", val);
            }
            fprintf(out, "\n");
            break;
      }
      a += len;
   }
}