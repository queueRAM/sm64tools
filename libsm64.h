#ifndef SM64_MODS_H_
#define SM64_MODS_H_

// typedefs
typedef struct
{
   char *in_filename;
   char *ext_filename;
   unsigned int ext_size;
   unsigned int padding;
   unsigned int alignment;
   char fill;
   char compress;
} sm64_config_t;

// find and decompress all MIO0 blocks
// config: configuration to determine alignment, padding and size
// in_buf: buffer containing entire contents of SM64 data in big endian
// length: length of in_buf
// out_buf: buffer containing extended SM64
void sm64_decompress_mio0(const sm64_config_t *config,
                          unsigned char *in_buf,
                          unsigned int in_length,
                          unsigned char *out_buf);

// find and compact/compress all MIO0 blocks
// config: configuration to determine alignment and compression
// in_buf: buffer containing entire contents of SM64 data in big endian
// length: length of in_buf and max size of out_buf
// out_buf: buffer containing extended SM64
// returns new size in out_buf, rounded up to nearest 4MB
int sm64_compress_mio0(const sm64_config_t *config,
                       unsigned char *in_buf,
                       unsigned int in_length,
                       unsigned char *out_buf);

// update N64 header checksums
// buf: buffer containing ROM data
// checksums are written into the buffer
void sm64_update_checksums(unsigned char *buf);

#endif // SM64_MODS_H_
