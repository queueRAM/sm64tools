#ifndef LIBBLAST_H_
#define LIBBLAST_H_

// 802A5E10 (061650)
// just a memcpy from a0 to a3
int decode_block0(unsigned char *in, int length, unsigned char *out);

// 802A5AE0 (061320)
int decode_block1(unsigned char *in, int length, unsigned char *out);

// 802A5B90 (0613D0)
int decode_block2(unsigned char *in, int length, unsigned char *out);

// 802A5A2C (06126C)
int decode_block3(unsigned char *in, int length, unsigned char *out);

// 802A5C5C (06149C)
int decode_block4(unsigned char *in, int length, unsigned char *out, unsigned char *lut);

// 802A5D34 (061574)
int decode_block5(unsigned char *in, int length, unsigned char *out, unsigned char *lut);

// 802A5958 (061198)
int decode_block6(unsigned char *in, int length, unsigned char *out);

// decode Blast Corps compressed data of given type
// in_filename - input file name of compressed data
// type - type of compression: 0-6
// out_filename - output file name of uncompressed data
// lut - lookup table to use for types 4 and 5
// returns 0 on success, non-0 otherwise
int blast_decode_file(char *in_filename, int type, char *out_filename, unsigned char *lut);

#endif // LIBBLAST_H_
