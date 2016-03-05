#include <stdio.h>
#include <stdlib.h>

#include <png.h>

#include "n64graphics.h"
#include "utils.h"

// upscale 5-bit integer to 8-bit
#define SCALE_5_8(VAL_) (((VAL_) * 0xFF) / 0x1F)
#define SCALE_8_5(VAL_) (((VAL_) * 0x1F) / 0xFF)
#define SCALE_4_8(VAL_) ((VAL_) * 0x11)
#define SCALE_8_4(VAL_) ((VAL_) / 0x11)
#define SCALE_3_8(VAL_) ((VAL_) * 0x24)
#define SCALE_8_3(VAL_) ((VAL_) / 0x24)

typedef enum
{
   IMG_FORMAT_RGBA,
   IMG_FORMAT_IA,
   IMG_FORMAT_SKYBOX,
} img_format;


rgba *raw2rgba(char *raw, int width, int height, int depth)
{
   rgba *img = NULL;
   unsigned img_size;
   int i;

   img_size = width * height * sizeof(*img);
   img = malloc(img_size);
   if (!img) {
      ERROR("Error allocating %u bytes\n", img_size);
      return NULL;
   }

   if (depth == 16) {
      for (i = 0; i < width * height; i++) {
         img[i].red   = SCALE_5_8((raw[i*2] & 0xF8) >> 3);
         img[i].green = SCALE_5_8(((raw[i*2] & 0x07) << 2) | ((raw[i*2+1] & 0xC0) >> 6));
         img[i].blue  = SCALE_5_8((raw[i*2+1] & 0x3E) >> 1);
         img[i].alpha = (raw[i*2+1] & 0x01) ? 0xFF : 0x00;
      }
   } else if (depth == 32) {
      for (i = 0; i < width * height; i++) {
         img[i].red   = raw[i*4];
         img[i].green = raw[i*4+1];
         img[i].blue  = raw[i*4+2];
         img[i].alpha = raw[i*4+3];
      }
   }

   return img;
}

ia *raw2ia(char *raw, int width, int height, int depth)
{
   ia *img = NULL;
   unsigned img_size;
   int i;

   img_size = width * height * sizeof(*img);
   img = malloc(img_size);
   if (!img) {
      ERROR("Error allocating %u bytes\n", img_size);
      return NULL;
   }

   switch (depth) {
      case 16:
         for (i = 0; i < width * height; i++) {
            img[i].intensity = raw[i*2];
            img[i].alpha     = raw[i*2+1];
         }
         break;
      case 8:
         for (i = 0; i < width * height; i++) {
            img[i].intensity = SCALE_4_8((raw[i] & 0xF0) >> 4);
            img[i].alpha     = SCALE_4_8(raw[i] & 0x0F);
         }
         break;
      case 4:
         for (i = 0; i < width * height; i++) {
            unsigned char bits;
            bits = raw[i/2];
            if (i % 2) {
               bits &= 0xF;
            } else {
               bits >>= 4;
            }
            img[i].intensity = SCALE_3_8((bits >> 1) & 0x07);
            img[i].alpha     = (bits & 0x01) ? 0xFF : 0x00;
         }
         break;
      case 1:
         for (i = 0; i < width * height; i++) {
            unsigned char bits;
            unsigned char mask;
            bits = raw[i/8];
            mask = 1 << (7 - (i % 8)); // MSb->LSb
            bits = (bits & mask) ? 0xFF : 0x00;
            img[i].intensity = bits;
            img[i].alpha     = bits;
         }
         break;
      default:
         ERROR("Error invalid depth %d\n", depth);
         break;
   }

   return img;
}

rgba *file2rgba(char *filename, int offset, int width, int height, int depth)
{
   FILE *fp;
   rgba *img = NULL;
   char *raw;
   unsigned size;

   fp = fopen(filename, "rb");
   if (!fp) {
      return NULL;
   }

   if (fseek(fp, offset, SEEK_SET)) {
      ERROR("Error seeking to 0x%X in file '%s'\n", offset, filename);
      goto file2rgba_close;
   }

   size = width * height * depth / 8;
   raw = malloc(size);
   if (!raw) {
      ERROR("Error allocating %u bytes\n", size);
      goto file2rgba_close;
   }
   if (fread(raw, 1, size, fp) != size) {
      ERROR("Error reading 0x%X bytes at 0x%X from '%s'\n", size, offset, filename);
      goto file2rgba_free;
   }

   img = raw2rgba(raw, width, height, depth);

file2rgba_free:
   free(raw);
file2rgba_close:
   fclose(fp);

   return img;
}

ia *file2ia(char *filename, int offset, int width, int height, int depth)
{
   FILE *fp;
   ia *img = NULL;
   char *raw;
   unsigned size;
   unsigned img_size;

   fp = fopen(filename, "rb");
   if (!fp) {
      return NULL;
   }

   if (fseek(fp, offset, SEEK_SET)) {
      ERROR("Error seeking to 0x%X in file '%s'\n", offset, filename);
      goto file2ia_close;
   }

   size = width * height * depth / 8;
   raw = malloc(size);
   if (!raw) {
      ERROR("Error allocating %u bytes\n", size);
      goto file2ia_close;
   }
   if (fread(raw, 1, size, fp) != size) {
      ERROR("Error reading 0x%X bytes at 0x%X from '%s'\n", size, offset, filename);
      goto file2ia_free;
   }

   img_size = width * height * sizeof(*img);
   img = malloc(img_size);
   if (!img) {
      ERROR("Error allocating %u bytes\n", img_size);
      goto file2ia_free;
   }

   img = raw2ia(raw, width, height, depth);

file2ia_free:
   free(raw);
file2ia_close:
   fclose(fp);

   return img;
}

int rgba2file(rgba *img, int offset, int width, int height, int depth, char *filename)
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
      ERROR("Error seeking to 0x%X in file '%s'\n", offset, filename);
      goto rgba2file_close;
   }

   size = width * height * depth / 8;
   raw = malloc(size);
   if (!raw) {
      ERROR("Error allocating %u bytes\n", size);
      goto rgba2file_close;
   }

   if (depth == 16) {
      for (i = 0; i < width * height; i++) {
         char r, g, b, a;
         r = SCALE_8_5(img[i].red);
         g = SCALE_8_5(img[i].green);
         b = SCALE_8_5(img[i].blue);
         a = img[i].alpha ? 0x1 : 0x0;
         raw[i*2]   = (r << 3) | (g >> 2);
         raw[i*2+1] = ((g & 0x3) << 6) | (b << 1) | a;
      }
   } else if (depth == 32) {
      for (i = 0; i < width * height; i++) {
         raw[i*4]   = img[i].red;
         raw[i*4+1] = img[i].green;
         raw[i*4+2] = img[i].blue;
         raw[i*4+3] = img[i].alpha;
      }
   }
   if (fwrite(raw, 1, size, fp) != size) {
      ERROR("Error writing %u bytes to output file '%s'\n", size, filename);
   }

   free(raw);
rgba2file_close:
   fclose(fp);

   return 0;
}

int ia2file(ia *img, int offset, int width, int height, int depth, char *filename)
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
      ERROR("Error seeking to 0x%X in file '%s'\n", offset, filename);
      goto ia2file_close;
   }

   size = width * height * depth / 8;
   raw = malloc(size);
   if (!raw) {
      ERROR("Error allocating %u bytes\n", size);
      goto ia2file_close;
   }

   switch (depth) {
      case 16:
         for (i = 0; i < width * height; i++) {
            raw[i*2]   = img[i].intensity;
            raw[i*2+1] = img[i].alpha;
         }
         break;
      case 8:
         for (i = 0; i < width * height; i++) {
            unsigned char val = SCALE_8_4(img[i].intensity);
            unsigned char alpha = SCALE_8_4(img[i].alpha);
            raw[i] = (val << 4) | alpha;
         }
         break;
      case 4:
         for (i = 0; i < width * height; i++) {
            unsigned char val = SCALE_8_3(img[i].intensity);
            unsigned char alpha = img[i].alpha ? 0x01 : 0x00;
            unsigned char old = raw[i/2];
            if (i % 2) {
               raw[i/2] = (old & 0xF0) | (val << 1) | alpha;
            } else {
               raw[i/2] = (old & 0x0F) | (((val << 1) | alpha) << 4);
            }
         }
         break;
      case 1:
         for (i = 0; i < width * height; i++) {
            unsigned char val = img[i].intensity;
            unsigned char old = raw[i/8];
            unsigned char bit = val ? (1 << (7 - (i % 8))) : 0x00;
            raw[i/8] = (old & (~bit)) | bit;
         }
         break;
      default:
         ERROR("Error invalid depth %d\n", depth);
         break;
   }

   if (size > 0) {
      if (fwrite(raw, 1, size, fp) != size) {
         ERROR("Error writing %u bytes to ouput file '%s'\n", size, filename);
      }
      free(raw);
   }

ia2file_close:
   fclose(fp);

   return 0;
}

int sky2file(rgba *img, int offset, int width, int height, char *filename)
{
   FILE *fp;
   char *raw;
   unsigned size = 0;
   int tx, ty;
   int n, m;
   int i, j;

   fp = fopen(filename, "r+b");
   if (!fp) {
      return -1;
   }

   if (fseek(fp, offset, SEEK_SET)) {
      ERROR("Error seeking to 0x%X in file '%s'\n", offset, filename);
      goto sky2file_close;
   }

   size = 32*32 * 2; // 16-bit
   raw = malloc(size);
   if (!raw) {
      ERROR("Error allocating %u bytes\n", size);
      goto sky2file_close;
   }

   // upscale NxM 31x31 to NxM 32x32
   m = width / 31;
   n = height / 31;
   for (ty = 0; ty < n; ty++) {
      for (tx = 0; tx < m; tx++) {
         for (j = 0; j < 32; j++) {
            for (i = 0; i < 32; i++) {
               char r, g, b, a;
               int idx = width * ((31*ty + j) % height) + ((31*tx + i) % width);
               int out_idx = 32*j + i;
               r = SCALE_8_5(img[idx].red);
               g = SCALE_8_5(img[idx].green);
               b = SCALE_8_5(img[idx].blue);
               a = img[idx].alpha ? 0x1 : 0x0;
               raw[out_idx*2]   = (r << 3) | (g >> 2);
               raw[out_idx*2+1] = ((g & 0x3) << 6) | (b << 1) | a;
            }
         }
         if (fwrite(raw, 1, size, fp) != size) {
            ERROR("Error writing %u bytes to output file '%s'\n", size, filename);
         }
      }
   }

   free(raw);
sky2file_close:
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
   png_bytep  *row_pointers;
   png_byte    color_type;
   FILE *fp;
   rgba *img = NULL;
   int w = 0, h = 0;
   int i, j;

   // open file and validate PNG
   fp = fopen(pngname, "rb");
   if (!fp) {
      ERROR("Error opening PNG file '%s'\n", pngname);
      return NULL;
   }

   fread(header, 1, 8, fp);
   if (png_sig_cmp(header, 0, 8)) {
      ERROR("File '%s' is not recognized as a PNG file\n", pngname);
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
   color_type = png_get_color_type(png_ptr, info_ptr);

   png_read_update_info(png_ptr, info_ptr);

   row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * h);
   for (j = 0; j < h; j++) {
      row_pointers[j] = (png_byte*) malloc(png_get_rowbytes(png_ptr, info_ptr));
   }

   png_read_image(png_ptr, row_pointers);

   img = malloc(w * h * sizeof(*img));

   switch (color_type) {
      case PNG_COLOR_TYPE_RGB_ALPHA:
         for (j = 0; j < h; j++) {
            for (i = 0; i < w; i++) {
               img[j*w + i].red   = row_pointers[j][4*i];
               img[j*w + i].green = row_pointers[j][4*i+1];
               img[j*w + i].blue  = row_pointers[j][4*i+2];
               img[j*w + i].alpha = row_pointers[j][4*i+3];
            }
         }
         break;
      case PNG_COLOR_TYPE_GRAY_ALPHA:
         for (j = 0; j < h; j++) {
            for (i = 0; i < w; i++) {
               img[j*w + i].red   = row_pointers[j][2*i];
               img[j*w + i].green = row_pointers[j][2*i];
               img[j*w + i].blue  = row_pointers[j][2*i];
               img[j*w + i].alpha = row_pointers[j][2*i+1];
            }
         }
         break;
      default:
         ERROR("Don't know how to read PNG color type: %d\n", color_type);
         exit(1);
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

int ia2png(ia *img, int width, int height, char *pngname)
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
         PNG_COLOR_TYPE_GRAY_ALPHA,
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT,
         PNG_FILTER_TYPE_DEFAULT);

   // initialize rows of png
   row_pointers = png_malloc(png_ptr, height * sizeof(png_byte*));
   for (j = 0; j < height; j++) {
      png_byte *row = png_malloc(png_ptr, sizeof(png_byte) * width * 4);
      row_pointers[j] = row;
      for (i = 0; i < width; i++) {
         *row++ = img[j*width+i].intensity;
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

ia *pngfile2ia(char *pngname, int *width, int *height)
{
   unsigned char header[8];
   png_structp png_ptr;
   png_infop   info_ptr;
   png_bytep  *row_pointers;
   png_byte    color_type;
   FILE *fp;
   ia *img = NULL;
   int w = 0, h = 0;
   int i, j;

   // open file and validate PNG
   fp = fopen(pngname, "rb");
   if (!fp) {
      ERROR("Error opening PNG file '%s'\n", pngname);
      return NULL;
   }

   fread(header, 1, 8, fp);
   if (png_sig_cmp(header, 0, 8)) {
      ERROR("File '%s' is not recognized as a PNG file\n", pngname);
      goto pngfile2ia_close;
   }

   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

   if (!png_ptr) {
      ERROR("png_create_read_struct failed\n");
      goto pngfile2ia_close;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (!info_ptr) {
      ERROR("png_create_info_struct failed\n");
      goto pngfile2ia_close;
   }

   png_init_io(png_ptr, fp);
   png_set_sig_bytes(png_ptr, 8);

   png_read_info(png_ptr, info_ptr);

   w = png_get_image_width(png_ptr, info_ptr);
   h = png_get_image_height(png_ptr, info_ptr);
   color_type = png_get_color_type(png_ptr, info_ptr);

   png_read_update_info(png_ptr, info_ptr);

   row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * h);
   for (j = 0; j < h; j++) {
      row_pointers[j] = (png_byte*) malloc(png_get_rowbytes(png_ptr, info_ptr));
   }

   png_read_image(png_ptr, row_pointers);

   img = malloc(w * h * sizeof(*img));

   switch (color_type) {
      case PNG_COLOR_TYPE_RGB_ALPHA:
         ERROR("Warning: averaging RGBA PNG to create gray IA\n");
         for (j = 0; j < h; j++) {
            for (i = 0; i < w; i++) {
               int sum = row_pointers[j][4*i] + row_pointers[j][4*i+1] + row_pointers[j][4*i+2];
               img[j*w + i].intensity = sum / 3;
               img[j*w + i].alpha = row_pointers[j][4*i+3];
            }
         }
         break;
      case PNG_COLOR_TYPE_GRAY_ALPHA:
         for (j = 0; j < h; j++) {
            for (i = 0; i < w; i++) {
               img[j*w + i].intensity = row_pointers[j][2*i];
               img[j*w + i].alpha     = row_pointers[j][2*i+1];
            }
         }
         break;
      default:
         ERROR("Don't know how to read PNG color type: %d\n", color_type);
         exit(1);
   }

   // cleanup
   for (j = 0; j < h; j++) {
      free(row_pointers[j]);
   }
   free(row_pointers);

pngfile2ia_close:
   fclose(fp);

   *width = w;
   *height = h;
   return img;
}

const char *graphics_get_version(void)
{
   return PNG_LIBPNG_VER_STRING;
}

#ifdef N64GRAPHICS_STANDALONE
#define N64GRAPHICS_VERSION "0.2"
#include <string.h>

static void print_usage(void)
{
   ERROR("Usage: n64graphics BINFILE [PNGFILE ...]\n"
         "\n"
         "n64graphics v" N64GRAPHICS_VERSION ": N64 graphics manipulator\n"
         "\n"
         "File arguments:\n"
         " BINFILE binary file to update\n"
         " PNGFILE list of PNG files to insert with filename: \"*.<offset>.<format><depth>.png\"\n");
   exit(EXIT_FAILURE);
}

// decode offset, format, and depth based on filename: <offset>.<format><depth>.png
// default if no format/depth specified, defaults to RGBA, 16
static void get_image_info(char *filename, int *offset, img_format *format, int *depth)
{
   char tmpname[512];
   char *base;
   char *stroffset;
   char *strformat;
   unsigned int c;
   int mode = 0;

   // defaults
   *offset = 0;
   *format = IMG_FORMAT_RGBA;
   *depth = 16;

   strcpy(tmpname, filename);

   // remove file extension and leading path
   base = basename(tmpname);
   strformat = base;
   for (c = 0; c < strlen(base); c++) {
      switch (mode) {
         case 0:
            if (base[c] == '.') {
               mode = 1;
               stroffset = &base[c+1];
            }
            break;
         case 1:
            if (base[c] == '.') {
               base[c] = '\0';
               *offset = strtoul(stroffset, NULL, 0);
               mode = 2;
               strformat = &base[c+1];
               base[c] = '.';
            }
            break;
         case 2:
            if (base[c] == '.') {
               base[c] = '\0';
               if (!strcmp("ia1", strformat)) {
                  *format = IMG_FORMAT_IA;
                  *depth = 1;
               } else if (!strcmp("ia4", strformat)) {
                  *format = IMG_FORMAT_IA;
                  *depth = 4;
               } else if (!strcmp("ia8", strformat)) {
                  *format = IMG_FORMAT_IA;
                  *depth = 8;
               } else if (!strcmp("ia16", strformat)) {
                  *format = IMG_FORMAT_IA;
                  *depth = 16;
               } else if (!strcmp("rgba32", strformat)) {
                  *format = IMG_FORMAT_RGBA;
                  *depth = 32;
               } else if (!strcmp("skybox", strformat)) {
                  *format = IMG_FORMAT_SKYBOX;
                  *depth = 16;
               }
               mode = 3;
            }
            break;
      }
   }
}

int main(int argc, char *argv[])
{
   char *binfilename;
   rgba *imgr;
   ia *imgi;
   img_format format;
   int offset, depth;
   int width, height;
   int i;

   if (argc < 3) {
      print_usage();
   }

   binfilename = argv[1];

   for (i = 2; i < argc; i++) {
      get_image_info(argv[i], &offset, &format, &depth);
      switch (format) {
         case IMG_FORMAT_RGBA:
            imgr = pngfile2rgba(argv[i], &width, &height);
            if (imgr) {
               rgba2file(imgr, offset, width, height, depth, binfilename);
            } else {
               exit(EXIT_FAILURE);
            }
            break;
         case IMG_FORMAT_IA:
            imgi = pngfile2ia(argv[i], &width, &height);
            if (imgi) {
               ia2file(imgi, offset, width, height, depth, binfilename);
            } else {
               exit(EXIT_FAILURE);
            }
            break;
         case IMG_FORMAT_SKYBOX:
            imgr = pngfile2rgba(argv[i], &width, &height);
            if (imgr) {
               sky2file(imgr, offset, width, height, binfilename);
            } else {
               exit(EXIT_FAILURE);
            }
            break;
         default:
            exit(EXIT_FAILURE);
      }
   }

   return 0;
}
#endif // N64GRAPHICS_STANDALONE
