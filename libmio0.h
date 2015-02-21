#ifndef LIBMIO0_H_
#define LIBMIO0_H_

// typedefs

typedef struct
{
   unsigned int dest_size;
   unsigned int comp_offset;
   unsigned int uncomp_offset;
   int big_endian;
} mio0_header_t;

// function prototypes


// decode MIO0 header
// returns 1 if valid header, 0 otherwise
int mio0_decode_header(const unsigned char *buf, mio0_header_t *head);

// encode MIO0 header from struct
void mio0_encode_header(unsigned char *buf, const mio0_header_t *head);

// decode MIO0 data in memory
// in: buffer containing MIO0 data
// out: buffer for output data - allocated by this function
// returns bytes extracted into output or negative value on failure
int mio0_decode(const unsigned char *in, unsigned char *out);

// encode MIO0 data in memory
// in: buffer containing raw data
// out: buffer for MIO0 data allocated by this function
// returns size of output buffer
int mio0_encode(const unsigned char *in, unsigned int length, unsigned char **out);

int mio0_decode_file(const char *in_file, unsigned long offset, const char *out_file);

int mio0_encode_file(const char *in_file, const char *out_file);

#endif // LIBMIO0_H_
