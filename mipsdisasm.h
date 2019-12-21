#ifndef MIPSDISASM_H_
#define MIPSDISASM_H_

// typedefs
typedef struct _disasm_state disasm_state;

typedef enum
{
   ASM_GAS,    // GNU as
   ASM_ARMIPS, // armips
} asm_syntax;

// allocate and initialize disassembler state to be passed into disassembler routines
// syntax: assembler syntax to use
// merge_pseudo: if true, attempt to link pseudo instructions
// emit_glabel: if true, emit "glabel name" instead of "name:" for global labels
// returns disassembler state
disasm_state *disasm_state_init(asm_syntax syntax, int merge_pseudo, int emit_glabel);

// free disassembler state allocated during pass1
// state: disassembler state returned from disasm_state_alloc() or mipsdisasm_pass1()
void disasm_state_free(disasm_state *state);

// add a label to the disassembler state
// state: disassembler state returned from disasm_state_alloc() or mipsdisasm_pass1()
// name: string name of label (if NULL, generated based on vaddr)
// vaddr: virtual address of label
void disasm_label_add(disasm_state *state, const char *name, unsigned int vaddr);

// lookup a global label from the disassembler state
// state: disassembler state returned from disasm_state_alloc() or mipsdisasm_pass1()
// vaddr: virtual address of label
// name: string to write label to (or vaddr if label was not found)
// returns 1 if found, 0 otherwise
int disasm_label_lookup(const disasm_state *state, unsigned int vaddr, char *name);

// Add a .text section relocation to the disassembler state, overriding merge_pseudo heuristics.
// Whether the relocation is R_MIPS_LO16 (%lo), R_MIPS_HI16 (%hi) or R_MIPS_26 is implicit based on instruction mnemonic.
//
// state: disassembler state returned from disasm_state_alloc() or mipsdisasm_pass1()
// offset: buffer offset to apply the relocation to
// name: symbol name
// addend: constant to add to the symbol address (typically 0)
void disasm_reloc_add(disasm_state *state, unsigned int offset, const char *name, int addend);

// first pass of disassembler - collects procedures called and sorts them
// data: buffer containing raw MIPS assembly
// offset: buffer offset to start at
// length: length to disassemble starting at 'offset'
// vaddr: virtual address of first byte
// syntax: assembler syntax to use
// state: disassembler state. if NULL, is allocated, returned at end
void mipsdisasm_pass1(unsigned char *data, unsigned int offset, unsigned int length, unsigned int vaddr, disasm_state *state);

// disassemble a region of code, output to file stream
// out: stream to output data to
// state: disassembler state from pass1
// offset: starting offset to match in disassembler state
void mipsdisasm_pass2(FILE *out, disasm_state *state, unsigned int offset);

// get version string of raw disassembler
const char *disasm_get_version(void);

// internal functions for use in mipsdisasm and elfdump
void print_asm_header(FILE *out, const char *output_file, asm_syntax syntax);
void print_asm_footer(FILE *out, asm_syntax syntax);

#endif // MIPSDISASM_H_
