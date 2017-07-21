#ifndef LIBSM64_H_
#define LIBSM64_H_

#define MIO0_DIR "mio0files"

// typedefs
typedef enum
{
   ROM_INVALID,     // not valid SM64 ROM
   ROM_SM64_BS,     // SM64 byte-swapped (BADC)
   ROM_SM64_BE,     // SM64 big-endian (ABCD)
   ROM_SM64_LE,     // SM64 little-endian
   ROM_SM64_BE_EXT, // SM64 big-endian, extended
} rom_type;

typedef struct
{
   char *in_filename;
   char *ext_filename;
   unsigned int ext_size;
   unsigned int padding;
   unsigned int alignment;
   char fill;
   char dump;
} sm64_config;

// determine ROM type based on data
// buf: buffer containing raw SM64 ROM file data
// length: length of in
// returns SM64 ROM type or invalid
rom_type sm64_rom_type(unsigned char *buf, unsigned int length);

// find and decompress all MIO0 blocks
// config: configuration to determine alignment, padding and size
// in_buf: buffer containing entire contents of SM64 data in big endian
// length: length of in_buf
// out_buf: buffer containing extended SM64
void sm64_decompress_mio0(const sm64_config *config,
                          unsigned char *in_buf,
                          unsigned int in_length,
                          unsigned char *out_buf);

// update N64 header checksums
// buf: buffer containing ROM data
// checksums are written into the buffer
void sm64_update_checksums(unsigned char *buf);

#endif // LIBSM64_H_
