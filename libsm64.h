#ifndef LIBSM64_H_
#define LIBSM64_H_

#define MIO0_DIR "mio0files"

// typedefs
typedef struct
{
   char *in_filename;
   char *ext_filename;
   unsigned int ext_size;
   unsigned int padding;
   unsigned int alignment;
   char fill;
   char dump;
} sm64_config_t;

// determine ROM type based on data
// buf: buffer containing raw SM64 ROM file data
// length: length of in
// returns values:
//   -1: not a valid SM64 ROM
//    0: already extended SM64 ROM
//    1: SM64 ROM in byte-swapped (BADC) format
//    2: SM64 ROM in big-endian (ABCD) format
int sm64_rom_type(unsigned char *buf, unsigned int length);

// find and decompress all MIO0 blocks
// config: configuration to determine alignment, padding and size
// in_buf: buffer containing entire contents of SM64 data in big endian
// length: length of in_buf
// out_buf: buffer containing extended SM64
void sm64_decompress_mio0(const sm64_config_t *config,
                          unsigned char *in_buf,
                          unsigned int in_length,
                          unsigned char *out_buf);

// update N64 header checksums
// buf: buffer containing ROM data
// checksums are written into the buffer
void sm64_update_checksums(unsigned char *buf);

#endif // LIBSM64_H_
