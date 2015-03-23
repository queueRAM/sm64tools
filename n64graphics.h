#ifndef N64GRAPHICS_H_
#define N64GRAPHICS_H_

typedef struct _rgba
{
   unsigned char red;
   unsigned char green;
   unsigned char blue;
   unsigned char alpha;
} rgba;

// extract RGBA image from file at offset to RGBA data
rgba *file2rgba(char *filename, int offset, int width, int height);

// extract IA image from file at offset to RGBA data
rgba *file2ia(char *filename, int offset, int width, int height, int depth);

// save RGBA data to raw file at offset
int rgba2file(rgba *img, int offset, int width, int height, char *filename);

// save IA data to raw file at offset
int ia2file(rgba *img, int offset, int width, int height, int depth, char *filename);

// save RGBA data to PNG file
int rgba2png(rgba *img, int width, int height, char *pngname);

// convert PNG image to RGBA data
rgba *pngfile2rgba(char *pngname, int *width, int *height);

#endif // N64GRAPHICS_H_
