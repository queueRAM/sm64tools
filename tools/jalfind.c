#include <stdio.h>
#include <stdlib.h>

#define JALFIND_VERSION "0.1"

#define read_u32_be(buf) (unsigned int)(((buf)[0] << 24) + ((buf)[1] << 16) + ((buf)[2] << 8) + ((buf)[3]))

static void print_usage(void)
{
   fprintf(stderr,
         "jalfind <FILE> <ADDRESS> [ADDRESS...]\n"
         "\n"
         "jalfind v" JALFIND_VERSION ": search for MIPS JAL, LUI/ADDIU, and direct references in a file\n"
         "\n"
         "Arguments:\n"
         " FILE       input ROM file\n"
         " ADDRESS    address to find references to (assumes hex)\n");
}

int read_file(const char *file_name, unsigned char **data)
{
   FILE *in;
   unsigned char *in_buf = NULL;
   size_t file_size;
   size_t bytes_read;

   in = fopen(file_name, "rb");
   if (in == NULL) {
      return -1;
   }

   // determine file size
   fseek(in, 0, SEEK_END);
   file_size = ftell(in);
   fseek(in, 0, SEEK_SET);
   // allocate buffer and read entire file in
   in_buf = malloc(file_size);
   bytes_read = fread(in_buf, 1, file_size, in);
   if (bytes_read != file_size) {
      return -2;
   }

   fclose(in);
   *data = in_buf;
   return (int)bytes_read;
}

#define OPCODE_MASK  0xFC000000
#define OPCODE_ADDIU 0x24000000
#define OPCODE_JAL   0x0C000000
#define OPCODE_LUI   0x3C000000
#define OPCODE_ORI   0x34000000
#define IMM_MASK     0x0000FFFF
#define RS_MASK      0x03E00000
#define RT_MASK      0x001F0000

static const char * const regs[] =
{
   "zero",
   "at",
   "v0",
   "v1",
   "a0",
   "a1",
   "a2",
   "a3",
   "t0",
   "t1",
   "t2",
   "t3",
   "t4",
   "t5",
   "t6",
   "t7",
   "s0",
   "s1",
   "s2",
   "s3",
   "s4",
   "s5",
   "s6",
   "s7",
   "t8",
   "t9",
   "k0",
   "k1",
   "gp",
   "sp",
   "s8",
   "ra",
};

void find_reference(unsigned char *data, int len, unsigned int addr)
{
   unsigned int jal = OPCODE_JAL | ((0x0FFFFFFF & addr) >> 2);
   unsigned int lui_imm = addr >> 16;
   unsigned int addiu_imm = addr & 0xFFFF;
   int i, j;

   if (addr & 0x8000) {
      lui_imm++;
   }

   printf("--> Looking for %08X/%08X\n", addr, jal);
   for (i = 0; i < len; i += 4) {
      unsigned int ival = read_u32_be(&data[i]);

      // look for direct address
      if (addr == ival) {
         printf("%06X: %08X\n", i, ival);

      // find direct JAL
      } else if (jal == ival) {
         printf("%06X: %08X JAL 0x%08X\n", i, ival, addr);

      // look for LUI/ADDIU pairs that match the address
      } else if (((ival & OPCODE_MASK) == OPCODE_LUI) && ((ival & IMM_MASK) == lui_imm)) {
         unsigned int lui_rt = (ival & RT_MASK) >> 16;
         // look up to 32 instructions
         for (j = i + 4; j < len && j < i + 32*4; j += 4) {
            unsigned int jval = read_u32_be(&data[j]);
            if ((jval & OPCODE_MASK) == OPCODE_ADDIU) {
               unsigned int addiu_rs = (jval & RS_MASK) >> 21;
               unsigned int addiu_rt = (jval & RT_MASK) >> 16;
               if (addiu_rs == lui_rt) {
                  if ((jval & IMM_MASK) == addiu_imm) {
                     printf("%06X: %08X LUI   %s, 0x%04X     // %%hi(0x%08X)\n"
                            "%06X: %08X ADDIU %s, %s, 0x%04X // %%lo(0x%08X)\n",
                            i, ival, regs[lui_rt], lui_imm, addr,
                            j, jval, regs[addiu_rt], regs[addiu_rs], addiu_imm, addr);
                  }
                  break; // stop looking for a matching ADDIU
               }
            }
         }
      }
   }
}

int main(int argc, char *argv[])
{
   unsigned char *data;
   char *fname;
   unsigned int addr;
   int len;
   int i;

   if (argc < 3) {
      print_usage();
      return 1;
   }

   fname = argv[1];
   len = read_file(fname, &data);
   if (len > 0) {
      for (i = 2; i < argc; i++) {
         addr = strtoul(argv[i], NULL, 16);
         find_reference(data, len, addr);
      }
   } else {
      fprintf(stderr, "Error opening/reading \"%s\"\n", fname);
      return 1;
   }

   return 0;
}
