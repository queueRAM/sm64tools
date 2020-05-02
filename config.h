#ifndef CONFIG_H_
#define CONFIG_H_

typedef enum
{
   TYPE_INVALID,
   TYPE_ASM,
   TYPE_BIN,
   TYPE_BLAST,
   TYPE_GZIP,
   TYPE_HEADER,
   TYPE_INSTRUMENT_SET,
   TYPE_M64,
   TYPE_SFX_CTL,
   TYPE_SFX_TBL,
   TYPE_MIO0,
   TYPE_PTR,
   // F3D display lists and related
   TYPE_F3D_DL,
   TYPE_F3D_LIGHT,
   TYPE_F3D_VERTEX,
   // Textures
   TYPE_TEX_CI,
   TYPE_TEX_I,
   TYPE_TEX_IA,
   TYPE_TEX_RGBA,
   TYPE_TEX_SKYBOX,
   // SM64 specific types
   TYPE_SM64_GEO,
   TYPE_SM64_BEHAVIOR,
   TYPE_SM64_COLLISION,
   TYPE_SM64_LEVEL,
} section_type;

typedef struct _label
{
   unsigned int ram_addr;
   char name[2048];
} label;

typedef struct _texture
{
   unsigned int offset;
   unsigned int palette; // only for CI textures
   unsigned short width;
   unsigned short height;
   unsigned short depth;
   section_type format;
} texture;

typedef struct _split_section
{
   char label[512];
   unsigned int start;
   unsigned int end;
   unsigned int vaddr;
   section_type type;
   char section_name[512];

   int subtype;

   // texture specific data
   texture tex;

   struct _split_section *children;
   int child_count;
} split_section;

typedef struct _rom_config
{
   char name[128];
   char basename[128];

   unsigned int checksum1;
   unsigned int checksum2;

   split_section *sections;
   int section_count;

   label *labels;
   int label_count;
} rom_config;

int config_parse_file(const char *filename, rom_config *config);
void config_print(const rom_config *config);
int config_validate(const rom_config *config, unsigned int max_len);
void config_free(rom_config *config);

section_type config_str2section(const char *type_name);
const char *config_section2str(section_type section);

// get version of underlying config library
const char *config_get_version(void);

#endif // CONFIG_H_
