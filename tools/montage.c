#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../utils.h"

#define DEFAULT_CONFIG "../configs/sm64.u.config"

typedef struct
{
   unsigned int rom;
   unsigned int ext;
} sm64_offset;

// extended ROM mapping based on output from sm64extend
static const sm64_offset mapping[] =
{
  {0x00108A40, 0x00803156}, // 0x00800000}, // 0081BB64 as raw data with a MIO0 header
  {0x00114750, 0x00823B64}, // 00858EDC as raw data
  {0x0012A7E0, 0x00860EDC}, // 0087623C as raw data
  {0x00132C60, 0x0087E23C}, // 008843BC as raw data
  {0x00134D20, 0x0088C3BC}, // 0089D45C as raw data
  {0x0013B910, 0x008A545C}, // 008B918C as raw data
  {0x00145E90, 0x008C118C}, // 008D57DC as raw data
  {0x001521D0, 0x008DD7DC}, // 008F3894 as raw data
  {0x00160670, 0x008FB894}, // 009089C4 as raw data
  {0x00165A50, 0x009109C4}, // 00913E8C as raw data
  {0x00166C60, 0x0091BE8C}, // 0092C004 as raw data
  {0x0016D870, 0x00934004}, // 00958204 as raw data
  {0x00180BB0, 0x00960204}, // 009770C4 as raw data
  {0x00188440, 0x0097F0C4}, // 009E1FD4 as raw data
  {0x001B9CC0, 0x009E9FD4}, // 00A01934 as raw data
  {0x001C4230, 0x00A09934}, // 00A2EABC as raw data
  {0x001D8310, 0x00A36ABC}, // 00A4E934 as raw data
  {0x001E51F0, 0x00A56934}, // 00A5C7AC as raw data
  {0x001E7EE0, 0x00A647AC}, // 00A7981C as raw data
  {0x001F2200, 0x00A8181C}, // 00AAA40C as raw data
  {0x00201410, 0x00AB240C}, // 00AE5714 as raw data
  {0x0026A3A0, 0x00AED714}, // 00AFA054 as raw data
  {0x0026F420, 0x00B02054}, // 00B085FC as raw data
  {0x002708C0, 0x00B112CD}, // 0x00B105FC}, // 00B178B5 as raw data with a MIO0 header
  {0x002A65B0, 0x00B1F8B5}, // 00B2D715 as raw data
  {0x002AC6B0, 0x00B35715}, // 00B55855 as raw data
  {0x002B8F10, 0x00B5D855}, // 00B7D995 as raw data
  {0x002C73D0, 0x00B85995}, // 00B9A2D5 as raw data
  {0x002D0040, 0x00BA22D5}, // 00BBAC15 as raw data
  {0x002D64F0, 0x00BC2C15}, // 00BE2D55 as raw data
  {0x002E7880, 0x00BEAD55}, // 00C0AE95 as raw data
  {0x002F14E0, 0x00C12E95}, // 00C32FD5 as raw data
  {0x002FB1B0, 0x00C3AFD5}, // 00C4F915 as raw data
  {0x00301CD0, 0x00C57915}, // 00C77A55 as raw data
  {0x0030CEC0, 0x00C7FA55}, // 00C9FB95 as raw data
  {0x0031E1D0, 0x00CA93A9}, // 0x00CA7B95}, // 00CB53A9 as raw data with a MIO0 header
  {0x00326E40, 0x00CBECBD}, // 0x00CBD3A9}, // 00CCB4BD as raw data with a MIO0 header
  {0x0032D070, 0x00CD4BD1}, // 0x00CD34BD}, // 00CE03D1 as raw data with a MIO0 header
  {0x00334B30, 0x00CE9CE5}, // 0x00CE83D1}, // 00CF64E5 as raw data with a MIO0 header
  {0x0033D710, 0x00CFF5F9}, // 0x00CFE4E5}, // 00D07DF9 as raw data with a MIO0 header
  {0x00341140, 0x00D1120D}, // 0x00D0FDF9}, // 00D1B20D as raw data with a MIO0 header
  {0x00347A50, 0x00D24B21}, // 0x00D2320D}, // 00D31321 as raw data with a MIO0 header
  {0x0034E760, 0x00D3A4B5}, // 0x00D39321}, // 00D430B5 as raw data with a MIO0 header
  {0x00351960, 0x00D4C9C9}, // 0x00D4B0B5}, // 00D591C9 as raw data with a MIO0 header
  {0x00357350, 0x00D629DD}, // 0x00D611C9}, // 00D6E9DD as raw data with a MIO0 header
  {0x0035ED10, 0x00D78271}, // 0x00D769DD}, // 00D84671 as raw data with a MIO0 header
  {0x00365980, 0x00D8DF85}, // 0x00D8C671}, // 00D9A785 as raw data with a MIO0 header
  {0x0036F530, 0x00DA2785}, // 00DA951D as raw data
  {0x00371C40, 0x00DB151D}, // 00DD8361 as raw data
  {0x00383950, 0x00DE0361}, // 00E03B07 as raw data
  {0x00396340, 0x00E0BB07}, // 00E84C1F as raw data
  {0x003D0DC0, 0x00E8CC1F}, // 00EB8587 as raw data
  {0x003E76B0, 0x00EC0587}, // 00EE8E37 as raw data
  {0x003FC2B0, 0x00EF0E37}, // 00F025F9 as raw data
  {0x00405FB0, 0x00F0A5F9}, // 00F1A081 as raw data
  {0x0040ED70, 0x00F22081}, // 00F3A809 as raw data
  {0x0041A760, 0x00F42809}, // 00F53BB5 as raw data
  {0x004246D0, 0x00F5BBB5}, // 00F69F71 as raw data
  {0x0042CF20, 0x00F71F71}, // 00F88991 as raw data
  {0x00437870, 0x00F90991}, // 00FBF807 as raw data
  {0x0044ABC0, 0x00FC7807}, // 00FD907F as raw data
  {0x00454E00, 0x00FE107F}, // 00FF0EAF as raw data
  {0x0045C600, 0x00FF8EAF}, // 01003B77 as raw data
  {0x004614D0, 0x0100BB77}, // 0102177F as raw data
  {0x0046B090, 0x0102977F}, // 0102CAAF as raw data
  {0x0046C3A0, 0x01034AAF}, // 010502A3 as raw data
  {0x004784A0, 0x010582A3}, // 01080B73 as raw data
  {0x0048D930, 0x01088B73}, // 01098883 as raw data
  {0x00496090, 0x010A0883}, // 010B269B as raw data
  {0x0049E710, 0x010BA69B}, // 010E19EB as raw data
  {0x004AC570, 0x010E99EB}, // 010F0867 as raw data
  {0x004AF930, 0x010F8867}, // 01109903 as raw data
  {0x004B80D0, 0x01111903}, // 0111D8AB as raw data
  {0x004BEC30, 0x011258AB}, // 0112E271 as raw data
  {0x004C2920, 0x01136271}, // 01138D39 as raw data
  {0x004C4320, 0x01140D39}, // 011544E7 as raw data
  {0x004CDBD0, 0x0115C4E7}, // 0115E087 as raw data
  {0x004CEC00, 0x01166087}, // 0116B143 as raw data
  {0x004D1910, 0x01173143}, // 011A35B7 as raw data
};

static unsigned int map(unsigned int rom)
{
   unsigned i;
   for (i = 0; i < DIM(mapping); i++) {
      if (mapping[i].rom == rom) {
         return mapping[i].ext;
      }
   }
   return 0;
}

int main(int argc, char *argv[])
{
   rom_config config;
   FILE *out;
   int i;
   unsigned int w, h;
   unsigned int offset;

   // load defaults and parse arguments
   out = stdout;

   // parse config file
   INFO("Parsing config file '%s'\n", DEFAULT_CONFIG);
   if (parse_config_file(DEFAULT_CONFIG, &config)) {
      ERROR("Error parsing config file '%s'\n", DEFAULT_CONFIG);
      return 1;
   }

   fprintf(out, "mkdir -p montages\n\n");

   for (i = 0; i < config.section_count; i++) {
      split_section *sec = &config.sections[i];
      if (sec->type == TYPE_MIO0) {
         if (sec->extra) {
            texture *texts = sec->extra;
            int t;
            fprintf(out, "montage \\\n");
            for (t = 0; t < sec->extra_len; t++) {
               char name[256];
               char format[8];
               w = texts[t].width;
               h = texts[t].height;
               offset = texts[t].offset;
               switch (texts[t].format) {
                  case FORMAT_IA:
                     sprintf(name, "%s.0x%05X.ia%d.png", sec->label, offset, texts[t].depth);
                     sprintf(format, "ia%d", texts[t].depth);
                     break;
                  case FORMAT_RGBA:
                     sprintf(name, "%s.0x%05X.png", sec->label, offset);
                     sprintf(format, "rgba");
                     break;
                  case FORMAT_SKYBOX:
                     sprintf(name, "%s.0x%05X.skybox.png", sec->label, offset);
                     sprintf(format, "sky");
                     break;
                  default:
                     continue;
               }
               fprintf(out, "        -label '%05X\\n%s\\n%dx%d' ../gen/textures/%s \\\n", offset, format, w, h, name);
            }
            int x = MIN(10, sec->extra_len);
            int y = MAX(1, (sec->extra_len + 9) / 10);
            fprintf(out, "        -tile %dx%d -background none -geometry '64x64+2+2>' montages/%s.png\n\n",
                    x, y, sec->label);
            fprintf(out, "pngcrush -ow montages/%s.png\n\n", sec->label);
         }
      }
   }

   fprintf(out, "cat << EOF > index.html\n");
   fprintf(out, "<html>\n"
                "<head>\n"
                "<title>%s Textures</title></head>\n", config.name);
   fprintf(out,
"<style type=\"text/css\">\n"
"table {border-spacing: 0; }\n"
"table {\n"
"  font-family:Arial, Helvetica, sans-serif;\n"
"  color:#333;\n"
"  font-size:12px;\n"
"  text-shadow: 1px 1px 0px #ccc;\n"
"  background:#eaebec;\n"
"  margin:20px;\n"
"  border:#ccc 1px solid;\n"
"\n"
"  -moz-border-radius:3px;\n"
"  -webkit-border-radius:3px;\n"
"  border-radius:3px;\n"
"\n"
"  -moz-box-shadow: 0 1px 2px #d1d1d1;\n"
"  -webkit-box-shadow: 0 1px 2px #d1d1d1;\n"
"  box-shadow: 0 1px 2px #d1d1d1;\n"
"}\n"
"table th {\n"
"  padding:21px 25px 22px 25px;\n"
"  border-top:1px solid #fafafa;\n"
"  border-bottom:1px solid #aaa;\n"
"  background: #ededed;\n"
"}\n"
"table tr:first-child th:first-child {\n"
"  -moz-border-radius-topleft:3px;\n"
"  -webkit-border-top-left-radius:3px;\n"
"  border-top-left-radius:3px;\n"
"}\n"
"table tr:first-child th:last-child {\n"
"  -moz-border-radius-topright:3px;\n"
"  -webkit-border-top-right-radius:3px;\n"
"  border-top-right-radius:3px;\n"
"}\n"
"table tr {\n"
"  text-align: center;\n"
"}\n"
"table tr td {\n"
"  vertical-align: top;\n"
"  padding: 18px;\n"
"  border-bottom: 1px solid #aaa;\n"
"  border-left: 1px solid #aaa;\n"
"  background: #ccc;\n"
"}\n"
"table tr:hover td {\n"
"  background: #fff;\n"
"}\n"
"table tr:last-child td:first-child {\n"
"   -moz-border-radius-bottomleft:3px;\n"
"   -webkit-border-bottom-left-radius:3px;\n"
"   border-bottom-left-radius:3px;\n"
"}\n"
"table tr:last-child td:last-child {\n"
"  -moz-border-radius-bottomright:3px;\n"
"  -webkit-border-bottom-right-radius:3px;\n"
"  border-bottom-right-radius:3px;\n"
"}\n"
"</style>\n");
   fprintf(out, "<body>\n");
   fprintf(out, "<center>\n");
   fprintf(out, "<table>\n");
   fprintf(out, "<tr><th>ROM MIO0</th><th>Extended ROM</th><th>Textures and offset in block</th></tr>\n");
   for (i = 0; i < config.section_count; i++) {
      split_section *sec = &config.sections[i];
      if (sec->type == TYPE_MIO0) {
         if (sec->extra) {
            fprintf(out, "<tr><td>%X</td><td>%X</td><td><img src=\"montages/%s.png\"></td></tr>\n",
                    sec->start, map(sec->start), sec->label);
         }
      }
   }
   fprintf(out, "</table>\n");
   fprintf(out, "</center>\n");
   fprintf(out, "</body>\n");
   fprintf(out, "</html>\n");
   fprintf(out, "EOF\n");

   return 0;
}

