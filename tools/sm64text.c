#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define COUNT_OF(ARR) (sizeof(ARR)/sizeof(ARR[0]))

typedef enum {
   REGION_U,
   REGION_J,
} region;

typedef enum {
   CONVERT_BYTES,
   CONVERT_TEXT,
} conversion;

typedef struct {
   region reg;
   conversion conv;
   union {
      uint8_t *bytes;
      char *text;
   };
   size_t length;
} config;

static const config default_config = {
   .reg = REGION_J,
   .conv = CONVERT_BYTES,
   .bytes = NULL,
   .text = NULL,
   .length = 0,
};

typedef struct {
   uint8_t *bytes;
   size_t blen;
   char *text;
} mapping;

const mapping j_mapping[] = {
   {(uint8_t[]){0x00}, 1, "0"},
   {(uint8_t[]){0x01}, 1, "1"},
   {(uint8_t[]){0x02}, 1, "2"},
   {(uint8_t[]){0x03}, 1, "3"},
   {(uint8_t[]){0x04}, 1, "4"},
   {(uint8_t[]){0x05}, 1, "5"},
   {(uint8_t[]){0x06}, 1, "6"},
   {(uint8_t[]){0x07}, 1, "7"},
   {(uint8_t[]){0x08}, 1, "8"},
   {(uint8_t[]){0x09}, 1, "9"},
   {(uint8_t[]){0x0A}, 1, "A"},
   {(uint8_t[]){0x0B}, 1, "B"},
   {(uint8_t[]){0x0C}, 1, "C"},
   {(uint8_t[]){0x0D}, 1, "D"},
   {(uint8_t[]){0x0E}, 1, "E"},
   {(uint8_t[]){0x0F}, 1, "F"},
   {(uint8_t[]){0x10}, 1, "G"},
   {(uint8_t[]){0x11}, 1, "H"},
   {(uint8_t[]){0x12}, 1, "I"},
   {(uint8_t[]){0x13}, 1, "J"},
   {(uint8_t[]){0x14}, 1, "K"},
   {(uint8_t[]){0x15}, 1, "L"},
   {(uint8_t[]){0x16}, 1, "M"},
   {(uint8_t[]){0x17}, 1, "N"},
   {(uint8_t[]){0x18}, 1, "O"},
   {(uint8_t[]){0x19}, 1, "P"},
   {(uint8_t[]){0x1A}, 1, "Q"},
   {(uint8_t[]){0x1B}, 1, "R"},
   {(uint8_t[]){0x1C}, 1, "S"},
   {(uint8_t[]){0x1D}, 1, "T"},
   {(uint8_t[]){0x1E}, 1, "U"},
   {(uint8_t[]){0x1F}, 1, "V"},
   {(uint8_t[]){0x20}, 1, "W"},
   {(uint8_t[]){0x21}, 1, "X"},
   {(uint8_t[]){0x22}, 1, "Y"},
   {(uint8_t[]){0x23}, 1, "Z"},
   {(uint8_t[]){0x40}, 1, "あ"},
   {(uint8_t[]){0x41}, 1, "い"},
   {(uint8_t[]){0x42}, 1, "う"},
   {(uint8_t[]){0x43}, 1, "え"},
   {(uint8_t[]){0x44}, 1, "お"},
   {(uint8_t[]){0x45}, 1, "か"},
   {(uint8_t[]){0x46}, 1, "き"},
   {(uint8_t[]){0x47}, 1, "く"},
   {(uint8_t[]){0x48}, 1, "け"},
   {(uint8_t[]){0x49}, 1, "こ"},
   {(uint8_t[]){0x4A}, 1, "さ"},
   {(uint8_t[]){0x4B}, 1, "し"},
   {(uint8_t[]){0x4C}, 1, "す"},
   {(uint8_t[]){0x4D}, 1, "せ"},
   {(uint8_t[]){0x4E}, 1, "そ"},
   {(uint8_t[]){0x4F}, 1, "た"},
   {(uint8_t[]){0x50}, 1, "ち"},
   {(uint8_t[]){0x51}, 1, "つ"},
   {(uint8_t[]){0x52}, 1, "て"},
   {(uint8_t[]){0x53}, 1, "と"},
   {(uint8_t[]){0x54}, 1, "な"},
   {(uint8_t[]){0x55}, 1, "に"},
   {(uint8_t[]){0x56}, 1, "ぬ"},
   {(uint8_t[]){0x57}, 1, "ね"},
   {(uint8_t[]){0x58}, 1, "の"},
   {(uint8_t[]){0x59}, 1, "は"},
   {(uint8_t[]){0x5A}, 1, "ひ"},
   {(uint8_t[]){0x5B}, 1, "ふ"},
   {(uint8_t[]){0x5C}, 1, "へ"},
   {(uint8_t[]){0x5D}, 1, "ほ"},
   {(uint8_t[]){0x5E}, 1, "ま"},
   {(uint8_t[]){0x5F}, 1, "み"},
   {(uint8_t[]){0x60}, 1, "む"},
   {(uint8_t[]){0x61}, 1, "め"},
   {(uint8_t[]){0x62}, 1, "も"},
   {(uint8_t[]){0x63}, 1, "や"},
   {(uint8_t[]){0x64}, 1, "ゆ"},
   {(uint8_t[]){0x65}, 1, "よ"},
   {(uint8_t[]){0x66}, 1, "ら"},
   {(uint8_t[]){0x67}, 1, "り"},
   {(uint8_t[]){0x68}, 1, "る"},
   {(uint8_t[]){0x69}, 1, "れ"},
   {(uint8_t[]){0x6A}, 1, "ろ"},
   {(uint8_t[]){0x6B}, 1, "わ"},
   {(uint8_t[]){0x6C}, 1, "を"},
   {(uint8_t[]){0x6D}, 1, "ん"},
   {(uint8_t[]){0x6E}, 1, "。"},
   {(uint8_t[]){0x6F}, 1, "，"},
   {(uint8_t[]){0x70}, 1, "ア"},
   {(uint8_t[]){0x71}, 1, "イ"},
   {(uint8_t[]){0x72}, 1, "ウ"},
   {(uint8_t[]){0x73}, 1, "エ"},
   {(uint8_t[]){0x74}, 1, "オ"},
   {(uint8_t[]){0x75}, 1, "カ"},
   {(uint8_t[]){0x76}, 1, "キ"},
   {(uint8_t[]){0x77}, 1, "ク"},
   {(uint8_t[]){0x78}, 1, "ケ"},
   {(uint8_t[]){0x79}, 1, "コ"},
   {(uint8_t[]){0x7A}, 1, "サ"},
   {(uint8_t[]){0x7B}, 1, "シ"},
   {(uint8_t[]){0x7C}, 1, "ス"},
   {(uint8_t[]){0x7D}, 1, "セ"},
   {(uint8_t[]){0x7E}, 1, "ソ"},
   {(uint8_t[]){0x7F}, 1, "タ"},
   {(uint8_t[]){0x80}, 1, "チ"},
   {(uint8_t[]){0x81}, 1, "ツ"},
   {(uint8_t[]){0x82}, 1, "テ"},
   {(uint8_t[]){0x83}, 1, "ト"},
   {(uint8_t[]){0x84}, 1, "ナ"},
   {(uint8_t[]){0x85}, 1, "ニ"},
   {(uint8_t[]){0x86}, 1, "ヌ"},
   {(uint8_t[]){0x87}, 1, "ネ"},
   {(uint8_t[]){0x88}, 1, "ノ"},
   {(uint8_t[]){0x89}, 1, "ハ"},
   {(uint8_t[]){0x8A}, 1, "ヒ"},
   {(uint8_t[]){0x8B}, 1, "フ"},
   {(uint8_t[]){0x8C}, 1, "ヘ"},
   {(uint8_t[]){0x8D}, 1, "ホ"},
   {(uint8_t[]){0x8E}, 1, "マ"},
   {(uint8_t[]){0x8F}, 1, "ミ"},
   {(uint8_t[]){0x90}, 1, "ム"},
   {(uint8_t[]){0x91}, 1, "メ"},
   {(uint8_t[]){0x92}, 1, "モ"},
   {(uint8_t[]){0x93}, 1, "ヤ"},
   {(uint8_t[]){0x94}, 1, "ユ"},
   {(uint8_t[]){0x95}, 1, "ヨ"},
   {(uint8_t[]){0x96}, 1, "ラ"},
   {(uint8_t[]){0x97}, 1, "リ"},
   {(uint8_t[]){0x98}, 1, "ル"},
   {(uint8_t[]){0x99}, 1, "レ"},
   {(uint8_t[]){0x9A}, 1, "ロ"},
   {(uint8_t[]){0x9B}, 1, "ワ"},
   {(uint8_t[]){0x9D}, 1, "ン"},
   {(uint8_t[]){0x9E}, 1, " "},
   {(uint8_t[]){0x9F}, 1, "ｰ"},
   {(uint8_t[]){0xA0}, 1, "ぇ"},
   {(uint8_t[]){0xA1}, 1, "っ"},
   {(uint8_t[]){0xA2}, 1, "ゃ"},
   {(uint8_t[]){0xA3}, 1, "ゅ"},
   {(uint8_t[]){0xA4}, 1, "ょ"},
   {(uint8_t[]){0xA5}, 1, "ぁ"},
   {(uint8_t[]){0xA6}, 1, "ぃ"},
   {(uint8_t[]){0xA7}, 1, "ぅ"},
   {(uint8_t[]){0xA8}, 1, "ぉ"},
   {(uint8_t[]){0xD0}, 1, "ェ"},
   {(uint8_t[]){0xD1}, 1, "ッ"},
   {(uint8_t[]){0xD2}, 1, "ャ"},
   {(uint8_t[]){0xD3}, 1, "ュ"},
   {(uint8_t[]){0xD4}, 1, "ョ"},
   {(uint8_t[]){0xD5}, 1, "ァ"},
   {(uint8_t[]){0xD6}, 1, "ィ"},
   {(uint8_t[]){0xD7}, 1, "ゥ"},
   {(uint8_t[]){0xD8}, 1, "ォ"},
   {(uint8_t[]){0xE0}, 1, "@"},
   {(uint8_t[]){0xE1}, 1, "("},
   {(uint8_t[]){0xE2}, 1, ")("},
   {(uint8_t[]){0xE3}, 1, ")"},
   {(uint8_t[]){0xE4}, 1, "<->"},
   {(uint8_t[]){0xF2}, 1, "!"},
   {(uint8_t[]){0xF3}, 1, "%"},
   {(uint8_t[]){0xF4}, 1, "?"},
   {(uint8_t[]){0xF5}, 1, "『"},
   {(uint8_t[]){0xF6}, 1, "』"},
   {(uint8_t[]){0xF7}, 1, "~"},
   {(uint8_t[]){0xF8}, 1, "…"},
   {(uint8_t[]){0xF9}, 1, "$"},
   {(uint8_t[]){0xFA}, 1, "*"},
   {(uint8_t[]){0xFB}, 1, "x"},
   {(uint8_t[]){0xFC}, 1, "・"},
   {(uint8_t[]){0xFD}, 1, "#"},
   {(uint8_t[]){0xFE}, 1, "\n"},
   {(uint8_t[]){0xF0, 0x45}, 2, "が"},
   {(uint8_t[]){0xF0, 0x46}, 2, "ぎ"},
   {(uint8_t[]){0xF0, 0x47}, 2, "ぐ"},
   {(uint8_t[]){0xF0, 0x48}, 2, "げ"},
   {(uint8_t[]){0xF0, 0x49}, 2, "ご"},
   {(uint8_t[]){0xF0, 0x4A}, 2, "ざ"},
   {(uint8_t[]){0xF0, 0x4B}, 2, "じ"},
   {(uint8_t[]){0xF0, 0x4C}, 2, "ず"},
   {(uint8_t[]){0xF0, 0x4D}, 2, "ぜ"},
   {(uint8_t[]){0xF0, 0x4E}, 2, "ぞ"},
   {(uint8_t[]){0xF0, 0x4F}, 2, "だ"},
   {(uint8_t[]){0xF0, 0x50}, 2, "ぢ"},
   {(uint8_t[]){0xF0, 0x51}, 2, "づ"},
   {(uint8_t[]){0xF0, 0x52}, 2, "で"},
   {(uint8_t[]){0xF0, 0x53}, 2, "ど"},
   {(uint8_t[]){0xF0, 0x59}, 2, "ば"},
   {(uint8_t[]){0xF0, 0x5A}, 2, "び"},
   {(uint8_t[]){0xF0, 0x5B}, 2, "ぶ"},
   {(uint8_t[]){0xF0, 0x5C}, 2, "べ"},
   {(uint8_t[]){0xF0, 0x5D}, 2, "ぼ"},
   {(uint8_t[]){0xF0, 0x75}, 2, "ガ"},
   {(uint8_t[]){0xF0, 0x76}, 2, "ギ"},
   {(uint8_t[]){0xF0, 0x77}, 2, "グ"},
   {(uint8_t[]){0xF0, 0x78}, 2, "ゲ"},
   {(uint8_t[]){0xF0, 0x79}, 2, "ゴ"},
   {(uint8_t[]){0xF0, 0x7A}, 2, "ザ"},
   {(uint8_t[]){0xF0, 0x7B}, 2, "ジ"},
   {(uint8_t[]){0xF0, 0x7C}, 2, "ズ"},
   {(uint8_t[]){0xF0, 0x7D}, 2, "ゼ"},
   {(uint8_t[]){0xF0, 0x7E}, 2, "ゾ"},
   {(uint8_t[]){0xF0, 0x7F}, 2, "ダ"},
   {(uint8_t[]){0xF0, 0x80}, 2, "ヂ"},
   {(uint8_t[]){0xF0, 0x81}, 2, "ヅ"},
   {(uint8_t[]){0xF0, 0x82}, 2, "デ"},
   {(uint8_t[]){0xF0, 0x83}, 2, "ド"},
   {(uint8_t[]){0xF0, 0x89}, 2, "バ"},
   {(uint8_t[]){0xF0, 0x8A}, 2, "ビ"},
   {(uint8_t[]){0xF0, 0x8B}, 2, "ブ"},
   {(uint8_t[]){0xF0, 0x8C}, 2, "ベ"},
   {(uint8_t[]){0xF0, 0x8D}, 2, "ボ"},
   {(uint8_t[]){0xF1, 0x59}, 2, "ぱ"},
   {(uint8_t[]){0xF1, 0x5A}, 2, "ぴ"},
   {(uint8_t[]){0xF1, 0x5B}, 2, "ぷ"},
   {(uint8_t[]){0xF1, 0x5C}, 2, "ぺ"},
   {(uint8_t[]){0xF1, 0x5D}, 2, "ぽ"},
   {(uint8_t[]){0xF1, 0x89}, 2, "パ"},
   {(uint8_t[]){0xF1, 0x8A}, 2, "ピ"},
   {(uint8_t[]){0xF1, 0x8B}, 2, "プ"},
   {(uint8_t[]){0xF1, 0x8C}, 2, "ペ"},
   {(uint8_t[]){0xF1, 0x8D}, 2, "ポ"},
};

const mapping us_mapping[] = {
   {(uint8_t[]){0x00}, 1, "0"},
   {(uint8_t[]){0x01}, 1, "1"},
   {(uint8_t[]){0x02}, 1, "2"},
   {(uint8_t[]){0x03}, 1, "3"},
   {(uint8_t[]){0x04}, 1, "4"},
   {(uint8_t[]){0x05}, 1, "5"},
   {(uint8_t[]){0x06}, 1, "6"},
   {(uint8_t[]){0x07}, 1, "7"},
   {(uint8_t[]){0x08}, 1, "8"},
   {(uint8_t[]){0x09}, 1, "9"},
   {(uint8_t[]){0x0A}, 1, "A"},
   {(uint8_t[]){0x0B}, 1, "B"},
   {(uint8_t[]){0x0C}, 1, "C"},
   {(uint8_t[]){0x0D}, 1, "D"},
   {(uint8_t[]){0x0E}, 1, "E"},
   {(uint8_t[]){0x0F}, 1, "F"},
   {(uint8_t[]){0x10}, 1, "G"},
   {(uint8_t[]){0x11}, 1, "H"},
   {(uint8_t[]){0x12}, 1, "I"},
   {(uint8_t[]){0x13}, 1, "J"},
   {(uint8_t[]){0x14}, 1, "K"},
   {(uint8_t[]){0x15}, 1, "L"},
   {(uint8_t[]){0x16}, 1, "M"},
   {(uint8_t[]){0x17}, 1, "N"},
   {(uint8_t[]){0x18}, 1, "O"},
   {(uint8_t[]){0x19}, 1, "P"},
   {(uint8_t[]){0x1A}, 1, "Q"},
   {(uint8_t[]){0x1B}, 1, "R"},
   {(uint8_t[]){0x1C}, 1, "S"},
   {(uint8_t[]){0x1D}, 1, "T"},
   {(uint8_t[]){0x1E}, 1, "U"},
   {(uint8_t[]){0x1F}, 1, "V"},
   {(uint8_t[]){0x20}, 1, "W"},
   {(uint8_t[]){0x21}, 1, "X"},
   {(uint8_t[]){0x22}, 1, "Y"},
   {(uint8_t[]){0x23}, 1, "Z"},
   {(uint8_t[]){0x24}, 1, "a"},
   {(uint8_t[]){0x25}, 1, "b"},
   {(uint8_t[]){0x26}, 1, "c"},
   {(uint8_t[]){0x27}, 1, "d"},
   {(uint8_t[]){0x28}, 1, "e"},
   {(uint8_t[]){0x29}, 1, "f"},
   {(uint8_t[]){0x2A}, 1, "g"},
   {(uint8_t[]){0x2B}, 1, "h"},
   {(uint8_t[]){0x2C}, 1, "i"},
   {(uint8_t[]){0x2D}, 1, "j"},
   {(uint8_t[]){0x2E}, 1, "k"},
   {(uint8_t[]){0x2F}, 1, "l"},
   {(uint8_t[]){0x30}, 1, "m"},
   {(uint8_t[]){0x31}, 1, "n"},
   {(uint8_t[]){0x32}, 1, "o"},
   {(uint8_t[]){0x33}, 1, "p"},
   {(uint8_t[]){0x34}, 1, "q"},
   {(uint8_t[]){0x35}, 1, "r"},
   {(uint8_t[]){0x36}, 1, "s"},
   {(uint8_t[]){0x37}, 1, "t"},
   {(uint8_t[]){0x38}, 1, "u"},
   {(uint8_t[]){0x39}, 1, "v"},
   {(uint8_t[]){0x3A}, 1, "w"},
   {(uint8_t[]){0x3B}, 1, "x"},
   {(uint8_t[]){0x3C}, 1, "y"},
   {(uint8_t[]){0x3D}, 1, "z"},
   {(uint8_t[]){0x3E}, 1, "'"},
   {(uint8_t[]){0x3F}, 1, "."},
   {(uint8_t[]){0x50}, 1, "^"}, // up
   {(uint8_t[]){0x51}, 1, "|"}, // down
   {(uint8_t[]){0x52}, 1, "<"}, // left
   {(uint8_t[]){0x53}, 1, ">"}, // right
   {(uint8_t[]){0x54}, 1, "[A]"},
   {(uint8_t[]){0x55}, 1, "[B]"},
   {(uint8_t[]){0x56}, 1, "[C]"},
   {(uint8_t[]){0x57}, 1, "[Z]"},
   {(uint8_t[]){0x58}, 1, "[R]"},
   {(uint8_t[]){0x6F}, 1, ","},
   {(uint8_t[]){0xD0}, 1, "/"},
   {(uint8_t[]){0xD1}, 1, "the"},
   {(uint8_t[]){0xD2}, 1, "you"},
   {(uint8_t[]){0x9E}, 1, " "},
   {(uint8_t[]){0x9F}, 1, "-"},
   {(uint8_t[]){0xE1}, 1, "("},
   {(uint8_t[]){0xE2}, 1, "\""},
   {(uint8_t[]){0xE3}, 1, ")"},
   {(uint8_t[]){0xE4}, 1, "+"},
   {(uint8_t[]){0xE5}, 1, "&"},
   {(uint8_t[]){0xE6}, 1, ":"},
   {(uint8_t[]){0xF2}, 1, "!"},
   {(uint8_t[]){0xF4}, 1, "?"},
   {(uint8_t[]){0xF5}, 1, "{"}, // opening "
   {(uint8_t[]){0xF6}, 1, "}"}, // closing "
   {(uint8_t[]){0xF7}, 1, "~"},
   {(uint8_t[]){0xF9}, 1, "$"},
   {(uint8_t[]){0xFA}, 1, "*"}, // filled star
   {(uint8_t[]){0xFB}, 1, "[x]"},
   {(uint8_t[]){0xFC}, 1, ";"},
   {(uint8_t[]){0xFD}, 1, "#"}, // open star
   {(uint8_t[]){0xFE}, 1, "\n"},
};

static void print_usage(void)
{
   printf("Usage: sm64text [-b] [-t] [-r REGION] INPUT\n");
}

static int is_hex(char val)
{
   return ((val >= '0' && val <= '9') || (val >= 'A' && val <= 'F') || (val >= 'a' && val <= 'f'));
}

static size_t parse_bytes(uint8_t *bytes, const char *hex)
{
   size_t count = 0;
   int i = 0;
   while (hex[i]) {
      if (is_hex(hex[i]) && is_hex(hex[i+1])) {
         const char val[3] = {hex[i], hex[i+1], '\0'};
         bytes[count++] = strtoul(val, NULL, 16);
         i += 2;
      } else {
         i++;
      }
   }
   return count;
}

static void parse_config(config *conf, int argc, char *argv[])
{
   int c;
   while ((c = getopt(argc, argv, "btr:")) != -1) {
      switch (c) {
         case 'b':
            conf->conv = CONVERT_BYTES;
            break;
         case 't':
            conf->conv = CONVERT_TEXT;
            break;
         case 'r':
            switch (tolower(optarg[0])) {
               case 'j': conf->reg = REGION_J; break;
               case 'u': conf->reg = REGION_U; break;
               default: print_usage(); exit(1); break;
            }
            break;
         case '?':
         default:
            print_usage();
            exit(1);
      }
   }

   switch (conf->conv) {
      case CONVERT_BYTES:
      {
         size_t allocation = 1024;
         uint8_t *bytes = malloc(allocation);
         size_t count = 0;
         for (int i = optind; i < argc; i++) {
            count += parse_bytes(&bytes[count], argv[i]);
            if (count >= allocation) {
               allocation *= 2;
               bytes = realloc(bytes, allocation);
            }
         }
         conf->bytes = bytes;
         conf->length = count;
         break;
      }
      case CONVERT_TEXT:
         if (optind < argc) {
            conf->text = argv[optind];
         }
         break;
   }
}

int lookup_longest_bytes(const mapping *map, size_t map_count, const uint8_t *bytes)
{
   int best_idx = -1;
   for (size_t i = 0; i < map_count; i++) {
      if (!memcmp(bytes, map[i].bytes, map[i].blen)) {
         if (best_idx < 0 || map[i].blen > map[best_idx].blen) {
            best_idx = i;
         }
      }
   }
   return best_idx;
}

void print_text(const mapping *map, size_t map_count, uint8_t *bytes, int length)
{
   int i = 0;
   while (i < length && bytes[i] != 0xFF) {
      int best = lookup_longest_bytes(map, map_count, &bytes[i]);
      if (best < 0) {
         fprintf(stderr, "Error: couldn't find %02X\n", bytes[i]);
         exit(1);
      } else {
         printf("%s", map[best].text);
         i += map[best].blen;
      }
   }
}

int lookup_longest_text(const mapping *map, size_t map_count, const char *text)
{
   int best_idx = -1;
   for (size_t i = 0; i < map_count; i++) {
      size_t len = strlen(map[i].text);
      if (!strncmp(text, map[i].text, len)) {
         if (best_idx < 0 || len > strlen(map[best_idx].text)) {
            best_idx = i;
         }
      }
   }
   return best_idx;
}

void print_bytes(const mapping *map, size_t map_count, const char *text)
{
   int i = 0;
   int first = 1;
   size_t len = strlen(text);
   while (i < len) {
      int best = lookup_longest_text(map, map_count, &text[i]);
      if (best < 0) {
         fprintf(stderr, "Error: couldn't find %c\n", text[i]);
         exit(1);
      } else {
         for (int b = 0; b < map[best].blen; b++) {
            if (!first) printf(", ");
            printf("0x%02X", map[best].bytes[b]);
            first = 0;
         }
         i += strlen(map[best].text);
      }
   }
   printf(", 0xFF\n");
}

int main(int argc, char *argv[])
{
   config conf = default_config;
   const mapping *map = us_mapping;
   size_t map_count = COUNT_OF(us_mapping);

   parse_config(&conf, argc, argv);
   if (!conf.bytes) {
      print_usage();
      return 1;
   }

   // select mapping table
   switch (conf.reg) {
      case REGION_U:
         map = us_mapping;
         map_count = COUNT_OF(us_mapping);
         break;
      case REGION_J:
         map = j_mapping;
         map_count = COUNT_OF(j_mapping);
         break;
   }

   // run conversion
   switch (conf.conv) {
      case CONVERT_BYTES: {
         print_text(map, map_count, conf.bytes, conf.length);
         printf("\n");
         break;
      }
      case CONVERT_TEXT:
         print_bytes(map, map_count, conf.text);
         break;
   }

   return 0;
}
