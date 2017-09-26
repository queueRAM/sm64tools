#ifndef MIPSDISASM_H_
#define MIPSDISASM_H_

// typedefs
typedef struct _disasm_state disasm_state;

typedef enum
{
   ASM_GAS,    // GNU as
   ASM_ARMIPS, // armips
} asm_syntax;

// allocate disassembler state to be passed into disassembler routines
// returns disassembler state
disasm_state *disasm_state_alloc(void);

// free disassembler state allocated during pass1
// state: disassembler state returned from disasm_state_alloc() or mipsdisasm_pass1()
void disasm_state_free(disasm_state *state);

// add a label to the disassembler state
// state: disassembler state returned from disasm_state_alloc() or mipsdisasm_pass1()
// vaddr: virtual address of label
// name: string name of label (if NULL, generated based on vaddr)
void disasm_state_add_label(disasm_state *state, unsigned int vaddr, const char *name);

// first pass of disassembler - collects procedures called and sorts them
// data: buffer containing raw MIPS assembly
// offset: buffer offset to start at
// data_len: length of 'data'
// vaddr: virtual address of first byte
// syntax: assembler syntax to use
// merge_pseudo: if true, link up pseudo instructions
// state: disassembler state. if NULL, is allocated, returned at end
void mipsdisasm_pass1(unsigned char *data, unsigned int offset, size_t data_len, unsigned int vaddr, disasm_state *state);

// disassemble a region of code, output to file stream
// out: stream to output data to
// state: disassembler state from pass1
// offset: starting offset to match in disassembler state
void mipsdisasm_pass2(FILE *out, disasm_state *state, unsigned int offset);

// get version string of raw disassembler
const char *disasm_get_version(void);

#endif // MIPSDISASM_H_
