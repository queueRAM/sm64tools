#include <stdio.h>
#include <stdlib.h>

#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_TGA
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include "n64graphics.h"
#include "utils.h"

// upscale 5-bit integer to 8-bit
#define SCALE_5_8(VAL_) (((VAL_) * 0xFF) / 0x1F)
#define SCALE_8_5(VAL_) ((((VAL_) + 4) * 0x1F) / 0xFF)
#define SCALE_4_8(VAL_) ((VAL_) * 0x11)
#define SCALE_8_4(VAL_) ((VAL_) / 0x11)
#define SCALE_3_8(VAL_) ((VAL_) * 0x24)
#define SCALE_8_3(VAL_) ((VAL_) / 0x24)

typedef enum
{
   IMG_FORMAT_RGBA,
   IMG_FORMAT_IA,
   IMG_FORMAT_I,
   IMG_FORMAT_CI,
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

ia *raw2i(char *raw, int width, int height, int depth)
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
      case 8:
         for (i = 0; i < width * height; i++) {
            img[i].intensity = raw[i];
            img[i].alpha     = 0xFF;
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
            img[i].intensity = SCALE_4_8(bits);
            img[i].alpha     = 0xFF;
         }
         break;
      default:
         ERROR("Error invalid depth %d\n", depth);
         break;
   }

   return img;
}

// extract RGBA from CI raw data and palette
rgba *rawci2rgba(unsigned char *rawci, char *palette, int width, int height, int depth)
{
   char *raw;
   rgba *img = NULL;
   unsigned raw_size;
   int i;

   // first convert to raw RGBA
   raw_size = 2 * width * height;
   raw = malloc(raw_size);
   if (!raw) {
      ERROR("Error allocating %u bytes\n", raw_size);
      return NULL;
   }

   for (i = 0; i < width * height; i++) {
      raw[2*i] = palette[2*rawci[i]];
      raw[2*i+1] = palette[2*rawci[i]+1];
   }

   // then convert to RGBA image data
   img = raw2rgba(raw, width, height, depth);

   free(raw);

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

ia *file2i(char *filename, int offset, int width, int height, int depth)
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
      goto file2i_close;
   }

   size = width * height * depth / 8;
   raw = malloc(size);
   if (!raw) {
      ERROR("Error allocating %u bytes\n", size);
      goto file2i_close;
   }
   if (fread(raw, 1, size, fp) != size) {
      ERROR("Error reading 0x%X bytes at 0x%X from '%s'\n", size, offset, filename);
      goto file2i_free;
   }

   img_size = width * height * sizeof(*img);
   img = malloc(img_size);
   if (!img) {
      ERROR("Error allocating %u bytes\n", img_size);
      goto file2i_free;
   }

   img = raw2i(raw, width, height, depth);

file2i_free:
   free(raw);
file2i_close:
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
               int idx_x = (31*tx + i) % width;
               int idx_y = 31*ty + j;
               // last row is duplicate of previous row
               if ((ty == n - 1) && (j == 31)) {
                  idx_y = 31*ty + j - 1;
               }
               int idx = width * idx_y + idx_x;
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
   int ret = 0;
   unsigned char *data = malloc(4*width*height);

   if (data) {
      for (int j = 0; j < height; j++) {
         for (int i = 0; i < width; i++) {
            int idx = j*width + i;
            data[4*idx]   = img[idx].red;
            data[4*idx + 1] = img[idx].green;
            data[4*idx + 2] = img[idx].blue;
            data[4*idx + 3] = img[idx].alpha;
         }
      }

      // write the image data
      ret = stbi_write_png(pngname, width, height, 4, data, 0);

      free(data);
   }

   return ret;
}

rgba *pngfile2rgba(char *pngname, int *width, int *height)
{
   rgba *img = NULL;
   int w = 0, h = 0;
   int channels = 0;

   stbi_uc *data = stbi_load(pngname, &w, &h, &channels, 4);

   if (data) {
      img = malloc(w * h * sizeof(*img));

      switch (channels) {
         case 3: // red, green, blue
         case 4: // red, green, blue, alpha
            for (int j = 0; j < h; j++) {
               for (int i = 0; i < w; i++) {
                  int idx = j*w + i;
                  img[idx].red   = data[channels*idx];
                  img[idx].green = data[channels*idx + 1];
                  img[idx].blue  = data[channels*idx + 2];
                  if (channels == 4) {
                     img[idx].alpha = data[channels*idx + 3];
                  } else {
                     img[idx].alpha = 0xFF;
                  }
               }
            }
            break;
         case 2: // grey, alpha
            for (int j = 0; j < h; j++) {
               for (int i = 0; i < w; i++) {
                  int idx = j*w + i;
                  img[idx].red   = data[2*idx];
                  img[idx].green = data[2*idx];
                  img[idx].blue  = data[2*idx];
                  img[idx].alpha = data[2*idx + 1];
               }
            }
            break;
         default:
            ERROR("Don't know how to read channels: %d\n", channels);
            exit(1);
      }

      // cleanup
      stbi_image_free(data);
   }

   *width = w;
   *height = h;
   return img;
}

int ia2png(ia *img, int width, int height, char *pngname)
{
   int ret = 0;
   unsigned char *data = malloc(2*width*height);

   if (data) {
      for (int j = 0; j < height; j++) {
         for (int i = 0; i < width; i++) {
            int idx = j*width + i;
            data[2*idx]   = img[idx].intensity;
            data[2*idx + 1] = img[idx].alpha;
         }
      }

      // write the image data
      ret = stbi_write_png(pngname, width, height, 2, data, 0);

      free(data);
   }

   return ret;
}

ia *pngfile2ia(char *pngname, int *width, int *height)
{
   ia *img = NULL;
   int w = 0, h = 0;
   int channels = 0;

   stbi_uc *data = stbi_load(pngname, &w, &h, &channels, 2);

   if (data) {
      img = malloc(w * h * sizeof(*img));
      switch (channels) {
         case 3: // red, green, blue
         case 4: // red, green, blue, alpha
            ERROR("Warning: averaging RGB PNG to create IA\n");
            for (int j = 0; j < h; j++) {
               for (int i = 0; i < w; i++) {
                  int idx = j*w + i;
                  int sum = data[channels*idx] + data[channels*idx + 1] + data[channels*idx + 2];
                  img[idx].intensity = sum / 3;
                  if (channels == 4) {
                     img[idx].alpha = data[channels*idx + 3];
                  } else {
                     img[idx].alpha = 0xFF;
                  }
               }
            }
            break;
         case 2: // grey, alpha
            for (int j = 0; j < h; j++) {
               for (int i = 0; i < w; i++) {
                  int idx = j*w + i;
                  img[idx].intensity = data[2*idx];
                  img[idx].alpha     = data[2*idx + 1];
               }
            }
            break;
         default:
            ERROR("Don't know how to read channels: %d\n", channels);
            exit(1);
      }

      // cleanup
      stbi_image_free(data);
   }

   *width = w;
   *height = h;
   return img;
}

const char *n64graphics_get_read_version(void)
{
   return "stb_image 2.17";
}

const char *n64graphics_get_write_version(void)
{
   return "stb_image_write 1.07";
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
               } else if (!strcmp("i4", strformat)) {
                  *format = IMG_FORMAT_I;
                  *depth = 4;
               } else if (!strcmp("i8", strformat)) {
                  *format = IMG_FORMAT_I;
                  *depth = 8;
               } else if (!strcmp("rgba32", strformat)) {
                  *format = IMG_FORMAT_RGBA;
                  *depth = 32;
               } else if (!strcmp("ci16", strformat)) {
                  *format = IMG_FORMAT_CI;
                  *depth = 16;
               } else if (!strcmp("ci32", strformat)) {
                  *format = IMG_FORMAT_CI;
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
         case IMG_FORMAT_CI:
            imgr = pngfile2rgba(argv[i], &width, &height);
            if (imgr) {
               // TODO
               rgba2file(imgr, offset, width, height, depth, binfilename);
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
