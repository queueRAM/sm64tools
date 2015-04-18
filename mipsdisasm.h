#ifndef MIPSDISASM_H_
#define MIPSDISASM_H_

#include "config.h"

// defines

#define MAX_PROCEDURES 4096

// typedefs

typedef struct _local_table
{
   unsigned int *offsets;
   int count;
} local_table;

typedef struct _procedure
{
   unsigned int start;
   unsigned int end;
   local_table locals;
} procedure;

typedef struct _proc_table
{
   procedure procedures[MAX_PROCEDURES];
   int count;
} proc_table;


// first pass of disassembler - collects procedures called and sorts them
// data: buffer containing raw MIPS assembly
// datalen: length of 'data'
// procs: procedure table
void mipsdisasm_pass1(unsigned char *data, long datalen, proc_table *procs, rom_config *config);

// second pass of disassembler - disassemble all procedures in order of procs
// out: stream to output data to
// data: buffer containing raw MIPS assembly
// datalen: length of 'data'
// procs: procedure table
void mipsdisasm_pass2(FILE *out, unsigned char *data, long datalen, proc_table *procs, rom_config *config);

#endif // MIPSDISASM_H_
