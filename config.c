#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libconfig.h>

#include "config.h"
#include "utils.h"

static section_type str2section(const char *type_name)
{
   section_type type = TYPE_INVALID;
   if (type_name != NULL) {
      if (0 == strcmp("asm", type_name)) {
         type = TYPE_ASM;
      } else if (0 == strcmp("behavior", type_name) || 0 == strcmp("behaviour", type_name)) {
         type = TYPE_BEHAVIOR;
      } else if (0 == strcmp("bin", type_name)) {
         type = TYPE_BIN;
      } else if (0 == strcmp("geo", type_name)) {
         type = TYPE_GEO;
      } else if (0 == strcmp("header", type_name)) {
         type = TYPE_HEADER;
      } else if (0 == strcmp("level", type_name)) {
         type = TYPE_LEVEL;
      } else if (0 == strcmp("mio0", type_name)) {
         type = TYPE_MIO0;
      } else if (0 == strcmp("ptr", type_name)) {
         type = TYPE_PTR;
      }
   }
   return type;
}

static texture_format str2format(const char *format_name)
{
   texture_format format = FORMAT_INVALID;
   if (format_name != NULL) {
      if (0 == strcmp("rgba", format_name)) {
         format = FORMAT_RGBA;
      } else if (0 == strcmp("ia", format_name)) {
         format = FORMAT_IA;
      } else if (0 == strcmp("skybox", format_name)) {
         format = FORMAT_SKYBOX;
      }
   }
   return format;
}


int parse_config_file(const char *filename, rom_config *config)
{
   config_t cfg;
   config_setting_t *setting;
   const char *str;
   int count;
   int i, j;

   config_init(&cfg);

   // read config file, exit if problem
   if (!config_read_file(&cfg, filename)) {
      ERROR("Error %s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
      config_destroy(&cfg);
      return -1;
   }

   // get name
   if (config_lookup_string(&cfg, "name", &str)) {
      strcpy(config->name, str);
      INFO("config.name: %s\n", config->name);
   } else {
      ERROR("No 'name' field in config file.\n");
   }

   // get file basename
   if (config_lookup_string(&cfg, "basename", &str)) {
      strcpy(config->basename, str);
      INFO("config.basename: %s\n", config->basename);
   } else {
      strcpy(config->basename, "default");
      ERROR("No 'basename' field in config file, using default.\n");
   }

   // output memory mapping
   setting = config_lookup(&cfg, "memory");
   if (setting != NULL) {
      unsigned int *ram_table;
      count = config_setting_length(setting);
      if (count <= 0) {
         ERROR("Error: need at least one memory: %d\n", count);
         config_destroy(&cfg);
         return -1;
      }
      INFO("config.memory: %d entries\n", count);
      ram_table = malloc(3 * count * sizeof(*ram_table));
      for (i = 0; i < count; i++) {
         config_setting_t *mem = config_setting_get_elem(setting, i);
         int mem_count;
         mem_count = config_setting_length(mem);
         if (mem_count == 3) {
            for (j = 0; j < 3; j++) {
               long long val = config_setting_get_int64_elem(mem, j);
               unsigned int uval = (unsigned int)(val & 0xFFFFFFFF);
               ram_table[3*i+j] = uval;
            }
         } else {
            ERROR("Error: %s:%d - expected 3 fields for memory, got %d\n", filename, mem->line, mem_count);
            config_destroy(&cfg);
            return -1;
         }
      }
      config->ram_table = ram_table;
      config->ram_count = count;
   }

   // output ranges
   setting = config_lookup(&cfg, "ranges");
   if(setting != NULL) {
      split_section *sec;
      count = config_setting_length(setting);
      if (count <= 0) {
         ERROR("Error: need at least one range: %d\n", count);
         config_destroy(&cfg);
         return -1;
      }
      INFO("config.ranges: %d entries\n", count);
      sec = malloc(count * sizeof(*sec));

      for (i = 0; i < count; i++) {
         config_setting_t *r = config_setting_get_elem(setting, i);
         config_setting_t *e;
         int r_count;
         texture *tex;
         const char *type, *label;
         r_count = config_setting_length(r);
         int extra_count = 0;
         sec[i].label[0] = '\0';
         switch (r_count) {
            case 5:
               // parse texture
               e = config_setting_get_elem(r, 4);
               extra_count = config_setting_length(e);
               tex = malloc(extra_count * sizeof(*tex));
               for (j = 0; j < extra_count; j++) {
                  config_setting_t *t = config_setting_get_elem(e, j);
                  int tex_count = config_setting_length(t);
                  if (tex_count != 5) {
                     ERROR("Error: %s:%d - expected 5 fields for texture, got %d\n", filename, e->line, tex_count);
                     config_destroy(&cfg);
                     return -1;
                  }
                  tex[j].offset = config_setting_get_int_elem(t, 0);
                  tex[j].depth  = config_setting_get_int_elem(t, 2);
                  tex[j].width  = config_setting_get_int_elem(t, 3);
                  tex[j].height = config_setting_get_int_elem(t, 4);
                  type = config_setting_get_string_elem(t, 1);
                  tex[j].format = str2format(type);
                  if (tex[j].format == FORMAT_INVALID) {
                     ERROR("Error: %s:%d - invalid texture format '%s'\n", filename, t->line, type);
                     config_destroy(&cfg);
                     return -1;
                  }
               }
               sec[i].extra = tex;
               sec[i].extra_len = extra_count;
               // fall through - no break
            case 4:
               label = config_setting_get_string_elem(r, 3);
               strcpy(sec[i].label, label);
               // fall through - no break
            case 3:
               sec[i].start = config_setting_get_int64_elem(r, 0);
               sec[i].end   = config_setting_get_int64_elem(r, 1);
               type = config_setting_get_string_elem(r, 2);
               sec[i].type  = str2section(type);
               if (sec[i].type == TYPE_INVALID) {
                  ERROR("Error: %s:%d - invalid section type '%s'\n", filename, r->line, type);
                  config_destroy(&cfg);
                  return -1;
               }
               break;
            default:
               ERROR("Error: %s:%d - expected 3-5 fields for range\n", filename, r->line);
               config_destroy(&cfg);
               return -1;
               break;
         }
      }
      config->section_count = count;
      config->sections = sec;
   }

   // labels
   setting = config_lookup(&cfg, "labels");
   if(setting != NULL) {
      label *labels;
      count = config_setting_length(setting);
      labels = malloc(count * sizeof(*labels));
      INFO("config.labels: %d entries\n", count);

      for (i = 0; i < count; i++) {
         config_setting_t *lab = config_setting_get_elem(setting, i);
         int lab_count;
         unsigned int addr;
         const char *label;
         lab_count = config_setting_length(lab);
         if (lab_count == 2) {
            addr  = config_setting_get_int64_elem(lab, 0);
            labels[i].ram_addr = addr;
            label = config_setting_get_string_elem(lab, 1);
            strcpy(labels[i].name, label);
         } else {
            ERROR("Error: %s:%d - expected 2 fields for label\n", filename, setting->line);
            config_destroy(&cfg);
            return -1;
         }
      }
      config->labels = labels;
      config->label_count = count;
   }

   config_destroy(&cfg);
   return 0;
}

void print_config(const rom_config *config)
{
   int i;
   unsigned int *r = config->ram_table;
   split_section *s = config->sections;
   label *l = config->labels;
   // memory
   printf("%-12s %-12s %-12s\n", "start", "end", "offset");
   for (i = 0; i < config->ram_count; i+=3) {
      printf("0x%08X   0x%08X   0x%08X\n", r[i], r[i+1], r[i+2]);
   }
   printf("\n");

   // ranges
   for (i = 0; i < config->section_count; i++) {
      printf("0x%08X 0x%08X %d %s", s[i].start, s[i].end, s[i].type, s[i].label);
      if (s[i].extra_len > 0) {
         printf(" %d", s[i].extra_len);
      }
      printf("\n");
   }
   printf("\n");

   // labels
   for (i = 0; i < config->label_count; i++) {
      printf("0x%08X %s\n", l[i].ram_addr, l[i].name);
   }
}

int validate_config(const rom_config *config, unsigned int max_len)
{
   // error on overlapped and out-of-order sections
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
   // error duplicate label addresses
   for (i = 0; i < config->label_count; i++) {
      for (j = i+1; j < config->label_count; j++) {
         if (config->labels[i].ram_addr == config->labels[j].ram_addr) {
            ERROR("Error: duplication label %X \"%s\" \"%s\"\n", config->labels[i].ram_addr,
                  config->labels[i].name, config->labels[j].name);
            return -5;
         }
         if (0 == strcmp(config->labels[i].name, config->labels[j].name)) {
            ERROR("Error: duplication label name \"%s\" %X %X\n", config->labels[i].name,
                  config->labels[i].ram_addr, config->labels[j].ram_addr);
            return -5;
         }
      }
   }
   return 0;
}

