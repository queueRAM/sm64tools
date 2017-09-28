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
// returns disassembler state
disasm_state *disasm_state_init(asm_syntax syntax, int merge_pseudo);

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
// name: string to write label to
// returns 1 if found, 0 otherwise
int disasm_label_lookup(const disasm_state *state, unsigned int vaddr, char *name);

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

#endif // MIPSDISASM_H_
