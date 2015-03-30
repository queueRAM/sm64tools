#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "utils.h"

#define is_digit(CHAR_) ((CHAR_) >= '0' && (CHAR_) <= '9')
#define is_hex(CHAR_) (((CHAR_) >= 'a' && (CHAR_) <= 'f') || \
                       ((CHAR_) >= 'A' && (CHAR_) <= 'F'))
#define is_whitespace(CHAR_) (((CHAR_) == ' ') || ((CHAR_) == '\t') || ((CHAR_) == '\n') || ((CHAR_) == '\r'))
#define is_operator(CHAR_) (((CHAR_) == ',') || ((CHAR_) == ';') || ((CHAR_) == '{') || ((CHAR_) == '}'))
// [0-9a-zA-Z_]
#define valid_char(CHAR_) (((CHAR_) >= '0' && (CHAR_) <= '9') || \
                           ((CHAR_) >= 'A' && (CHAR_) <= 'Z') || \
                           ((CHAR_) >= 'a' && (CHAR_) <= 'z') || \
                           ((CHAR_) == '_'))

typedef enum
{
   GROUP_TOP,
   GROUP_TEXTURE,
} config_state;

// remove single line comments starting with # and remove newlines
static void remove_comments(char *line)
{
   unsigned int i;
   int in_str = 0;
   for (i = 0; i < strlen(line); i++) {
      switch (line[i]) {
         case '"':
            in_str = 1 - in_str;
            break;
         case '#':
            if (!in_str) line[i] = '\0';
            return;
         case '\r':
         case '\n':
            line[i] = '\0';
            return;
      }
   }
}

// split line into tokens
char **tokenize_line(char *line, int *tokcount)
{
   static char strpool[2048];
   static char *tokens[64]; // hopefully won't need more than this
   int count;
   int state;
   int linelen;
   int tstart;
   int poolidx;
   int len;
   int i;

   state = 0;
   count = 0;
   poolidx = 0;
   linelen = strlen(line);
   for (i = 0; i < linelen; i++) {
      switch (state) {
         case 0: // looking for token
            if (!is_whitespace(line[i])) {
               if (is_operator(line[i])) {
                  strpool[poolidx] = line[i];
                  strpool[poolidx+1] = '\0';
                  tokens[count] = &strpool[poolidx];
                  count++;
                  poolidx += 2;
               } else {
                  tstart = i;
                  state = 1;
               }
            }
            break;
         case 1: // inside token
            if (is_whitespace(line[i]) || is_operator(line[i])) {
               len = i - tstart;
               memcpy(&strpool[poolidx], &line[tstart], len);
               strpool[poolidx+len] = '\0';
               tokens[count] = &strpool[poolidx];
               count++;
               poolidx += len + 1;
               // operator is always one character
               if (is_operator(line[i])) {
                  strpool[poolidx] = line[i];
                  strpool[poolidx+1] = '\0';
                  tokens[count] = &strpool[poolidx];
                  count++;
                  poolidx += 2;
               }
               state = 0;
            }
            break;
      }
   }
   // token at end of line
   if (state == 1) {
      len = i - tstart;
      memcpy(&strpool[poolidx], &line[tstart], len);
      strpool[poolidx+len] = '\0';
      tokens[count] = &strpool[poolidx];
      count++;
   }
   *tokcount = count;
   return tokens;
}

// convert a string (decimal or hex) to an integer
// returns 0 on failure, 1 on success
static int parse_number(char *str, unsigned int *num)
{
   unsigned int i;
   unsigned int len;
   int hex = 0;
   int start = 0;
   len = strlen(str);
   if (len > 1 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
      hex = 1;
      start = 2;
   }
   for (i = start; i < len; i++) {
      if (!(is_digit(str[i]) || (hex && (is_hex(str[i]))))) {
         return 0;
      }
   }
   *num = strtoul(str, NULL, 0);
   return 1;
}

// extract a quote enclosed string
// returns 0 on success,
//         negative if improper quotes
//         positive if invalid character
static int extract_string(char *dest, char *src)
{
   int i;
   int len = strlen(src);
   if (len > 1 && src[0] == '\"' && src[len-1] == '\"') {
      for (i = 1; i < len-1; i++) {
         if (valid_char(src[i])) {
            dest[i-1] = src[i];
         } else {
            return i;
         }
      }
      dest[len-2] = '\0';
      return 0;
   }
   return -1;
}

int parse_config_file(const char *filename, rom_config *config)
{
   char line[2048];
   char textlabel[128];
   split_section tmpsec;
   texture tmptext;
   split_section *sections;
   split_section *textsec;
   texture *textures;
   FILE *fp;
   char *ret;
   char **toks;
   int tokcount;
   int scount;
   int tcount;
   int tstart = 0;
   int numalloc = 1024;
   int parsing = 1;
   int line_count = 0;
   int mio0count;
   int i;
   config_state state;

   fp = fopen(filename, "r");

   if (!fp) {
      return 1;
   }

   sections = malloc(numalloc * sizeof(*sections));
   memset(sections, 0, numalloc * sizeof(*sections));
   textures = NULL;
   textsec = NULL;

   state = GROUP_TOP;

   mio0count = 0;
   scount = 0;
   tcount = 0;
   while (parsing) {
      ret = fgets(line, sizeof(line), fp);
      if (ret) {
         line_count++;
         remove_comments(line);
         toks = tokenize_line(line, &tokcount);
         if (tokcount > 0) {
            switch (state) {
               case GROUP_TOP:
                  if (0 == strcmp("GLOBAL", toks[0])) {
                     // one liner
                     if (tokcount != 5) {
                        fprintf(stderr, "%d: Error, expected 1 parameter for GLOBAL\n", line_count);
                        exit(1);
                     }
                     if (toks[1][0] != '{' || toks[3][0] != '}' || toks[4][0] != ';') {
                        fprintf(stderr, "%d: Error, GLOBAL syntax error\n", line_count);
                        exit(1);
                     }
                     if (!parse_number(toks[2], &config->ram_offset)) {
                        fprintf(stderr, "%d: Error parsing RAM offset\n", line_count);
                        exit(1);
                     }
                     state = GROUP_TOP;
                  } else if (0 == strcmp("RANGE", toks[0])) {
                     // one liner
                     if (tokcount != 9 && tokcount != 11) {
                        fprintf(stderr, "%d: Error, expected 3 or 4 parameters for RANGE\n", line_count);
                        exit(1);
                     }
                     if (toks[1][0] != '{' || toks[tokcount-2][0] != '}' || toks[tokcount-1][0] != ';') {
                        fprintf(stderr, "%d: Error, RANGE syntax error\n", line_count);
                        exit(1);
                     }
                     if (!parse_number(toks[2], &tmpsec.start)) {
                        fprintf(stderr, "%d: Error parsing RANGE start\n", line_count);
                        exit(1);
                     }
                     if (!parse_number(toks[4], &tmpsec.end)) {
                        fprintf(stderr, "%d: Error parsing RANGE end\n", line_count);
                        exit(1);
                     }
                     if (0 == strcmp("Asm", toks[6])) {
                        tmpsec.type = TYPE_ASM;
                     } else if (0 == strcmp("Behavior", toks[6]) ||
                                0 == strcmp("Behaviour", toks[6])) {
                        tmpsec.type = TYPE_BEHAVIOR;
                     } else if (0 == strcmp("Bin", toks[6])) {
                        tmpsec.type = TYPE_BIN;
                     } else if (0 == strcmp("Header", toks[6])) {
                        tmpsec.type = TYPE_HEADER;
                     } else if (0 == strcmp("LA", toks[6])) {
                        tmpsec.type = TYPE_LA;
                     } else if (0 == strcmp("Level", toks[6])) {
                        tmpsec.type = TYPE_LEVEL;
                     } else if (0 == strcmp("MIO0", toks[6])) {
                        tmpsec.type = TYPE_MIO0;
                        mio0count++;
                     } else if (0 == strcmp("Ptr", toks[6])) {
                        tmpsec.type = TYPE_PTR;
                     } else {
                        fprintf(stderr, "%d: Error, invalid RANGE type \"%s\"\n", line_count, toks[6]);
                        exit(1);
                     }
                     if (tokcount == 11) {
                        if (extract_string(tmpsec.label, toks[8])) {
                           fprintf(stderr, "%d: Error parsing RANGE label\n", line_count);
                           exit(1);
                        }
                     } else {
                        tmpsec.label[0] = '\0';
                     }
                     tmpsec.extra = NULL;
                     tmpsec.extra_len = 0;
                     sections[scount] = tmpsec;
                     scount++;
                     if (scount >= numalloc) {
                        numalloc *= 4;
                        sections = realloc(sections, numalloc * sizeof(*sections));
                     }
                     state = GROUP_TOP;
                  } else if (0 == strcmp("TEXTURE", toks[0])) {
                     state = GROUP_TEXTURE;
                     // first texture needs to allocate space
                     if (textures == NULL) {
                        textures = malloc(32*mio0count * sizeof(*textures));
                     }
                     if (tokcount != 3) {
                        fprintf(stderr, "%d: Error, TEXTURE format\n", line_count);
                        exit(1);
                     }
                     if (toks[2][0] != '{') {
                        fprintf(stderr, "%d: Error, TEXTURE syntax error\n", line_count);
                        exit(1);
                     }
                     if (extract_string(textlabel, toks[1])) {
                        fprintf(stderr, "%d: Error parsing TEXTURE label\n", line_count);
                        exit(1);
                     }
                     // locate section by name
                     textsec = NULL;
                     for (i = 0; i < scount; i++) {
                        if (0 == strcmp(textlabel, sections[i].label)) {
                           textsec = &sections[i];
                        }
                     }
                     if (textsec == NULL) {
                        fprintf(stderr, "%d: Error, unmatched TEXTURE label \"%s\"\n", line_count, textlabel);
                        exit(1);
                     }
                     // keep track of first texture in array
                     tstart = tcount;
                  } else {
                     fprintf(stderr, "%d: Error, expected group name\n", line_count);
                     exit(1);
                  }
                  break;
                  // texture
               case GROUP_TEXTURE:
                  if (tokcount == 2 && toks[0][0] == '}' && toks[1][0] == ';') {
                     state = GROUP_TOP;
                     // end of group of textures, assign texture array to extra
                     textsec->extra = &textures[tstart];
                     textsec->extra_len = tcount - tstart;
                  } else if (tokcount == 12) {
                     unsigned int tmpnum;
                     // one liner
                     if (toks[0][0] != '{' || toks[tokcount-2][0] != '}' || toks[tokcount-1][0] != ',') {
                        fprintf(stderr, "%d: Error TEXTURE syntax\n", line_count);
                        exit(1);
                     }
                     if (!parse_number(toks[1], &tmptext.offset)) {
                        fprintf(stderr, "%d: Error parsing TEXTURE offset\n", line_count);
                        exit(1);
                     }
                     if (!parse_number(toks[5], &tmpnum)) {
                        fprintf(stderr, "%d: Error parsing TEXTURE depth\n", line_count);
                        exit(1);
                     }
                     tmptext.depth = tmpnum;
                     if (!parse_number(toks[7], &tmpnum)) {
                        fprintf(stderr, "%d: Error parsing TEXTURE width\n", line_count);
                        exit(1);
                     }
                     tmptext.width = tmpnum;
                     if (!parse_number(toks[9], &tmpnum)) {
                        fprintf(stderr, "%d: Error parsing TEXTURE height\n", line_count);
                        exit(1);
                     }
                     tmptext.height = tmpnum;
                     if (0 == strcmp("RGBA", toks[3])) {
                        tmptext.format = FORMAT_RGBA;
                     } else if (0 == strcmp("IA", toks[3])) {
                        tmptext.format = FORMAT_IA;
                     } else {
                        fprintf(stderr, "%d: Error invalid TEXTURE format \"%s\"\n", line_count, toks[3]);
                        exit(1);
                     }
                     textures[tcount] = tmptext;
                     tcount++;
                  } else {
                     fprintf(stderr, "%d: Error, expected 5 parameters for TEXTURE \"%s\"\n",
                             line_count, textlabel);
                     exit(1);
                  }
                  break;
            }
         }
      } else {
         parsing = 0;
      }
   }

   fclose(fp);

   config->sections = sections;
   config->section_count = scount;
   return 0;
}

int validate_config(const rom_config *config, unsigned int max_len)
{
   // warn on out of order sections
   // error on overlapped sections
   int i, j;
   unsigned int last_end = 0;
   for (i = 0; i < config->section_count; i++) {
      split_section *isec = &config->sections[i];
      if (isec->start < last_end) {
         ERROR("Error: section %d \"%s\" (%X-%X) out of order\n",
               i, isec->label, isec->start, isec->end);
         return -2;
      }
      if (isec->end > max_len) {
         ERROR("Error: section %d \"%s\" (%X-%X) past end of file (%X)\n",
               i, isec->label, isec->start, isec->end, max_len);
         return -3;
      }
      if (isec->start >= isec->end) {
         ERROR("Error: section %d \"%s\" (%X-%X) invalid range\n",
               i, isec->label, isec->start, isec->end);
         return -4;
      }
      for (j = 0; j < i; j++) {
         split_section *jsec = &config->sections[j];
         if (!((isec->end <= jsec->start) || (isec->start >= jsec->end))) {
            ERROR("Error: section %d \"%s\" (%X-%X) overlaps %d \"%s\" (%X-%X)\n",
                  i, isec->label, isec->start, isec->end,
                  j, jsec->label, jsec->start, jsec->end);
            return -1;
         }
      }
      last_end = isec->end;
   }
   return 0;
}

