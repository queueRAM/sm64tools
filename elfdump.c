#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mipsdisasm.h"
#include "utils.h"
#include "elf.h"

#define ELFDUMP_VERSION "0.1"

#define u32be(x) (uint32_t)(((x & 0xff) << 24) + ((x & 0xff00) << 8) + ((x & 0xff0000) >> 8) + ((uint32_t)(x) >> 24))
#define u16be(x) (uint16_t)(((x & 0xff) << 8) + ((x & 0xff00) >> 8))

typedef struct
{
   unsigned int start;
   unsigned int length;
   unsigned int vaddr;
} range;

typedef struct
{
   int has_vaddr;
   unsigned int vaddr;
   char *input_file;
   char *output_file;
   int merge_pseudo;
   int emit_glabel;
   asm_syntax syntax;
} arg_config;

static arg_config default_args =
{
   0,    // has_vaddr
   0x0,  // vaddr
   NULL, // input_file
   NULL, // output_file
   0,    // merge_pseudo
   1,    // emit_glabel
   ASM_GAS, // GNU as
};

static void print_usage(void)
{
   ERROR("Usage: elfdump [-g] [-o OUTPUT] [-p] [-s SYNTAX] [-v] OBJFILE [VADDR]\n"
         "\n"
         "elfdump v" ELFDUMP_VERSION ": MIPS ELF object file disassembler\n"
         "\n"
         "Optional arguments:\n"
         " -g           emit \"glabel name\" for global labels\n"
         " -o OUTPUT    output filename (default: stdout)\n"
         " -p           emit pseudoinstructions for related instructions\n"
         "              (not useful for objfiles, but possibly ELF binaries)\n"
         " -s SYNTAX    assembler syntax to use [gas, armips] (default: gas)\n"
         " -v           verbose progress output\n"
         "\n"
         "Arguments:\n"
         " OBJFILE      ELF object file to disassemble .text section for\n"
         " [VADDR]      virtual address of the first instruction\n");
   exit(EXIT_FAILURE);
}

// parse command line arguments
static void parse_arguments(int argc, char *argv[], arg_config *config)
{
   int has_file = 0;
   if (argc < 2) {
      print_usage();
      exit(1);
   }
   for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         switch (argv[i][1]) {
            case 'g':
               config->emit_glabel = 1;
               break;
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
         if (!has_file) {
            config->input_file = argv[i];
            has_file = 1;
         } else if (!config->has_vaddr) {
            config->vaddr = strtoul(argv[i], NULL, 0);
            config->has_vaddr = 1;
         } else {
            print_usage();
         }
      }
   }
   if (!has_file) {
      print_usage();
   }
}

static void add_reloc(disasm_state *state, unsigned int offset, const char *name, int addend, unsigned int vaddr)
{
   char label_name[256];
   if (!strcmp(name, ".text")) {
      vaddr += addend;
      addend = 0;
      if (!disasm_label_lookup(state, vaddr, label_name)) {
         sprintf(label_name, "static_%08X", vaddr);
         disasm_label_add(state, label_name, vaddr);
      }
      name = label_name;
   }

   disasm_reloc_add(state, offset, name, addend);
}

static range parse_elf(disasm_state *state, unsigned char *data, long file_len, arg_config *args)
{
   Elf32_Ehdr *ehdr;
   Elf32_Shdr *shdr, *str_shdr, *sym_shdr, *sym_strtab;
   int text_section_index = -1;
   int symtab_section_index = -1;
   uint32_t text_offset = 0;
   uint32_t vaddr_adj = 0;
   range out_range;

   if (file_len < 4 || data[0] != 0x7f || data[1] != 'E' || data[2] != 'L' || data[3] != 'F') {
      ERROR("Not an ELF file.\n");
      exit(EXIT_FAILURE);
   }

   ehdr = (Elf32_Ehdr *) data;
   if (ehdr->e_ident[EI_DATA] != 2 || u16be(ehdr->e_machine) != 8) {
      ERROR("Not big-endian MIPS.\n");
      exit(EXIT_FAILURE);
   }

   if (u16be(ehdr->e_shstrndx) == 0) {
      // (We could look at program headers instead in this case.)
      ERROR("Missing section headers; stripped binaries are not yet supported.\n");
      exit(EXIT_FAILURE);
   }

#define SECTION(index) (Elf32_Shdr *)(data + u32be(ehdr->e_shoff) + (index) * u16be(ehdr->e_shentsize))
#define STR(strtab, offset) (const char *)(data + u32be(strtab->sh_offset) + offset)

   str_shdr = SECTION(u16be(ehdr->e_shstrndx));
   for (int i = 0; i < u16be(ehdr->e_shnum); i++) {
      shdr = SECTION(i);
      const char *name = STR(str_shdr, u32be(shdr->sh_name));
      if (memcmp(name, ".text", 5) == 0) {
         text_offset = u32be(shdr->sh_offset);
         if (!args->has_vaddr)
            vaddr_adj = out_range.vaddr - u32be(shdr->sh_addr);
         else
            out_range.vaddr = u32be(shdr->sh_addr);
         vaddr_adj = out_range.vaddr - u32be(shdr->sh_addr);
         out_range.length = u32be(shdr->sh_size);
         out_range.start = text_offset;
         text_section_index = i;
      }
      if (u32be(shdr->sh_type) == SHT_SYMTAB) {
         symtab_section_index = i;
      }
   }

   if (text_section_index == -1) {
      ERROR("Missing .text section.\n");
      exit(EXIT_FAILURE);
   }

   if (symtab_section_index == -1) {
      ERROR("Missing symtab section.\n");
      exit(EXIT_FAILURE);
   }

   // add symbols
   sym_shdr = SECTION(symtab_section_index);
   sym_strtab = SECTION(u32be(sym_shdr->sh_link));

   assert(u32be(sym_shdr->sh_entsize) == sizeof(Elf32_Sym));
   for (unsigned int i = 0; i < u32be(sym_shdr->sh_size); i += sizeof(Elf32_Sym)) {
      Elf32_Sym *sym = (Elf32_Sym *)(data + u32be(sym_shdr->sh_offset) + i);
      const char *name = STR(sym_strtab, u32be(sym->st_name));
      uint32_t addr = u32be(sym->st_value);
      if (u16be(sym->st_shndx) != text_section_index || name[0] == '.') {
         continue;
      }
      addr += vaddr_adj;
      disasm_label_add(state, name, addr);
   }

   // add relocations
   for (int i = 0; i < u16be(ehdr->e_shnum); i++) {
      Elf32_Rel *prevHi = NULL;
      shdr = SECTION(i);
      if (u32be(shdr->sh_type) != SHT_REL || u32be(shdr->sh_info) != (unsigned int) text_section_index)
         continue;

      assert(u32be(shdr->sh_link) == (unsigned int) symtab_section_index);
      assert(u32be(shdr->sh_entsize) == sizeof(Elf32_Rel));
      for (unsigned int i = 0; i < u32be(shdr->sh_size); i += sizeof(Elf32_Rel)) {
         Elf32_Rel *rel = (Elf32_Rel *)(data + u32be(shdr->sh_offset) + i);
         uint32_t offset = text_offset + u32be(rel->r_offset);
         uint32_t symIndex = ELF32_R_SYM(u32be(rel->r_info));
         uint32_t rtype = ELF32_R_TYPE(u32be(rel->r_info));
         const char *symName = "0";
         if (symIndex != STN_UNDEF) {
            Elf32_Sym *sym = (Elf32_Sym *)(data + u32be(sym_shdr->sh_offset) + symIndex * sizeof(Elf32_Sym));
            symName = STR(sym_strtab, u32be(sym->st_name));
         }

         if (rtype == R_MIPS_HI16) {
            if (prevHi != NULL) {
               ERROR("Consecutive R_MIPS_HI16.\n");
               exit(EXIT_FAILURE);
            }
            prevHi = rel;
            continue;
         }
         if (rtype == R_MIPS_LO16) {
            int32_t addend = (int16_t)((data[offset + 2] << 8) + data[offset + 3]);
            if (prevHi != NULL) {
               uint32_t offset2 = text_offset + u32be(prevHi->r_offset);
               addend += (uint32_t)((data[offset2 + 2] << 8) + data[offset2 + 3]) << 16;
               add_reloc(state, offset2, symName, addend, out_range.vaddr);
            }
            prevHi = NULL;
            add_reloc(state, offset, symName, addend, out_range.vaddr);
         }
         else if (rtype == R_MIPS_26) {
            int32_t addend = (u32be(*(uint32_t*)(data + offset)) & ((1 << 26) - 1)) << 2;
            if (addend >= (1 << 27)) {
               addend -= 1 << 28;
            }
            add_reloc(state, offset, symName, addend, out_range.vaddr);
         }
         else {
            ERROR("Bad relocation type %d.\n", rtype);
            exit(EXIT_FAILURE);
         }
      }
      if (prevHi != NULL) {
         ERROR("R_MIPS_HI16 without matching R_MIPS_LO16.\n");
         exit(EXIT_FAILURE);
      }
   }

   return out_range;
}
#undef SECTION
#undef STR

int main(int argc, char *argv[])
{
   arg_config args;
   long file_len;
   disasm_state *state;
   unsigned char *data;
   FILE *out;
   range r;

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

   state = disasm_state_init(args.syntax, args.merge_pseudo, args.emit_glabel);

   r = parse_elf(state, data, file_len, &args);

   // run first pass disassembler
   INFO("Disassembling range 0x%X-0x%X at 0x%08X\n", r.start, r.start + r.length, r.vaddr);
   mipsdisasm_pass1(data, r.start, r.length, r.vaddr, state);

   // second pass, generate output
   print_asm_header(out, args.output_file, args.syntax);
   if (args.syntax == ASM_ARMIPS) {
      fprintf(out, ".headersize 0x%08X\n\n", r.vaddr);
   }
   mipsdisasm_pass2(out, state, r.start);
   print_asm_footer(out, args.syntax);

   disasm_state_free(state);
   free(data);
   return EXIT_SUCCESS;
}
