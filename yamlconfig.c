#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yaml.h>

#include "config.h"
#include "utils.h"

#define MAX_SIZE 2048
typedef struct
{
   const char *name;
   const section_type section;
} section_entry;

static const section_entry section_table[] = {
   {"asm",        TYPE_ASM},
   {"bin",        TYPE_BIN},
   {"blast",      TYPE_BLAST},
   {"gzip",       TYPE_GZIP},
   {"header",     TYPE_HEADER},
   {"instrset",   TYPE_INSTRUMENT_SET},
   {"m64",        TYPE_M64},
   {"sfx.ctl",    TYPE_SFX_CTL},
   {"sfx.tbl",    TYPE_SFX_TBL},
   {"mio0",       TYPE_MIO0},
   {"ptr",        TYPE_PTR},
   // F3D formats
   {"f3d.dl",     TYPE_F3D_DL},
   {"f3d.light",  TYPE_F3D_LIGHT},
   {"f3d.vertex", TYPE_F3D_VERTEX},
   // Texture formats
   {"tex.ci",     TYPE_TEX_CI},
   {"tex.i",      TYPE_TEX_I},
   {"tex.ia",     TYPE_TEX_IA},
   {"tex.rgba",   TYPE_TEX_RGBA},
   {"tex.skybox", TYPE_TEX_SKYBOX},
   // SM64 specific sections
   {"sm64.behavior",  TYPE_SM64_BEHAVIOR},
   {"sm64.collision", TYPE_SM64_COLLISION},
   {"sm64.geo",       TYPE_SM64_GEO},
   {"sm64.level",     TYPE_SM64_LEVEL},
};
// TODO would be cool to extend this dynamically with plugins

static inline int is_texture(section_type section)
{
   switch (section) {
      case TYPE_TEX_CI:
      case TYPE_TEX_I:
      case TYPE_TEX_IA:
      case TYPE_TEX_RGBA:
      case TYPE_TEX_SKYBOX:
         return 1;
      default:
         return 0;
   }
}

section_type config_str2section(const char *type_name)
{
   if (type_name != NULL) {
      for (unsigned i = 0; i < DIM(section_table); i++) {
         if (0 == strcmp(section_table[i].name, type_name)) {
            return section_table[i].section;
         }
      }
   }
   return TYPE_INVALID;
}

const char *config_section2str(section_type section)
{
   for (unsigned int i = 0; i < DIM(section_table); i++) {
      if (section == section_table[i].section) {
         return section_table[i].name;
      }
   }
   return "";
}

int get_scalar_value(char *scalar, yaml_node_t *node)
{
   if (node->type == YAML_SCALAR_NODE) {
      memcpy(scalar, node->data.scalar.value, node->data.scalar.length);
      scalar[node->data.scalar.length] = '\0';
      return node->data.scalar.length;
   } else {
      return -1;
   }
}

int get_scalar_uint(unsigned int *val, yaml_node_t *node)
{
   char value[32];
   int status = get_scalar_value(value, node);
   if (status > 0) {
      *val = strtoul(value, NULL, 0);
   }
   return status;
}

void load_child_node(split_section *section, yaml_document_t *doc, yaml_node_t *node)
{
   char val[MAX_SIZE];
   yaml_node_item_t *i_node;
   yaml_node_t *next_node;
   texture *tex = &section->tex;
   size_t count = node->data.sequence.items.top - node->data.sequence.items.start;
   i_node = node->data.sequence.items.start;
   for (size_t i = 0; i < count; i++) {
      next_node = yaml_document_get_node(doc, i_node[i]);
      if (next_node && next_node->type == YAML_SCALAR_NODE) {
         get_scalar_value(val, next_node);
         switch (i) {
            case 0:
               section->start = tex->offset = strtoul(val, NULL, 0);
               break;
            case 1:
               tex->format = config_str2section(val);
               if (tex->format == TYPE_INVALID) {
                  ERROR("Error: " SIZE_T_FORMAT " - invalid texture format '%s'\n", node->start_mark.line, val);
                  return;
               }
               break;
            case 2:
            {
               if (is_texture(tex->format)) {
                  tex->depth = strtoul(val, NULL, 0);
               } else {
                  section->end = strtoul(val, NULL, 0);
               }
               break;
            }
            case 3: tex->width = strtoul(val, NULL, 0); break;
            case 4: tex->height = strtoul(val, NULL, 0); break;
            case 5:
               if (tex->format != TYPE_TEX_CI) {
                  ERROR("Error: expected 5 fields for non-CI texture\n");
               } else {
                  tex->palette = strtoul(val, NULL, 0); break;
               }
               break;
         }
      } else {
         ERROR("Error: non-scalar value in texture sequence\n");
      }
   }
}

void load_behavior(split_section *beh, yaml_document_t *doc, yaml_node_t *node)
{
   char val[64];
   yaml_node_item_t *i_node;
   yaml_node_t *next_node;
   size_t count = node->data.sequence.items.top - node->data.sequence.items.start;
   i_node =  node->data.sequence.items.start;
   if (count != 2) {
      ERROR("Error: " SIZE_T_FORMAT " - expected 2 fields for behavior, got " SIZE_T_FORMAT "\n", node->start_mark.line, count);
      return;
   }
   for (size_t i = 0; i < count; i++) {
      next_node = yaml_document_get_node(doc, i_node[i]);
      if (next_node && next_node->type == YAML_SCALAR_NODE) {
         get_scalar_value(val, next_node);
         switch (i) {
            case 0: beh->start = strtoul(val, NULL, 0); break;
            case 1: strcpy(beh->label, val); break;
         }
      } else {
         ERROR("Error: non-scalar value in behavior sequence\n");
      }
   }
}

void load_section_data(split_section *section, yaml_document_t *doc, yaml_node_t *node)
{
   yaml_node_item_t *i_node;
   yaml_node_t *next_node;
   size_t count = node->data.sequence.items.top - node->data.sequence.items.start;
   i_node = node->data.sequence.items.start;
   switch (section->type) {
      case TYPE_BLAST:
      case TYPE_MIO0:
      case TYPE_GZIP:
      {
         // parse child nodes
         split_section *children = calloc(count, sizeof(*children));
         for (size_t i = 0; i < count; i++) {
            next_node = yaml_document_get_node(doc, i_node[i]);
            if (next_node->type == YAML_SEQUENCE_NODE) {
               load_child_node(&children[i], doc, next_node);
            } else {
               ERROR("Error: non-sequence child node\n");
               return;
            }
         }
         section->children = children;
         section->child_count = count;
         break;
      }
      case TYPE_SM64_BEHAVIOR:
      {
         // parse behavior
         split_section *beh = calloc(count, sizeof(*beh));
         for (size_t i = 0; i < count; i++) {
            next_node = yaml_document_get_node(doc, i_node[i]);
            if (next_node->type == YAML_SEQUENCE_NODE) {
               load_behavior(&beh[i], doc, next_node);
            } else {
               ERROR("Error: non-sequence behavior node\n");
               return;
            }
         }
         section->children = beh;
         section->child_count = count;
         break;
      }
      default:
         ERROR("Warning: " SIZE_T_FORMAT " - extra fields for section\n", node->start_mark.line);
         break;
   }
}

void load_section(split_section *section, yaml_document_t *doc, yaml_node_t *node)
{
   char val[MAX_SIZE];
   yaml_node_item_t *i_node;
   yaml_node_t *next_node;
   size_t count = node->data.sequence.items.top - node->data.sequence.items.start;
   if (count >= 3) {
      i_node = node->data.sequence.items.start;
      for (int i = 0; i < 3; i++) {
         next_node = yaml_document_get_node(doc, i_node[i]);
         if (next_node && next_node->type == YAML_SCALAR_NODE) {
            get_scalar_value(val, next_node);
            switch (i) {
               case 0: section->start = strtoul(val, NULL, 0); break;
               case 1: section->end = strtoul(val, NULL, 0); break;
               case 2: 
                  section->type = config_str2section(val); 
                  strcpy(section->section_name, val);
                  break;
               case 3: section->vaddr = strtoul(val, NULL, 0); break;
            }
         } else {
            ERROR("Error: non-scalar value in section sequence\n");
         }
      }
      section->children = NULL;
      section->child_count = 0;
      // validate section parameter counts
      switch (section->type) {
         case TYPE_ASM:
         case TYPE_BIN:
         case TYPE_HEADER:
         case TYPE_M64:
         case TYPE_SM64_GEO:
         case TYPE_SM64_LEVEL:
         case TYPE_SFX_CTL:
         case TYPE_SFX_TBL:
            if (count > 5) {
               ERROR("Error: " SIZE_T_FORMAT " - expected 3-5 fields for section\n", node->start_mark.line);
               return;
            }
            break;
         case TYPE_INSTRUMENT_SET:
         case TYPE_PTR:
            if (count > 5) {
               ERROR("Error: " SIZE_T_FORMAT " - expected 3-5 fields for section\n", node->start_mark.line);
               return;
            }
            break;
         case TYPE_BLAST:
         case TYPE_MIO0:
         case TYPE_GZIP:
         case TYPE_SM64_BEHAVIOR:
            if (count < 4 || count > 5) {
               ERROR("Error: " SIZE_T_FORMAT " - expected 4-5 fields for section\n", node->start_mark.line);
               return;
            }
            break;
         default:
            // ERROR("Error: " SIZE_T_FORMAT " - invalid section type '%s'\n", node->start_mark.line, val);
            return;
      }
      if (count > 3) {
         next_node = yaml_document_get_node(doc, i_node[3]);
         if (next_node && next_node->type == YAML_SCALAR_NODE) {
            get_scalar_value(val, next_node);
            // most are label names, blast is subtype
            switch (section->type) {
               case TYPE_BLAST:
                  section->subtype = strtoul(val, NULL, 0);
                  break;
               default:
                  strcpy(section->label, val);
                  break;
            }
         } else {
            ERROR("Error: non-scalar value in section sequence\n");
         }
      } else {
         section->label[0] = '\0';
      }
      // extra parameters for some types
      if (count > 4) {
         next_node = yaml_document_get_node(doc, i_node[4]);
         if (next_node && next_node->type == YAML_SCALAR_NODE) {
            get_scalar_value(val, next_node);
            switch (section->type) {
               case TYPE_ASM:
               case TYPE_MIO0:
                  section->vaddr = strtoul(val, NULL, 0);
                  break;
               case TYPE_PTR:
               case TYPE_INSTRUMENT_SET:
                  section->child_count = strtoul(val, NULL, 0);
                  break;
               default:
                  ERROR("Warning: " SIZE_T_FORMAT " - extra fields for section\n", node->start_mark.line);
                  break;
            }
         } else {
            ERROR("Error: non-scalar value in section sequence\n");
         }
      }
   } else {
      ERROR("Error: section sequence needs 3-5 scalars (got " SIZE_T_FORMAT ")\n", count);
   }
}

int load_sections_sequence(rom_config *c, yaml_document_t *doc, yaml_node_t *node)
{
   yaml_node_t *next_node;
   yaml_node_t *key_node;
   yaml_node_t *val_node;
   int ret_val = -1;
   if (node->type == YAML_SEQUENCE_NODE) {
      size_t count = node->data.sequence.items.top - node->data.sequence.items.start;
      c->sections = calloc(count, sizeof(*c->sections));
      c->section_count = 0;
      yaml_node_item_t *i_node = node->data.sequence.items.start;
      for (size_t i = 0; i < count; i++) {
         next_node = yaml_document_get_node(doc, i_node[i]);
         if (next_node) {
            if (next_node->type == YAML_SEQUENCE_NODE) {
               load_section(&c->sections[c->section_count], doc, next_node);
               c->section_count++;
            } else if (next_node->type == YAML_MAPPING_NODE) {
               yaml_node_pair_t *i_node_p;
               for (i_node_p = next_node->data.mapping.pairs.start; i_node_p < next_node->data.mapping.pairs.top; i_node_p++) {
                  // the key node is a sequence
                  key_node = yaml_document_get_node(doc, i_node_p->key);
                  val_node = yaml_document_get_node(doc, i_node_p->value);
                  if (key_node && val_node && key_node->type == YAML_SEQUENCE_NODE && val_node->type == YAML_SEQUENCE_NODE) {
                     load_section(&c->sections[c->section_count], doc, key_node);
                     load_section_data(&c->sections[c->section_count], doc, val_node);
                     c->section_count++;
                  } else {
                     ERROR("ERROR: sections sequence map is not sequence\n");
                  }
               }
            } else {
               ERROR("Error: sections sequence contains non-sequence and non-mapping\n");
            }
         }
      }
      ret_val = 0;
   }
   return ret_val;
}

void load_label(label *lab, yaml_document_t *doc, yaml_node_t *node)
{
   char val[MAX_SIZE];
   yaml_node_item_t *i_node;
   yaml_node_t *next_node;
   if (node->type == YAML_SEQUENCE_NODE) {
      size_t count = node->data.sequence.items.top - node->data.sequence.items.start;
      if (count >= 2 && count <= 3) {
         i_node = node->data.sequence.items.start;
         for (size_t i = 0; i < count; i++) {
            next_node = yaml_document_get_node(doc, i_node[i]);
            if (next_node && next_node->type == YAML_SCALAR_NODE) {
               get_scalar_value(val, next_node);
               switch (i) {
                  case 0: lab->ram_addr = strtoul(val, NULL, 0); break;
                  case 1: strcpy(lab->name, val); break;
               }
            } else {
               ERROR("Error: non-scalar value in label sequence\n");
            }
         }
      } else {
         ERROR("Error: label sequence needs 2-3 scalars\n");
      }
   }
}

int load_labels_sequence(rom_config *c, yaml_document_t *doc, yaml_node_t *node)
{
   yaml_node_t *next_node;
   int ret_val = -1;
   if (node->type == YAML_SEQUENCE_NODE) {
      size_t count = node->data.sequence.items.top - node->data.sequence.items.start;
      c->labels = calloc(count, sizeof(*c->labels));
      c->label_count = 0;
      yaml_node_item_t *i_node = node->data.sequence.items.start;
      for (size_t i = 0; i < count; i++) {
         next_node = yaml_document_get_node(doc, i_node[i]);
         if (next_node && next_node->type == YAML_SEQUENCE_NODE) {
            load_label(&c->labels[c->label_count], doc, next_node);
            c->label_count++;
         } else {
            ERROR("Error: non-sequence in labels sequence\n");
         }
      }
      ret_val = 0;
   }
   return ret_val;
}

void parse_yaml_root(yaml_document_t *doc, yaml_node_t *node, rom_config *c)
{
   char key[128];
   yaml_node_pair_t *i_node_p;

   yaml_node_t *key_node;
   yaml_node_t *val_node;

   // only accept mapping nodes at root level
   switch (node->type) {
      case YAML_MAPPING_NODE:
         for (i_node_p = node->data.mapping.pairs.start; i_node_p < node->data.mapping.pairs.top; i_node_p++) {
            key_node = yaml_document_get_node(doc, i_node_p->key);
            if (key_node) {
               get_scalar_value(key, key_node);
               val_node = yaml_document_get_node(doc, i_node_p->value);
               if (!strcmp(key, "name")) {
                  get_scalar_value(c->name, val_node);
                  INFO("config.name: %s\n", c->name);
               } else if (!strcmp(key, "basename")) {
                  get_scalar_value(c->basename, val_node);
                  INFO("config.basename: %s\n", c->basename);
               } else if (!strcmp(key, "checksum1")) {
                  unsigned int val;
                  get_scalar_uint(&val, val_node);
                  c->checksum1 = (unsigned int)val;
               } else if (!strcmp(key, "checksum2")) {
                  unsigned int val;
                  get_scalar_uint(&val, val_node);
                  c->checksum2 = (unsigned int)val;
               } else if (!strcmp(key, "ranges")) {
                  load_sections_sequence(c, doc, val_node);
               } else if (!strcmp(key, "labels")) {
                  load_labels_sequence(c, doc, val_node);
               }
            } else {
               ERROR("Couldn't find next node\n");
            }
         }
         break;
      default:
         ERROR("Error: " SIZE_T_FORMAT " only mapping node supported at root level\n", node->start_mark.line);
         break;
   }
}

int config_parse_file(const char *filename, rom_config *c)
{
   yaml_parser_t parser;
   yaml_document_t doc;
   yaml_node_t *root;
   FILE *file;

   c->name[0] = '\0';
   c->basename[0] = '\0';
   c->section_count = 0;
   c->label_count = 0;

   // read config file, exit if problem
   file = fopen(filename, "rb");
   if (!file) {
      ERROR("Error: cannot open %s\n", filename);
      return -1;
   }
   yaml_parser_initialize(&parser);
   yaml_parser_set_input_file(&parser, file);

   if (!yaml_parser_load(&parser, &doc)) {
      ERROR("Error: problem parsing YAML %s:" SIZE_T_FORMAT " %s (%d)\n", filename, parser.problem_offset, parser.problem, (int)parser.error);
   }

   root = yaml_document_get_root_node(&doc);
   if (root) {
      parse_yaml_root(&doc, root, c);
   }

   yaml_document_delete(&doc);
   yaml_parser_delete(&parser);

   fclose(file);

   return 0;
}

void config_free(rom_config *config)
{
   if (config) {
      if (config->sections) {
         for (int i = 0; i < config->section_count; i++) {
            switch (config->sections[i].type) {
               case TYPE_BLAST:
               case TYPE_GZIP:
               case TYPE_MIO0:
               case TYPE_SM64_BEHAVIOR:
                  if (config->sections[i].child_count) {
                     free(config->sections[i].children);
                     config->sections[i].children = NULL;
                     config->sections[i].child_count = 0;
                  }
                  break;
               default:
                  break;
            }
         }
         free(config->sections);
         config->sections = NULL;
         config->section_count = 0;
      }
      if (config->labels) {
         free(config->labels);
         config->labels = NULL;
         config->label_count = 0;
      }
   }
}

void config_print(const rom_config *config)
{
   int i, j;
   split_section *s = config->sections;
   label *l = config->labels;

   printf("name: %s\n", config->name);
   printf("basename: %s\n", config->basename);
   printf("checksum1: %08X\n", config->checksum1);
   printf("checksum2: %08X\n\n", config->checksum2);

   // ranges
   printf("ranges:\n");
   for (i = 0; i < config->section_count; i++) {
      printf("0x%08X 0x%08X 0x%08X %d %s %d\n", s[i].start, s[i].end, s[i].vaddr, s[i].type, s[i].label, s[i].child_count);
      if (s[i].child_count > 0) {
         switch (s[i].type) {
            case TYPE_BLAST:
            case TYPE_MIO0:
            case TYPE_GZIP:
            {
               split_section *textures = s[i].children;
               for (j = 0; j < s[i].child_count; j++) {
                  texture *tex = &textures[j].tex;
                  printf("  0x%06X %d", tex->offset, tex->format);
                  switch (tex[j].format) {
                     case TYPE_TEX_CI:
                     case TYPE_TEX_I:
                     case TYPE_TEX_IA:
                     case TYPE_TEX_RGBA:
                     case TYPE_TEX_SKYBOX:
                        printf(" %2d %3d %3d", tex->depth, tex->width, tex->height);
                        break;
                     default:
                        break;
                  }
                  printf("\n");
               }
               break;
            }
            case TYPE_SM64_BEHAVIOR:
            {
               split_section *beh = s[i].children;
               for (j = 0; j < s[i].child_count; j++) {
                  printf("  0x%06X %s\n", beh[j].start, beh[j].label);
               }
               break;
            }
            default:
               break;
         }
      }
   }
   printf("\n");

   // labels
   printf("labels:\n");
   for (i = 0; i < config->label_count; i++) {
      printf("0x%08X: %s\n", l[i].ram_addr, l[i].name);
   }
}

int config_validate(const rom_config *config, unsigned int max_len)
{
   // error on overlapped and out-of-order sections
   int i, j, beh_i;
   unsigned int last_end = 0;
   int ret_val = 0;
   for (i = 0; i < config->section_count; i++) {
      split_section *isec = &config->sections[i];
      if (isec->start < last_end) {
         // ERROR("Error: section %d \"%s\" (%X-%X) out of order\n",
         //       i, isec->label, isec->start, isec->end);
         isec->start = last_end;
         // ret_val = -2;
      }
      if (isec->end > max_len) {
         ERROR("Error: section %d \"%s\" (%X-%X) past end of file (%X)\n",
               i, isec->label, isec->start, isec->end, max_len);
         ret_val = -3;
      }
      if (isec->start >= isec->end) {
         ERROR("Error: section %d \"%s\" (%X-%X) invalid range\n",
               i, isec->label, isec->start, isec->end);
         isec->end=isec->start;
         //ret_val = -4;
      }
      for (j = 0; j < i; j++) {
         split_section *jsec = &config->sections[j];
         if (!((isec->end <= jsec->start) || (isec->start >= jsec->end))) {
            ERROR("Error: section %d \"%s\" (%X-%X) overlaps %d \"%s\" (%X-%X)\n",
                  i, isec->label, isec->start, isec->end,
                  j, jsec->label, jsec->start, jsec->end);
            // ret_val = -1;
         }
      }
      last_end = isec->end;
   }
   // error duplicate label addresses
   for (i = 0; i < config->label_count; i++) {
      for (j = i+1; j < config->label_count; j++) {
         if (config->labels[i].ram_addr == config->labels[j].ram_addr) {
            ERROR("Error: duplicate label %X \"%s\" \"%s\"\n", config->labels[i].ram_addr,
                  config->labels[i].name, config->labels[j].name);
            ret_val = -5;
         }
         if (0 == strcmp(config->labels[i].name, config->labels[j].name)) {
            ERROR("Error: duplicate label name \"%s\" %X %X\n", config->labels[i].name,
                  config->labels[i].ram_addr, config->labels[j].ram_addr);
            ret_val = -5;
         }
      }
   }
   // error duplicate behavior addresses
   beh_i = -1;
   for (i = 0; i < config->section_count; i++) {
      split_section *isec = &config->sections[i];
      if (isec->type == TYPE_SM64_BEHAVIOR) {
         beh_i = i;
         break;
      }
   }
   if (beh_i >= 0) {
      split_section *beh = config->sections[beh_i].children;
      int count = config->sections[beh_i].child_count;
      for (i = 0; i < count; i++) {
         for (j = i+1; j < count; j++) {
            if (beh[i].start == beh[j].start) {
               ERROR("Error: duplicate behavior offset %04X \"%s\" \"%s\"\n", beh[i].start,
                     beh[i].label, beh[j].label);
               ret_val = -6;
            }
            if (0 == strcmp(beh[i].label, beh[j].label)) {
               ERROR("Error: duplicate behavior name \"%s\" %04X %04X\n", beh[i].label,
                     beh[i].start, beh[j].start);
               ret_val = -6;
            }
         }
      }
   }
   return ret_val;
}

const char *config_get_version(void)
{
   static char version[16];
   int major, minor, patch;
   yaml_get_version(&major, &minor, &patch);
   sprintf(version, "libyaml %d.%d.%d", major, minor, patch);
   return version;
}

#ifdef YAML_CONFIG_TEST
int main(int argc, char *argv[])
{
   rom_config config;
   int status;

   if (argc < 2) {
      printf("Usage: yamlconfig <file.yaml> ...\n");
      return 1;
   }

   for (int i = 1; i < argc; i++) {
      status = config_parse_file(argv[i], &config);
      if (status == 0) {
         config_print(&config);
         config_free(&config);
      } else {
         ERROR("Error parsing %s: %d\n", argv[i], status);
      }
   }

   return 0;
}
#endif
