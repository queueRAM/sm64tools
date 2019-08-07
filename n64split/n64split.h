#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <zlib.h>

#include "config.h"
#include "libblast.h"
#include "libmio0.h"
#include "libsfx.h"
#include "mipsdisasm.h"
#include "n64graphics.h"
#include "strutils.h"
#include "utils.h"


//================================================================================
//    Constant Definitions
//================================================================================

#define N64SPLIT_VERSION "0.4a"

#define GLOBALS_FILE "globals.inc"
#define MACROS_FILE "macros.inc"

#define MUSIC_SUBDIR    "music"
#define SOUNDS_SUBDIR   "sounds"
#define BIN_SUBDIR      "bin"
#define ASM_SUBDIR      "asm"
#define MIO0_SUBDIR     "bin"
#define TEXTURE_SUBDIR  "textures"
#define GEO_SUBDIR      "geo"
#define LEVEL_SUBDIR    "levels"
#define MODEL_SUBDIR    "models"
#define BEHAVIOR_SUBDIR "."


//================================================================================
//    Structure Definitions
//================================================================================

/* Main */
typedef struct _arg_config
{
   char input_file[FILENAME_MAX];
   char config_file[FILENAME_MAX];
   char output_dir[FILENAME_MAX];
   float model_scale;
   bool raw_texture; // TODO: this should be the default path once n64graphics is updated
   bool large_texture;
   bool large_texture_depth;
   bool keep_going;
   bool merge_pseudo;
} arg_config;

typedef enum {
   N64_ROM_INVALID,
   N64_ROM_Z64,
   N64_ROM_V64,
} n64_rom_format;


/* Collision */
typedef struct
{
   unsigned int type;
   char *name;
} terrain_t;

extern const terrain_t terrain_table[];


/* Geo */
typedef struct
{
   int length;
   const char *macro;
} geo_command;

extern geo_command geo_table[];


//================================================================================
//    Function Declarations
//================================================================================

/* Main */
void print_spaces(FILE *fp, int count);
n64_rom_format n64_rom_type(unsigned char *buf, unsigned int length);
void gzip_decode_file(char *gzfilename, int offset, char *binfilename);
int config_section_lookup(rom_config *config, unsigned int addr, char *label, int is_end);
void write_level(FILE *out, unsigned char *data, rom_config *config, int s, disasm_state *state);

void generate_globals(arg_config *args, rom_config *config);
void generate_macros(arg_config *args);
void generate_ld_script(arg_config *args, rom_config *config);

void section_sm64_geo(unsigned char *data, arg_config *args, rom_config *config,
                      disasm_state *state, split_section *sec, char* start_label,
                      char* outfilename, char* outfilepath, FILE *fasm, strbuf *makeheader_level);

void write_bin_type(split_section *sec, char* outfilename, char* start_label, FILE* fasm,
                    unsigned char *data, char* outfilepath, arg_config * args, rom_config *config);

void split_file(unsigned char *data, unsigned int length, arg_config *args, rom_config *config, disasm_state *state);

void print_usage(void);
void print_version(void);
void parse_arguments(int argc, char *argv[], arg_config *config);
int detect_config_file(unsigned int c1, unsigned int c2, rom_config *config);
int main(int argc, char *argv[]);


/* Behavior */
void write_behavior(FILE *out, unsigned char *data, rom_config *config, int s, disasm_state *state);


/* Collision */
char *terrain2str(unsigned int type);
int collision2obj(char *binfilename, unsigned int binoffset, char *objfilename, char *name, float scale);


/* Geo */
void write_geolayout(FILE *out, unsigned char *data, unsigned int start, unsigned int end, disasm_state *state);
void generate_geo_macros(arg_config *args);


/* Sound */
void parse_music_sequences(FILE *out, unsigned char *data, split_section *sec, arg_config *args, strbuf *makeheader);
void parse_instrument_set(FILE *out, unsigned char *data, split_section *sec);
void parse_sound_banks(FILE *out, unsigned char *data, split_section *secCtl, split_section *secTbl, arg_config *args, strbuf *makeheader);
