#include <stdio.h>
#include <stdlib.h>

#include <png.h>

#include "n64graphics.h"
#include "utils.h"

typedef enum
{
   IMG_FORMAT_RGBA,
   IMG_FORMAT_IA,
} img_format;

rgba *file2rgba(char *filename, int offset, int width, int height)
{
   FILE *fp;
   rgba *img = NULL;
   char *raw;
   unsigned size;
   unsigned img_size;
   int i;

   fp = fopen(filename, "rb");
   if (!fp) {
      return NULL;
   }

   if (fseek(fp, offset, SEEK_SET)) {
      ERROR("Error reading input file\n");
      goto file2rgba_close;
   }

   size = width * height * 2; // 16-bit
   raw = malloc(size);
   if (!raw) {
      ERROR("Error allocating %u bytes\n", size);
      goto file2rgba_close;
   }
   if (fread(raw, 1, size, fp) != size) {
      ERROR("Error reading input file\n");
      goto file2rgba_free;
   }

   img_size = width * height * sizeof(*img);
   img = malloc(img_size);
   if (!img) {
      ERROR("Error allocating %u bytes\n", img_size);
      goto file2rgba_free;
   }

   for (i = 0; i < width * height; i++) {
      img[i].red   = (raw[i*2] & 0xF8);
      img[i].green = ((raw[i*2] & 0x07) << 5) | ((raw[i*2+1] & 0xC0) >> 3);
      img[i].blue  = (raw[i*2+1] & 0x3E) << 2;
      img[i].alpha = (raw[i*2+1] & 0x01) ? 0xFF : 0x00;
   }

file2rgba_free:
   free(raw);
file2rgba_close:
   fclose(fp);

   return img;
}

rgba *file2ia(char *filename, int offset, int width, int height, int depth)
{
   FILE *fp;
   rgba *img = NULL;
   char *raw;
   unsigned size;
   unsigned img_size;
   int i;

   fp = fopen(filename, "rb");
   if (!fp) {
      return NULL;
   }

   if (fseek(fp, offset, SEEK_SET)) {
      ERROR("Error reading input file\n");
      goto file2rgba_close;
   }

   size = width * height * 2; // 16-bit
   raw = malloc(size);
   if (!raw) {
      ERROR("Error allocating %u bytes\n", size);
      goto file2rgba_close;
   }
   if (fread(raw, 1, size, fp) != size) {
      ERROR("Error reading input file\n");
      goto file2rgba_free;
   }

   img_size = width * height * sizeof(*img);
   img = malloc(img_size);
   if (!img) {
      ERROR("Error allocating %u bytes\n", img_size);
      goto file2rgba_free;
   }

   switch (depth) {
      case 16:
         for (i = 0; i < width * height; i++) {
            img[i].red   = raw[i*2];
            img[i].green = raw[i*2];
            img[i].blue  = raw[i*2];
            img[i].alpha = raw[i*2+1];
         }
         break;
      case 8:
         for (i = 0; i < width * height; i++) {
            unsigned char val = (raw[i] & 0xF0) | ((raw[i] & 0xF0) >> 4);
            img[i].red   = val;
            img[i].green = val;
            img[i].blue  = val;
            img[i].alpha = ((raw[i] & 0x0F) << 0x4) | (raw[i] & 0x0F);
         }
         break;
      default:
         ERROR("Error invalid depth %d\n", depth);
         break;
   }

file2rgba_free:
   free(raw);
file2rgba_close:
   fclose(fp);

   return img;
}

int rgba2file(rgba *img, int offset, int width, int height, char *filename)
{
   FILE *fp;
   char *raw;
   unsigned size;
   int i;

   fp = fopen(filename, "r+b");
   if (!fp) {
      return -1;
   }

   if (fseek(fp, offset, SEEK_SET)) {
      ERROR("Error reading input file\n");
      goto rgba2file_close;
   }

   size = width * height * 2; // 16-bit
   raw = malloc(size);
   if (!raw) {
      ERROR("Error allocating %u bytes\n", size);
      goto rgba2file_close;
   }

   for (i = 0; i < width * height; i++) {
      char r, g, b, a;
      r = img[i].red >> 3;
      g = img[i].green >> 3;
      b = img[i].blue >> 3;
      a = img[i].alpha ? 0x1 : 0x0;
      raw[i*2]   = (r << 3) | (g >> 2);
      raw[i*2+1] = ((g & 0x3) << 6) | (b << 1) | a;
   }
   if (fwrite(raw, 1, size, fp) != size) {
      ERROR("Error reading input file\n");
   }

   free(raw);
rgba2file_close:
   fclose(fp);

   return 0;
}

int ia2file(rgba *img, int offset, int width, int height, int depth, char *filename)
{
   FILE *fp;
   char *raw;
   unsigned size = 0;
   int i;

   fp = fopen(filename, "r+b");
   if (!fp) {
      return -1;
   }

   if (fseek(fp, offset, SEEK_SET)) {
      ERROR("Error reading input file\n");
      goto ia2file_close;
   }

   switch (depth) {
      case 16:
         size = width * height * 2; // 16-bit
         raw = malloc(size);
         if (!raw) {
            ERROR("Error allocating %u bytes\n", size);
            goto ia2file_close;
         }
         for (i = 0; i < width * height; i++) {
            raw[i*2]   = img[i].red;
            raw[i*2+1] = img[i].alpha;
         }
         break;
      case 8:
         size = width * height; // 8-bit
         raw = malloc(size);
         if (!raw) {
            ERROR("Error allocating %u bytes\n", size);
            goto ia2file_close;
         }
         for (i = 0; i < width * height; i++) {
            unsigned char val = (img[i].red >> 4) & 0x0F;
            unsigned char alpha = (img[i].alpha >> 4) & 0x0F;
            raw[i] = (val << 4) | alpha;
         }
         break;
      default:
         ERROR("Error invalid depth %d\n", depth);
         break;
   }

   if (size > 0) {
      if (fwrite(raw, 1, size, fp) != size) {
         ERROR("Error reading input file\n");
      }
      free(raw);
   }

ia2file_close:
   fclose(fp);

   return 0;
}

int rgba2png(rgba *img, int width, int height, char *pngname)
{
   png_structp png_ptr = NULL;
   png_infop   info_ptr = NULL;
   int i, j;
   png_byte **row_pointers = NULL;
   int depth = 8;
   FILE *fp;

   fp = fopen(pngname, "wb");
   if (!fp) {
      return -1;
   }

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (png_ptr == NULL) {
      return -1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL) {
      png_destroy_write_struct(&png_ptr, &info_ptr);
      fclose(fp);
      return -1;
   }

   // set up error handling
   if (setjmp(png_jmpbuf(png_ptr))) {
      png_destroy_write_struct(&png_ptr, &info_ptr);
      fclose(fp);
      return -1;
   }

   // Set image attributes
   png_set_IHDR(png_ptr,
         info_ptr,
         width,
         height,
         depth,
         PNG_COLOR_TYPE_RGB_ALPHA,
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT,
         PNG_FILTER_TYPE_DEFAULT);

   // initialize rows of png
   row_pointers = png_malloc(png_ptr, height * sizeof(png_byte*));
   for (j = 0; j < height; j++) {
      png_byte *row = png_malloc(png_ptr, sizeof(png_byte) * width * 4);
      row_pointers[j] = row;
      for (i = 0; i < width; i++) {
         *row++ = img[j*width+i].red;
         *row++ = img[j*width+i].green;
         *row++ = img[j*width+i].blue;
         *row++ = img[j*width+i].alpha;
      }
   }

   // write the image data
   png_init_io(png_ptr, fp);
   png_set_rows(png_ptr, info_ptr, row_pointers);
   png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

   for (j = 0; j < height; j++) {
      png_free(png_ptr, row_pointers[j]);
   }
   png_free(png_ptr, row_pointers);

   png_destroy_write_struct(&png_ptr, &info_ptr);
   fclose(fp);
   return 0;
}

rgba *pngfile2rgba(char *pngname, int *width, int *height)
{
   unsigned char header[8];
   png_structp png_ptr;
   png_infop   info_ptr;
   png_bytep *row_pointers;
   FILE *fp;
   rgba *img = NULL;
   int w = 0, h = 0;
   int i, j;

   // open file and validate PNG
   fp = fopen(pngname, "rb");
   if (!fp) {
      ERROR("Error opening PNG file\n");
      return NULL;
   }

   fread(header, 1, 8, fp);
   if (png_sig_cmp(header, 0, 8)) {
      ERROR("File is not recognized as a PNG file\n");
      goto pngfile2rgba_close;
   }

   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

   if (!png_ptr) {
      ERROR("png_create_read_struct failed\n");
      goto pngfile2rgba_close;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (!info_ptr) {
      ERROR("png_create_info_struct failed\n");
      goto pngfile2rgba_close;
   }

   png_init_io(png_ptr, fp);
   png_set_sig_bytes(png_ptr, 8);

   png_read_info(png_ptr, info_ptr);

   w = png_get_image_width(png_ptr, info_ptr);
   h = png_get_image_height(png_ptr, info_ptr);

   png_read_update_info(png_ptr, info_ptr);

   row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * h);
   for (j = 0; j < h; j++) {
      row_pointers[j] = (png_byte*) malloc(png_get_rowbytes(png_ptr, info_ptr));
   }

   png_read_image(png_ptr, row_pointers);

   img = malloc(w * h * sizeof(*img));

   for (j = 0; j < h; j++) {
      for (i = 0; i < w; i++) {
         img[j * w + i].red   = row_pointers[j][i*4+0];
         img[j * w + i].green = row_pointers[j][i*4+1];
         img[j * w + i].blue  = row_pointers[j][i*4+2];
         img[j * w + i].alpha = row_pointers[j][i*4+3];
      }
   }

   // cleanup
   for (j = 0; j < h; j++) {
      free(row_pointers[j]);
   }
   free(row_pointers);

pngfile2rgba_close:
   fclose(fp);

   *width = w;
   *height = h;
   return img;
}


#ifdef N64GRAPHICS_STANDALONE
#include <libgen.h>
#include <string.h>

static void print_usage(void)
{
   ERROR("n64graphics <binfile> [png files]\n");
}

// decode offset, format, and depth based on filename: <offset>.<format><depth>.png
// default if no format/depth specified, defaults to RGBA, 16
static void get_image_info(char *filename, int *offset, img_format *format, int *depth)
{
   char *stroffset;
   char *strformat;
   unsigned int c;
   int mode = 0;

   // defaults
   *offset = 0;
   *format = IMG_FORMAT_RGBA;
   *depth = 16;

   // remove file extension and leading path
   stroffset = basename(filename);
   strformat = stroffset;
   for (c = 0; c < strlen(stroffset); c++) {
      switch (mode) {
         case 0:
            if (stroffset[c] == '.') {
               stroffset[c] = '\0';
               *offset = strtoul(stroffset, NULL, 0);
               mode = 1;
               strformat = &stroffset[c+1];
               stroffset[c] = '.';
            }
            break;
         case 1:
            if (stroffset[c] == '.') {
               stroffset[c] = '\0';
               if (!strcmp("ia8", strformat)) {
                  *format = IMG_FORMAT_IA;
                  *depth = 8;
               } else if (!strcmp("ia16", strformat)) {
                  *format = IMG_FORMAT_IA;
                  *depth = 16;
               }
               mode = 2;
            }
            break;
      }
   }
}

int main(int argc, char *argv[])
{
   char *binfilename;
   rgba *img;
   img_format format;
   int offset, depth;
   int width, height;
   int i;

   if (argc < 3) {
      print_usage();
      return 1;
   }

   binfilename = argv[1];

   for (i = 2; i < argc; i++) {
      // convert each PNG image to internal RGBA format
      img = pngfile2rgba(argv[i], &width, &height);
      if (img) {
         get_image_info(argv[i], &offset, &format, &depth);
         switch (format) {
            case IMG_FORMAT_RGBA:
               rgba2file(img, offset, width, height, binfilename);
               break;
            case IMG_FORMAT_IA:
               ia2file(img, offset, width, height, depth, binfilename);
               break;
         }
      } else {
         exit(2);
      }
   }

   return 0;
}
#endif // N64GRAPHICS_STANDALONE
