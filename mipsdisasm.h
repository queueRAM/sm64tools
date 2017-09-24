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

// first pass of disassembler - collects procedures called and sorts them
// data: buffer containing raw MIPS assembly
// datalen: length of 'data'
// vaddr: virtual address of first byte
// syntax: assembler syntax to use
// merge_pseudo: if true, link up pseudo instructions
// state: disassembler state. if NULL, is allocated, returned at end
// returns disassembler state collected during first pass
disasm_state *mipsdisasm_pass1(unsigned char *data, size_t data_len, unsigned int vaddr, asm_syntax syntax, int merge_pseudo, disasm_state *state);

// disassemble a region of code, output to file stream
// out: stream to output data to
// state: disassembler state from pass1
void mipsdisasm_pass2(FILE *out, disasm_state *state);

// get version string of raw disassembler
const char *disasm_get_version(void);

#endif // MIPSDISASM_H_
