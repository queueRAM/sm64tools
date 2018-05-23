#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define SM64TEXT_VERSION "0.2"

#define COUNT_OF(ARR) (sizeof(ARR)/sizeof(ARR[0]))

typedef struct {
   enum {REGION_U, REGION_J} reg;
   enum {CONVERT_BYTES, CONVERT_TEXT, DUMP_TABLE} conv;
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
   .length = 0,
};

typedef struct {
   uint8_t bytes[2];
   size_t blen;
   char *text;
} mapping;

const uint8_t sm64_string_terminator = 0xFF;

const mapping j_mapping[] = {
   {{0x00}, 1, "0"},
   {{0x01}, 1, "1"},
   {{0x02}, 1, "2"},
   {{0x03}, 1, "3"},
   {{0x04}, 1, "4"},
   {{0x05}, 1, "5"},
   {{0x06}, 1, "6"},
   {{0x07}, 1, "7"},
   {{0x08}, 1, "8"},
   {{0x09}, 1, "9"},
   {{0x0A}, 1, "A"},
   {{0x0B}, 1, "B"},
   {{0x0C}, 1, "C"},
   {{0x0D}, 1, "D"},
   {{0x0E}, 1, "E"},
   {{0x0F}, 1, "F"},
   {{0x10}, 1, "G"},
   {{0x11}, 1, "H"},
   {{0x12}, 1, "I"},
   {{0x13}, 1, "J"},
   {{0x14}, 1, "K"},
   {{0x15}, 1, "L"},
   {{0x16}, 1, "M"},
   {{0x17}, 1, "N"},
   {{0x18}, 1, "O"},
   {{0x19}, 1, "P"},
   {{0x1A}, 1, "Q"},
   {{0x1B}, 1, "R"},
   {{0x1C}, 1, "S"},
   {{0x1D}, 1, "T"},
   {{0x1E}, 1, "U"},
   {{0x1F}, 1, "V"},
   {{0x20}, 1, "W"},
   {{0x21}, 1, "X"},
   {{0x22}, 1, "Y"},
   {{0x23}, 1, "Z"},
   {{0x40}, 1, "あ"},
   {{0x41}, 1, "い"},
   {{0x42}, 1, "う"},
   {{0x43}, 1, "え"},
   {{0x44}, 1, "お"},
   {{0x45}, 1, "か"},
   {{0x46}, 1, "き"},
   {{0x47}, 1, "く"},
   {{0x48}, 1, "け"},
   {{0x49}, 1, "こ"},
   {{0x4A}, 1, "さ"},
   {{0x4B}, 1, "し"},
   {{0x4C}, 1, "す"},
   {{0x4D}, 1, "せ"},
   {{0x4E}, 1, "そ"},
   {{0x4F}, 1, "た"},
   {{0x50}, 1, "ち"},
   {{0x51}, 1, "つ"},
   {{0x52}, 1, "て"},
   {{0x53}, 1, "と"},
   {{0x54}, 1, "な"},
   {{0x55}, 1, "に"},
   {{0x56}, 1, "ぬ"},
   {{0x57}, 1, "ね"},
   {{0x58}, 1, "の"},
   {{0x59}, 1, "は"},
   {{0x5A}, 1, "ひ"},
   {{0x5B}, 1, "ふ"},
   {{0x5C}, 1, "へ"},
   {{0x5D}, 1, "ほ"},
   {{0x5E}, 1, "ま"},
   {{0x5F}, 1, "み"},
   {{0x60}, 1, "む"},
   {{0x61}, 1, "め"},
   {{0x62}, 1, "も"},
   {{0x63}, 1, "や"},
   {{0x64}, 1, "ゆ"},
   {{0x65}, 1, "よ"},
   {{0x66}, 1, "ら"},
   {{0x67}, 1, "り"},
   {{0x68}, 1, "る"},
   {{0x69}, 1, "れ"},
   {{0x6A}, 1, "ろ"},
   {{0x6B}, 1, "わ"},
   {{0x6C}, 1, "を"},
   {{0x6D}, 1, "ん"},
   {{0x6E}, 1, "。"},
   {{0x6F}, 1, "，"},
   {{0x70}, 1, "ア"},
   {{0x71}, 1, "イ"},
   {{0x72}, 1, "ウ"},
   {{0x73}, 1, "エ"},
   {{0x74}, 1, "オ"},
   {{0x75}, 1, "カ"},
   {{0x76}, 1, "キ"},
   {{0x77}, 1, "ク"},
   {{0x78}, 1, "ケ"},
   {{0x79}, 1, "コ"},
   {{0x7A}, 1, "サ"},
   {{0x7B}, 1, "シ"},
   {{0x7C}, 1, "ス"},
   {{0x7D}, 1, "セ"},
   {{0x7E}, 1, "ソ"},
   {{0x7F}, 1, "タ"},
   {{0x80}, 1, "チ"},
   {{0x81}, 1, "ツ"},
   {{0x82}, 1, "テ"},
   {{0x83}, 1, "ト"},
   {{0x84}, 1, "ナ"},
   {{0x85}, 1, "ニ"},
   {{0x86}, 1, "ヌ"},
   {{0x87}, 1, "ネ"},
   {{0x88}, 1, "ノ"},
   {{0x89}, 1, "ハ"},
   {{0x8A}, 1, "ヒ"},
   {{0x8B}, 1, "フ"},
   {{0x8C}, 1, "ヘ"},
   {{0x8D}, 1, "ホ"},
   {{0x8E}, 1, "マ"},
   {{0x8F}, 1, "ミ"},
   {{0x90}, 1, "ム"},
   {{0x91}, 1, "メ"},
   {{0x92}, 1, "モ"},
   {{0x93}, 1, "ヤ"},
   {{0x94}, 1, "ユ"},
   {{0x95}, 1, "ヨ"},
   {{0x96}, 1, "ラ"},
   {{0x97}, 1, "リ"},
   {{0x98}, 1, "ル"},
   {{0x99}, 1, "レ"},
   {{0x9A}, 1, "ロ"},
   {{0x9B}, 1, "ワ"},
   {{0x9D}, 1, "ン"},
   {{0x9E}, 1, " "},
   {{0x9F}, 1, "ｰ"},
   {{0xA0}, 1, "ぇ"},
   {{0xA1}, 1, "っ"},
   {{0xA2}, 1, "ゃ"},
   {{0xA3}, 1, "ゅ"},
   {{0xA4}, 1, "ょ"},
   {{0xA5}, 1, "ぁ"},
   {{0xA6}, 1, "ぃ"},
   {{0xA7}, 1, "ぅ"},
   {{0xA8}, 1, "ぉ"},
   {{0xD0}, 1, "ェ"},
   {{0xD1}, 1, "ッ"},
   {{0xD2}, 1, "ャ"},
   {{0xD3}, 1, "ュ"},
   {{0xD4}, 1, "ョ"},
   {{0xD5}, 1, "ァ"},
   {{0xD6}, 1, "ィ"},
   {{0xD7}, 1, "ゥ"},
   {{0xD8}, 1, "ォ"},
   {{0xE0}, 1, "@"},
   {{0xE1}, 1, "("},
   {{0xE2}, 1, ")("},
   {{0xE3}, 1, ")"},
   {{0xE4}, 1, "<->"},
   {{0xF2}, 1, "!"},
   {{0xF3}, 1, "%"},
   {{0xF4}, 1, "?"},
   {{0xF5}, 1, "『"},
   {{0xF6}, 1, "』"},
   {{0xF7}, 1, "~"},
   {{0xF8}, 1, "…"},
   {{0xF9}, 1, "$"},
   {{0xFA}, 1, "*"},
   {{0xFB}, 1, "x"},
   {{0xFC}, 1, "・"},
   {{0xFD}, 1, "#"},
   {{0xFE}, 1, "\n"},
   {{0xF0, 0x45}, 2, "が"},
   {{0xF0, 0x46}, 2, "ぎ"},
   {{0xF0, 0x47}, 2, "ぐ"},
   {{0xF0, 0x48}, 2, "げ"},
   {{0xF0, 0x49}, 2, "ご"},
   {{0xF0, 0x4A}, 2, "ざ"},
   {{0xF0, 0x4B}, 2, "じ"},
   {{0xF0, 0x4C}, 2, "ず"},
   {{0xF0, 0x4D}, 2, "ぜ"},
   {{0xF0, 0x4E}, 2, "ぞ"},
   {{0xF0, 0x4F}, 2, "だ"},
   {{0xF0, 0x50}, 2, "ぢ"},
   {{0xF0, 0x51}, 2, "づ"},
   {{0xF0, 0x52}, 2, "で"},
   {{0xF0, 0x53}, 2, "ど"},
   {{0xF0, 0x59}, 2, "ば"},
   {{0xF0, 0x5A}, 2, "び"},
   {{0xF0, 0x5B}, 2, "ぶ"},
   {{0xF0, 0x5C}, 2, "べ"},
   {{0xF0, 0x5D}, 2, "ぼ"},
   {{0xF0, 0x75}, 2, "ガ"},
   {{0xF0, 0x76}, 2, "ギ"},
   {{0xF0, 0x77}, 2, "グ"},
   {{0xF0, 0x78}, 2, "ゲ"},
   {{0xF0, 0x79}, 2, "ゴ"},
   {{0xF0, 0x7A}, 2, "ザ"},
   {{0xF0, 0x7B}, 2, "ジ"},
   {{0xF0, 0x7C}, 2, "ズ"},
   {{0xF0, 0x7D}, 2, "ゼ"},
   {{0xF0, 0x7E}, 2, "ゾ"},
   {{0xF0, 0x7F}, 2, "ダ"},
   {{0xF0, 0x80}, 2, "ヂ"},
   {{0xF0, 0x81}, 2, "ヅ"},
   {{0xF0, 0x82}, 2, "デ"},
   {{0xF0, 0x83}, 2, "ド"},
   {{0xF0, 0x89}, 2, "バ"},
   {{0xF0, 0x8A}, 2, "ビ"},
   {{0xF0, 0x8B}, 2, "ブ"},
   {{0xF0, 0x8C}, 2, "ベ"},
   {{0xF0, 0x8D}, 2, "ボ"},
   {{0xF1, 0x59}, 2, "ぱ"},
   {{0xF1, 0x5A}, 2, "ぴ"},
   {{0xF1, 0x5B}, 2, "ぷ"},
   {{0xF1, 0x5C}, 2, "ぺ"},
   {{0xF1, 0x5D}, 2, "ぽ"},
   {{0xF1, 0x89}, 2, "パ"},
   {{0xF1, 0x8A}, 2, "ピ"},
   {{0xF1, 0x8B}, 2, "プ"},
   {{0xF1, 0x8C}, 2, "ペ"},
   {{0xF1, 0x8D}, 2, "ポ"},
};

const mapping us_mapping[] = {
   {{0x00}, 1, "0"},
   {{0x01}, 1, "1"},
   {{0x02}, 1, "2"},
   {{0x03}, 1, "3"},
   {{0x04}, 1, "4"},
   {{0x05}, 1, "5"},
   {{0x06}, 1, "6"},
   {{0x07}, 1, "7"},
   {{0x08}, 1, "8"},
   {{0x09}, 1, "9"},
   {{0x0A}, 1, "A"},
   {{0x0B}, 1, "B"},
   {{0x0C}, 1, "C"},
   {{0x0D}, 1, "D"},
   {{0x0E}, 1, "E"},
   {{0x0F}, 1, "F"},
   {{0x10}, 1, "G"},
   {{0x11}, 1, "H"},
   {{0x12}, 1, "I"},
   {{0x13}, 1, "J"},
   {{0x14}, 1, "K"},
   {{0x15}, 1, "L"},
   {{0x16}, 1, "M"},
   {{0x17}, 1, "N"},
   {{0x18}, 1, "O"},
   {{0x19}, 1, "P"},
   {{0x1A}, 1, "Q"},
   {{0x1B}, 1, "R"},
   {{0x1C}, 1, "S"},
   {{0x1D}, 1, "T"},
   {{0x1E}, 1, "U"},
   {{0x1F}, 1, "V"},
   {{0x20}, 1, "W"},
   {{0x21}, 1, "X"},
   {{0x22}, 1, "Y"},
   {{0x23}, 1, "Z"},
   {{0x24}, 1, "a"},
   {{0x25}, 1, "b"},
   {{0x26}, 1, "c"},
   {{0x27}, 1, "d"},
   {{0x28}, 1, "e"},
   {{0x29}, 1, "f"},
   {{0x2A}, 1, "g"},
   {{0x2B}, 1, "h"},
   {{0x2C}, 1, "i"},
   {{0x2D}, 1, "j"},
   {{0x2E}, 1, "k"},
   {{0x2F}, 1, "l"},
   {{0x30}, 1, "m"},
   {{0x31}, 1, "n"},
   {{0x32}, 1, "o"},
   {{0x33}, 1, "p"},
   {{0x34}, 1, "q"},
   {{0x35}, 1, "r"},
   {{0x36}, 1, "s"},
   {{0x37}, 1, "t"},
   {{0x38}, 1, "u"},
   {{0x39}, 1, "v"},
   {{0x3A}, 1, "w"},
   {{0x3B}, 1, "x"},
   {{0x3C}, 1, "y"},
   {{0x3D}, 1, "z"},
   {{0x3E}, 1, "'"},
   {{0x3F}, 1, "."},
   {{0x50}, 1, "^"}, // up
   {{0x51}, 1, "|"}, // down
   {{0x52}, 1, "<"}, // left
   {{0x53}, 1, ">"}, // right
   {{0x54}, 1, "[A]"},
   {{0x55}, 1, "[B]"},
   {{0x56}, 1, "[C]"},
   {{0x57}, 1, "[Z]"},
   {{0x58}, 1, "[R]"},
   {{0x6F}, 1, ","},
   {{0xD0}, 1, "/"},
   {{0xD1}, 1, "the"},
   {{0xD2}, 1, "you"},
   {{0x9E}, 1, " "},
   {{0x9F}, 1, "-"},
   {{0xE1}, 1, "("},
   {{0xE2}, 1, "\""},
   {{0xE3}, 1, ")"},
   {{0xE4}, 1, "+"},
   {{0xE5}, 1, "&"},
   {{0xE6}, 1, ":"},
   {{0xF2}, 1, "!"},
   {{0xF4}, 1, "?"},
   {{0xF5}, 1, "{"}, // opening "
   {{0xF6}, 1, "}"}, // closing "
   {{0xF7}, 1, "~"},
   {{0xF9}, 1, "$"},
   {{0xFA}, 1, "*"}, // filled star
   {{0xFB}, 1, "[x]"},
   {{0xFC}, 1, ";"},
   {{0xFD}, 1, "#"}, // open star
   {{0xFE}, 1, "\n"},
};

static void print_usage(void)
{
   printf("Usage: sm64text [-b] [-t] [-r REGION] [-d] INPUT\n"
         "\n"
         "sm64text v" SM64TEXT_VERSION ": SM64 dialog text encoder/decoder\n"
         "\n"
         "Optional arguments:\n"
         " -b           INPUT is bytes to convert to utf8 text\n"
         " -t           INPUT is utf8 text convert to SM64 dialog encoded bytes (appends 0xFF end of string)\n"
         " -r REGION    region to use: U or J (default: J)\n"
         " -d           dump table to format suitable for armips\n"
         "Required arguments:\n"
         " INPUT        either text string (if using -t) or hex byte pairs (if using -b)\n"
         "\n"
         "Examples:\n"
         " $ sm64text -r u 2B 28 2F 2F 32\n"
         " hello\n"
         " $ sm64text -r u -t hello\n"
         " 0x2B, 0x28, 0x2F, 0x2F, 0x32, 0xFF\n");
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
   while ((c = getopt(argc, argv, "bdtr:")) != -1) {
      switch (c) {
         case 'b':
            conf->conv = CONVERT_BYTES;
            break;
         case 'd':
            conf->conv = DUMP_TABLE;
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
         if (optind < argc) {
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
         }
         break;
      case CONVERT_TEXT:
         if (optind < argc) {
            conf->text = argv[optind];
         }
         break;
      default:
         break;
   }
}

// find longest byte pattern in mapping table that matches 'bytes'
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

// decode bytes to text for given mapping table
void print_text(const mapping *map, size_t map_count, uint8_t *bytes, int length)
{
   int i = 0;
   while (i < length && bytes[i] != sm64_string_terminator) {
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

// find longest matching text in mapping tables that matches 'text'
int lookup_longest_text(const mapping *map, size_t map_count, const char *text)
{
   int best_idx = -1;
   size_t best_len = 0;
   for (size_t i = 0; i < map_count; i++) {
      size_t len = strlen(map[i].text);
      if (!strncmp(text, map[i].text, len)) {
         if (best_idx < 0 || len > best_len) {
            best_idx = i;
            best_len = len;
         }
      }
   }
   return best_idx;
}

void print_bytes(const mapping *map, size_t map_count, const char *text)
{
   size_t i = 0;
   int first = 1;
   size_t len = strlen(text);
   while (i < len) {
      int best = lookup_longest_text(map, map_count, &text[i]);
      if (best < 0) {
         fprintf(stderr, "Error: couldn't find %c\n", text[i]);
         exit(1);
      } else {
         for (size_t b = 0; b < map[best].blen; b++) {
            if (!first) printf(", ");
            printf("0x%02X", map[best].bytes[b]);
            first = 0;
         }
         i += strlen(map[best].text);
      }
   }
   printf(", 0x%02X\n", sm64_string_terminator);
}

static void dump_table(const mapping *map, size_t map_count)
{
   for (size_t i = 0; i < map_count; i++) {
      for (size_t b = 0; b < map[i].blen; b++) {
         printf("%02X", map[i].bytes[b]);
      }
      printf("=");
      switch (map[i].text[0]) {
         case '\n': printf("\\n"); break;
         case '\r': printf("\\r"); break;
         default: printf("%s", map[i].text); break;
      }
      printf("\n");
   }
   printf("/%02X\n", sm64_string_terminator);
}

int main(int argc, char *argv[])
{
   config conf = default_config;
   const mapping *map = us_mapping;
   size_t map_count = COUNT_OF(us_mapping);

   parse_config(&conf, argc, argv);
   if (conf.conv != DUMP_TABLE && !conf.bytes) {
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
      case CONVERT_BYTES:
         print_text(map, map_count, conf.bytes, conf.length);
         printf("\n");
         break;
      case CONVERT_TEXT:
         print_bytes(map, map_count, conf.text);
         break;
      case DUMP_TABLE:
         dump_table(map, map_count);
         break;
   }

   return 0;
}
