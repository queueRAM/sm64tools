#ifndef SM64_MODS_H_
#define SM64_MODS_H_

// find and decompress all MIO0 blocks
// in_buf: buffer containing entire contents of SM64 data in big endian
// length: length of in_buf
// out_buf: buffer containing extended SM64
void sm64_decompress_mio0(unsigned char *in_buf, unsigned int in_length, unsigned char *out_buf);

// update N64 header CRCs
// buf: buffer containing extended SM64
void sm64_update_checksums(unsigned char *buf);

#endif // SM64_MODS_H_
