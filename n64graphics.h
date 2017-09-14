#ifndef N64GRAPHICS_H_
#define N64GRAPHICS_H_

typedef struct _rgba
{
   unsigned char red;
   unsigned char green;
   unsigned char blue;
   unsigned char alpha;
} rgba;

typedef struct _ia
{
   unsigned char intensity;
   unsigned char alpha;
} ia;

// extract RGBA image from raw data
rgba *raw2rgba(char *raw, int width, int height, int depth);

// extract IA image from raw data
ia *raw2ia(char *raw, int width, int height, int depth);

// extract I image from raw data
ia *raw2i(char *raw, int width, int height, int depth);

// extract RGBA from CI raw data and palette
rgba *rawci2rgba(unsigned char *rawci, char *palette, int width, int height, int depth);

// extract RGBA image from file at offset to RGBA data
rgba *file2rgba(char *filename, int offset, int width, int height, int depth);

// extract IA image from file at offset to RGBA data
ia *file2ia(char *filename, int offset, int width, int height, int depth);

// extract IA image from file at offset to RGBA data
ia *file2i(char *filename, int offset, int width, int height, int depth);

// save RGBA data to raw file at offset
int rgba2file(rgba *img, int offset, int width, int height, int depth, char *filename);

// save IA data to raw file at offset
int ia2file(ia *img, int offset, int width, int height, int depth, char *filename);

// save RGBA data to PNG file
int rgba2png(rgba *img, int width, int height, char *pngname);

// convert PNG image to RGBA data
rgba *pngfile2rgba(char *pngname, int *width, int *height);

// save IA data to grayscale PNG file
int ia2png(ia *img, int width, int height, char *pngname);

// convert PNG image to IA data
ia *pngfile2ia(char *pngname, int *width, int *height);

// get version of underlying graphics reading library
const char *n64graphics_get_read_version(void);

// get version of underlying graphics writing library
const char *n64graphics_get_write_version(void);

#endif // N64GRAPHICS_H_
