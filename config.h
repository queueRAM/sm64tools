#ifndef CONFIG_H_
#define CONFIG_H_

typedef enum
{
   TYPE_INVALID,
   TYPE_ASM,
   TYPE_BEHAVIOR,
   TYPE_BIN,
   TYPE_HEADER,
   TYPE_LA,
   TYPE_LEVEL,
   TYPE_MIO0,
   TYPE_PTR,
} section_type;

typedef struct _label
{
   unsigned int ram_addr;
   char name[128];
} label;

typedef struct _split_section
{
   char label[128];
   unsigned int start;
   unsigned int end;
   section_type type;
   void *extra;
   int extra_len;
} split_section;

typedef enum
{
   FORMAT_INVALID,
   FORMAT_RGBA,
   FORMAT_IA,
   FORMAT_SKYBOX,
} texture_format;

typedef struct _texture
{
   unsigned int offset;
   unsigned short width;
   unsigned short height;
   unsigned short depth;
   texture_format format;
} texture;

typedef struct _rom_config
{
   char *basename;
   unsigned int ram_offset;

   unsigned int *ram_table;
   int ram_count;

   split_section *sections;
   int section_count;

   label *labels;
   int label_count;
} rom_config;

int parse_config_file(const char *filename, rom_config *config);
void print_config(const rom_config *config);
int validate_config(const rom_config *config, unsigned int max_len);

#endif // CONFIG_H_
