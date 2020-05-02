#include "n64split.h"

geo_command geo_table[] =
{
   /* 0x00 */ {0x08, "geo_branch_and_link"},
   /* 0x01 */ {0x04, "geo_end"},
   /* 0x02 */ {0x08, "geo_branch"},
   /* 0x03 */ {0x04, "geo_return"},
   /* 0x04 */ {0x04, "geo_open_node"},
   /* 0x05 */ {0x04, "geo_close_node"},
   /* 0x06 */ {0x04, "geo_todo_06"},
   /* 0x07 */ {0x04, "geo_update_node_flags"},
   /* 0x08 */ {0x0C, "geo_node_screen_area"},
   /* 0x09 */ {0x04, "geo_todo_09"},
   /* 0x0A */ {0x08, "geo_camera_frustum"}, // 8-12 variable
   /* 0x0B */ {0x04, "geo_node_start"},
   /* 0x0C */ {0x04, "geo_zbuffer"},
   /* 0x0D */ {0x08, "geo_render_range"},
   /* 0x0E */ {0x08, "geo_switch_case"},
   /* 0x0F */ {0x14, "geo_todo_0F"},
   /* 0x10 */ {0x10, "geo_translate_rotate"}, // variable
   /* 0x11 */ {0x08, "geo_todo_11"}, // variable
   /* 0x12 */ {0x08, "geo_todo_12"}, // variable
   /* 0x13 */ {0x0C, "geo_dl_translated"},
   /* 0x14 */ {0x08, "geo_billboard"},
   /* 0x15 */ {0x08, "geo_display_list"},
   /* 0x16 */ {0x08, "geo_shadow"},
   /* 0x17 */ {0x04, "geo_todo_17"},
   /* 0x18 */ {0x08, "geo_asm"},
   /* 0x19 */ {0x08, "geo_background"},
   /* 0x1A */ {0x08, "geo_nop_1A"},
   /* 0x1B */ {0x04, "geo_todo_1B"},
   /* 0x1C */ {0x0C, "geo_todo_1C"},
   /* 0x1D */ {0x08, "geo_scale"}, // variable
   /* 0x1E */ {0x08, "geo_nop_1E"},
   /* 0x1F */ {0x10, "geo_nop_1F"},
   /* 0x20 */ {0x04, "geo_start_distance"},
};

void write_geolayout(FILE *out, unsigned char *data, unsigned int start, unsigned int end, disasm_state *state)
{
   const int INDENT_AMOUNT = 3;
   const int INDENT_START = INDENT_AMOUNT;
   char label[128];
   unsigned int a = start;
   unsigned int tmp;
   int indent;
   int cmd_len;
   int print_label = 1;
   indent = INDENT_START;
   fprintf(out, ".include \"macros.inc\"\n"
                ".include \"geo_commands.inc\"\n\n"
                ".section .geo, \"a\"\n\n");
   while (a < end) {
      unsigned int cmd = data[a];
      if (print_label) {
         fprintf(out, "glabel geo_layout_X_%06X # %04X\n", a, a);
         print_label = 0;
      }
      if ((cmd == 0x01 || cmd == 0x05) && indent > INDENT_AMOUNT) {
         indent -= INDENT_AMOUNT;
      }
      print_spaces(out, indent);
      if (cmd < DIM(geo_table)) {
         if (cmd != 0x10) { // special case 0x10 since multiple pseudo
            fprintf(out, "%s", geo_table[cmd].macro);
         }
      } else {
         ERROR("Unknown geo layout command: 0x%02X\n", cmd);
      }
      cmd_len = geo_table[cmd].length;
      switch (cmd) {
         case 0x00: // 00 00 00 00 [SS SS SS SS]: branch and store
            tmp = read_u32_be(&data[a+4]);
            fprintf(out, " geo_layout_%08X # 0x%08X", tmp, tmp);
            break;
         case 0x01: // 01 00 00 00: terminate
         case 0x03: // 03 00 00 00: return from branch
            // no params
            fprintf(out, "\n");
            indent = INDENT_START;
            print_label = 1;
            break;
         case 0x04: // 04 00 00 00: open node
         case 0x05: // 05 00 00 00: close node
         case 0x0B: // 05 00 00 00: start geo layout
         case 0x17: // 17 00 00 00: set up object rendering
         case 0x1A: // 1A 00 00 00 00 00 00 00: no op
         case 0x1E: // 1E 00 00 00 00 00 00 00: no op
         case 0x1F: // 1E 00 00 00 00 00 00 00 00 00 00 00: no op
            // no params
            break;
         case 0x02: // 02 [AA] 00 00 [SS SS SS SS]
            tmp = read_u32_be(&data[a+4]);
            fprintf(out, " %d, geo_layout_%08X # 0x%08X", data[a+1], tmp, tmp);
            break;
         case 0x08: // 08 00 00 [AA] [XX XX] [YY YY] [WW WW] [HH HH]
            fprintf(out, " %d, %d, %d, %d, %d", data[a+3],
                  read_s16_be(&data[a+4]), read_s16_be(&data[a+6]),
                  read_s16_be(&data[a+8]), read_s16_be(&data[a+10]));
            break;
         case 0x09: // 09 00 00 [AA]
            fprintf(out, " %d", data[a+3]);
            break;
         case 0x0A: // 0A [AA] [BB BB] [NN NN] [FF FF] {EE EE EE EE}: set camera frustum
            fprintf(out, " %d, %d, %d", read_s16_be(&data[a+2]), read_s16_be(&data[a+4]), read_s16_be(&data[a+6]));
            if (data[a+1] > 0) {
               cmd_len += 4;
               disasm_label_lookup(state, read_u32_be(&data[a+8]), label);
               fprintf(out, ", %s", label);
            }
            break;
         case 0x0C: // 0C [AA] 00 00: enable/disable Z-buffer
            fprintf(out, " %d", data[a+1]);
            break;
         case 0x0D: // 0D 00 00 00 [AA AA] [BB BB]: set render range
            fprintf(out, " %d, %d", read_s16_be(&data[a+4]), read_s16_be(&data[a+6]));
            break;
         case 0x0E: // 0E 00 [NN NN] [AA AA AA AA]: switch/case
            fprintf(out, " %d, geo_switch_case_%08X", read_s16_be(&data[a+2]), read_u32_be(&data[a+4]));
            break;
         case 0x0F: // 0F 00 [TT TT] [XX XX] [YY YY] [ZZ ZZ] [UU UU] [VV VV] [WW WW] [AA AA AA AA]
            fprintf(out, " %d, %d, %d, %d, %d, %d, %d", read_s16_be(&data[a+2]),
                  read_s16_be(&data[a+4]), read_s16_be(&data[a+6]), read_s16_be(&data[a+8]),
                  read_s16_be(&data[a+10]), read_s16_be(&data[a+12]), read_s16_be(&data[a+14]));
            disasm_label_lookup(state, read_u32_be(&data[a+0x10]), label);
            fprintf(out, ", %s", label);
            break;
         case 0x10: // 10 [AA] [BB BB] [XX XX] [YY YY] [ZZ ZZ] [RX RX] [RY RY] [RZ RZ] {SS SS SS SS}: translate & rotate
         {
            unsigned char params = data[a+1];
            unsigned char field_type = (params & 0x70) >> 4;
            unsigned char layer = params & 0xF;
            switch (field_type) {
               case 0: // 10 [0L] 00 00 [TX TX] [TY TY] [TZ TZ] [RX RX] [RY RY] [RZ RZ] {SS SS SS SS}: translate & rotate
                  fprintf(out, "geo_translate_rotate %d, %d, %d, %d, %d, %d, %d", layer,
                          read_s16_be(&data[a+4]), read_s16_be(&data[a+6]), read_s16_be(&data[a+8]),
                          read_s16_be(&data[a+10]), read_s16_be(&data[a+12]), read_s16_be(&data[a+14]));
                  cmd_len = 16;
                  break;
               case 1: // 10 [1L] [TX TX] [TY TY] [TZ TZ] {SS SS SS SS}: translate
                  fprintf(out, "geo_translate %d, %d, %d, %d", layer,
                          read_s16_be(&data[a+2]), read_s16_be(&data[a+4]), read_s16_be(&data[a+6]));
                  cmd_len = 8;
                  break;
               case 2: // 10 [2L] [RX RX] [RY RY] [RZ RZ] {SS SS SS SS}: rotate
                  fprintf(out, "geo_rotate %d, %d, %d, %d", layer,
                          read_s16_be(&data[a+2]), read_s16_be(&data[a+4]), read_s16_be(&data[a+6]));
                  cmd_len = 8;
                  break;
               case 3: // 10 [3L] [RY RY] {SS SS SS SS}: rotate Y
                  fprintf(out, "geo_rotate_y %d, %d", layer, read_s16_be(&data[a+2]));
                  cmd_len = 4;
                  break;
            }
            if (params & 0x80) {
               tmp = read_u32_be(&data[a+cmd_len]);
               fprintf(out, ", seg%X_dl_%08X", (tmp >> 24) & 0xFF, tmp);
               cmd_len += 4;
            }
            break;
         }
         case 0x11: // 11 [P][L] [XX XX] [YY YY] [ZZ ZZ] {SS SS SS SS}: ? scene graph node, optional DL
         case 0x12: // 12 [P][L] [XX XX] [YY YY] [ZZ ZZ] {SS SS SS SS}: ? scene graph node, optional DL
         case 0x14: // 14 [P][L] [XX XX] [YY YY] [ZZ ZZ] {SS SS SS SS}: billboard model
            fprintf(out, " 0x%02X, %d, %d, %d", data[a+1] & 0xF, read_s16_be(&data[a+2]),
                  read_s16_be(&data[a+4]), read_s16_be(&data[a+6]));
            if (data[a+1] & 0x80) {
               disasm_label_lookup(state, read_u32_be(&data[a+8]), label);
               fprintf(out, ", %s", label);
               cmd_len += 4;
            }
            break;
         case 0x13: // 13 [LL] [XX XX] [YY YY] [ZZ ZZ] [AA AA AA AA]: scene graph node with layer and translation
            fprintf(out, " 0x%02X, %d, %d, %d", data[a+1],
                    read_s16_be(&data[a+2]), read_s16_be(&data[a+4]), read_s16_be(&data[a+6]));
            tmp = read_u32_be(&data[a+8]);
            if (tmp != 0x0) {
               fprintf(out, ", seg%X_dl_%08X", data[a+8], tmp);
            }
            break;
         case 0x15: // 15 [LL] 00 00 [AA AA AA AA]: load display list
            fprintf(out, " 0x%02X, seg%X_dl_%08X", data[a+1], data[a+4], read_u32_be(&data[a+4]));
            break;
         case 0x16: // 16 00 00 [AA] 00 [BB] [CC CC]: start geo layout with shadow
            fprintf(out, " 0x%02X, 0x%02X, %d", data[a+3], data[a+5], read_s16_be(&data[a+6]));
            break;
         case 0x18: // 18 00 [XX XX] [AA AA AA AA]: load polygons from asm
         case 0x19: // 19 00 [TT TT] [AA AA AA AA]: set background/skybox
            disasm_label_lookup(state, read_u32_be(&data[a+4]), label);
            fprintf(out, " %d, %s", read_s16_be(&data[a+2]), label);
            break;
         case 0x1B: // 1B 00 [XX XX]: ??
            fprintf(out, " %d", read_s16_be(&data[a+2]));
            break;
         case 0x1C: // 1C [PP] [XX XX] [YY YY] [ZZ ZZ] [AA AA AA AA]
            disasm_label_lookup(state, read_u32_be(&data[a+8]), label);
            fprintf(out, " 0x%02X, %d, %d, %d, %s", data[a+1], read_s16_be(&data[a+2]),
                    read_s16_be(&data[a+4]), read_s16_be(&data[a+6]), label);
            break;
         case 0x1D: // 1D [P][L] 00 00 [MM MM MM MM] {SS SS SS SS}: scale model
            fprintf(out, " 0x%02X, %d", data[a+1] & 0xF, read_u32_be(&data[a+4]));
            if (data[a+1] & 0x80) {
               disasm_label_lookup(state, read_u32_be(&data[a+8]), label);
               fprintf(out, ", %s", label);
               cmd_len += 4;
            }
            break;
         case 0x20: // 20 00 [AA AA]: start geo layout with rendering area
            fprintf(out, " %d", read_s16_be(&data[a+2]));
            break;
         default:
            ERROR("Unknown geo layout command: 0x%02X\n", cmd);
            break;
      }
      fprintf(out, "\n");
      switch (cmd) {
         case 0x04: // open_node
         case 0x08: // node_screen_area
         //case 0x0B: // node_start
         case 0x16: // geo_shadow
         case 0x20: // start_distance
            indent += INDENT_AMOUNT;
         default:
            break;
      }
      if (cmd == 0x01 || cmd == 0x03) { // end or return
         a += cmd_len;
         cmd_len = 0;
         while (a < end && 0 == read_u32_be(&data[a])) {
             fprintf(out, ".word 0x0\n");
             a += 4;
         }
      }
      a += cmd_len;
   }
}

void generate_geo_macros(arg_config *args)
{
   char macrofilename[FILENAME_MAX];
   FILE *fmacro;
   sprintf(macrofilename, "%s/geo_commands.inc", args->output_dir);
   fmacro = fopen(macrofilename, "w");
   if (fmacro == NULL) {
      ERROR("Error opening %s\n", macrofilename);
      exit(3);
   }
   fprintf(fmacro,
"# geo layout macros\n"
"\n"
"# 0x00: Branch and store return address\n"
"#   0x04: scriptTarget, segment address of geo layout\n"
".macro geo_branch_and_link scriptTarget\n"
"    .byte 0x00, 0x00, 0x00, 0x00\n"
"    .word \\scriptTarget\n"
".endm\n"
"\n"
"# 0x01: Terminate geo layout\n"
"#   0x01-0x03: unused\n"
".macro geo_end\n"
"    .byte 0x01, 0x00, 0x00, 0x00\n"
".endm\n"
"\n"
"# 0x02: Branch\n"
"#   0x01: if 1, store next geo layout address on stack\n"
"#   0x02-0x03: unused\n"
"#   0x04: scriptTarget, segment address of geo layout\n"
".macro geo_branch type, scriptTarget\n"
"    .byte 0x02, \\type, 0x00, 0x00\n"
"    .word \\scriptTarget\n"
".endm\n"
"\n"
"# 0x03: Return from branch\n"
"#   0x01-0x03: unused\n"
".macro geo_return\n"
"    .byte 0x03, 0x00, 0x00, 0x00\n"
".endm\n"
"\n"
"# 0x04: Open node\n"
"#   0x01-0x03: unused\n"
".macro geo_open_node\n"
"    .byte 0x04, 0x00, 0x00, 0x00\n"
".endm\n"
"\n"
"# 0x05: Close node\n"
"#   0x01-0x03: unused\n"
".macro geo_close_node\n"
"    .byte 0x05, 0x00, 0x00, 0x00\n"
".endm\n"
"\n"
"# 0x06: TODO\n"
"#   0x01: unused\n"
"#   0x02: s16, index of some array\n"
".macro geo_todo_06 param\n"
"    .byte 0x06, 0x00\n"
"    .hword \\param\n"
".endm\n"
"\n"
"# 0x07: Update current scene graph node flags\n"
"#   0x01: u8 operation (0 = reset, 1 = set, 2 = clear)\n"
"#   0x02: s16 bits\n"
".macro geo_update_node_flags operation, flagBits\n"
"    .byte 0x07, \\operation\n"
"    .hword \\flagBits\n"
".endm\n"
"\n"
"# 0x08: Create screen area scene graph node\n"
"#   0x01: unused\n"
"#   0x02: s16 num entries (+2) to allocate\n"
"#   0x04: s16 x\n"
"#   0x06: s16 y\n"
"#   0x08: s16 width\n"
"#   0x0A: s16 height\n"
".macro geo_node_screen_area numEntries, x, y, width, height\n"
"    .byte 0x08, 0x00\n"
"    .hword \\numEntries\n"
"    .hword \\x, \\y, \\width, \\height\n"
".endm\n"
"\n"
"# 0x09: TODO Create ? scene graph node\n"
"#   0x02: s16 ?\n"
".macro geo_todo_09 param\n"
"    .byte 0x09, 0x00\n"
"    .hword \\param\n"
".endm\n"
"\n"
"# 0x0A: Create camera frustum scene graph node\n"
"#   0x01: u8  if nonzero, enable function field\n"
"#   0x02: s16 field of view\n"
"#   0x04: s16 near\n"
"#   0x06: s16 far\n"
"#   0x08: [GraphNodeFunc function]\n"
".macro geo_camera_frustum fov, near, far, function=0\n"
"    .byte 0x0A\n"
"    .if (\\function != 0)\n"
"       .byte 0x01\n"
"    .else\n"
"       .byte 0x00\n"
"    .endif\n"
"    .hword \\fov, \\near, \\far\n"
"    .if (\\function != 0)\n"
"       .word \\function\n"
"    .endif\n"
".endm\n"
"\n"
"# 0x0B: Create a root scene graph node\n"
"#   0x01-0x03: unused\n"
".macro geo_node_start\n"
"    .byte 0x0B, 0x00, 0x00, 0x00\n"
".endm\n"
"\n"
"# 0x0C: Create zbuffer-toggling scene graph node\n"
"#   0x01: u8 enableZBuffer (1 = on, 0 = off)\n"
"#   0x02-0x03: unused\n"
".macro geo_zbuffer enable\n"
"    .byte 0x0C, \\enable, 0x00, 0x00\n"
".endm\n"
"\n"
"# 0x0D: Create render range scene graph node\n"
"#   0x01-0x03: unused\n"
"#   0x04: s16 minDistance\n"
"#   0x06: s16 maxDistance\n"
".macro geo_render_range minDistance, maxDistance\n"
"    .byte 0x0D, 0x00, 0x00, 0x00\n"
"    .hword \\minDistance, \\maxDistance\n"
".endm\n"
"\n"
"# 0x0E: Create switch-case scene graph node\n"
"#   0x01: unused\n"
"#   0x02: s16 numCases\n"
"#   0x04: GraphNodeFunc caseSelectorFunc\n"
".macro geo_switch_case count, function\n"
"    .byte 0x0E, 0x00\n"
"    .hword \\count\n"
"    .word \\function\n"
".endm\n"
"\n"
"# 0x0F: TODO Create ? scene graph node\n"
"#   0x01: unused\n"
"#   0x02: s16 ?\n"
"#   0x04: s16 unkX\n"
"#   0x06: s16 unkY\n"
"#   0x08: s16 unkZ\n"
"#   0x0A: s16 unkX_2\n"
"#   0x0C: s16 unkY_2\n"
"#   0x0E: s16 unkZ_2\n"
"#   0x10: GraphNodeFunc function\n"
".macro geo_todo_0F unknown, x1, y1, z1, x2, y2, z2, function\n"
"    .byte 0x0F, 0x00\n"
"    .hword \\unknown, \\x1, \\y1, \\z1, \\x2, \\y2, \\z2\n"
"    .word \\function\n"
".endm\n"
"\n"
"# 0x10: Create translation & rotation scene graph node with optional display list\n"
"# Four different versions of 0x10\n"
"#   cmd+0x01: u8 params\n"
"#     0b1000_0000: if set, enable displayList field and drawingLayer\n"
"#     0b0111_0000: fieldLayout (determines how rest of data is formatted\n"
"#     0b0000_1111: drawingLayer\n"
"#\n"
"#   fieldLayout = 0: Translate & Rotate\n"
"#     0x04: s16 xTranslation\n"
"#     0x06: s16 xTranslation\n"
"#     0x08: s16 xTranslation\n"
"#     0x0A: s16 xRotation\n"
"#     0x0C: s16 xRotation\n"
"#     0x0E: s16 xRotation\n"
"#     0x10: [u32 displayList: if MSbit of params set, display list segmented address]\n"
".macro geo_translate_rotate layer, tx, ty, tz, rx, ry, rz, displayList=0\n"
"    .byte 0x10\n"
"    .if (\\displayList != 0)\n"
"        .byte 0x00 | \\layer | 0x80\n"
"    .else\n"
"        .byte 0x00 | \\layer\n"
"    .endif\n"
"    .hword 0x0000\n"
"    .hword \\tx, \\ty, \\tz\n"
"    .hword \\rx, \\ry, \\rz\n"
"    .if (\\displayList != 0)\n"
"        .word \\displayList\n"
"    .endif\n"
".endm\n"
"\n"
"#   fieldLayout = 1: Translate\n"
"#     0x02: s16 xTranslation\n"
"#     0x04: s16 yTranslation\n"
"#     0x06: s16 zTranslation\n"
"#     0x08: [u32 displayList: if MSbit of params set, display list segmented address]\n"
".macro geo_translate layer, tx, ty, tz, displayList=0\n"
"    .byte 0x10\n"
"    .if (\\displayList != 0)\n"
"        .byte 0x10 | \\layer | 0x80\n"
"    .else\n"
"        .byte 0x10 | \\layer\n"
"    .endif\n"
"    .hword \\tx, \\ty, \\tz\n"
"    .if (\\displayList != 0)\n"
"        .word \\displayList\n"
"    .endif\n"
".endm\n"
"\n"
"#   fieldLayout = 2: Rotate\n"
"#     0x02: s16 xRotation\n"
"#     0x04: s16 yRotation\n"
"#     0x06: s16 zRotation\n"
"#     0x08: [u32 displayList: if MSbit of params set, display list segmented address]\n"
".macro geo_rotate layer, rx, ry, rz, displayList=0\n"
"    .byte 0x10\n"
"    .if (\\displayList != 0)\n"
"        .byte 0x20 | \\layer | 0x80\n"
"    .else\n"
"        .byte 0x20 | \\layer\n"
"    .endif\n"
"    .hword \\rx, \\ry, \\rz\n"
"    .if (\\displayList != 0)\n"
"        .word \\displayList\n"
"    .endif\n"
".endm\n"
"\n"
"#   fieldLayout = 3: Rotate Y\n"
"#     0x02: s16 yRotation\n"
"#     0x04: [u32 displayList: if MSbit of params set, display list segmented address]\n"
".macro geo_rotate_y layer, ry, displayList=0\n"
"    .byte 0x10\n"
"    .if (\\displayList != 0)\n"
"        .byte 0x30 | \\layer | 0x80\n"
"    .else\n"
"        .byte 0x30 | \\layer\n"
"    .endif\n"
"    .hword \\ry\n"
"    .if (\\displayList != 0)\n"
"        .word \\displayList\n"
"    .endif\n"
".endm\n"
"\n"
"# 0x11: TODO Create ? scene graph node with optional display list\n"
"#   0x01: u8 params\n"
"#     0b1000_0000: if set, enable displayList field and drawingLayer\n"
"#     0b0000_1111: drawingLayer\n"
"#   0x02: s16 unkX\n"
"#   0x04: s16 unkY\n"
"#   0x06: s16 unkZ\n"
"#   0x08: [u32 displayList: if MSbit of params set, display list segmented address]\n"
".macro geo_todo_11 layer, ux, uy, uz, displayList=0\n"
"    .byte 0x11\n"
"    .if (\\displayList != 0)\n"
"        .byte 0x80 | \\layer\n"
"    .else\n"
"        .byte 0x00\n"
"    .endif\n"
"    .hword \\ux, \\uy, \\uz\n"
"    .if (\\displayList != 0)\n"
"        .word \\displayList\n"
"    .endif\n"
".endm\n"
"\n"
"# 0x12: TODO Create ? scene graph node\n"
"#   0x01: u8 params\n"
"#      0b1000_0000: if set, enable displayList field and drawingLayer\n"
"#      0b0000_1111: drawingLayer\n"
"#   0x02: s16 unkX\n"
"#   0x04: s16 unkY\n"
"#   0x06: s16 unkZ\n"
"#   0x08: [u32 displayList: if MSbit of params set, display list segmented address]\n"
".macro geo_todo_12 layer, ux, uy, uz, displayList=0\n"
"    .byte 0x12\n"
"    .if (\\displayList != 0)\n"
"        .byte 0x80 | \\layer\n"
"    .else\n"
"        .byte 0x00\n"
"    .endif\n"
"    .hword \\ux, \\uy, \\uz\n"
"    .if (\\displayList != 0)\n"
"        .word \\displayList\n"
"    .endif\n"
".endm\n"
"\n"
"# 0x13: Create display list scene graph node with translation\n"
"#   0x01: u8 drawingLayer\n"
"#   0x02: s16 xTranslation\n"
"#   0x04: s16 yTranslation\n"
"#   0x06: s16 zTranslation\n"
"#   0x08: u32 displayList: dislay list segmented address\n"
".macro geo_dl_translated layer, x, y, z, displayList=0\n"
"    .byte 0x13, \\layer\n"
"    .hword \\x, \\y, \\z\n"
"    .word \\displayList\n"
".endm\n"
"\n"
"# 0x14: Create billboarding node with optional display list\n"
"#   0x01: u8 params\n"
"#      0b1000_0000: if set, enable displayList field and drawingLayer\n"
"#      0b0000_1111: drawingLayer\n"
"#   0x02: s16 xTranslation\n"
"#   0x04: s16 yTranslation\n"
"#   0x06: s16 zTranslation\n"
"#   0x08: [u32 displayList: if MSbit of params is set, display list segmented address]\n"
".macro geo_billboard layer=0, tx=0, ty=0, tz=0, displayList=0\n"
"    .byte 0x14\n"
"    .if (\\displayList != 0)\n"
"        .byte 0x80 | \\layer\n"
"    .else\n"
"        .byte 0x00\n"
"    .endif\n"
"    .hword \\tx, \\ty, \\tz\n"
"    .if (\\displayList != 0)\n"
"        .word \\displayList\n"
"    .endif\n"
".endm\n"
"\n"
"# 0x15: Create plain display list scene graph node\n"
"#   0x01: u8 drawingLayer\n"
"#   0x02=0x03: unused\n"
"#   0x04: u32 displayList: display list segmented address\n"
".macro geo_display_list layer, displayList\n"
"    .byte 0x15, \\layer, 0x00, 0x00\n"
"    .word \\displayList\n"
".endm\n"
"\n"
"# 0x16: Create shadow scene graph node\n"
"#   0x01: unused\n"
"#   0x02: s16 shadowType (cast to u8)\n"
"#   0x04: s16 shadowSolidity (cast to u8)\n"
"#   0x06: s16 shadowScale\n"
".set SHADOW_CIRCLE_UNK0,      0x00\n"
".set SHADOW_CIRCLE_UNK1,      0x01\n"
".set SHADOW_CIRCLE_UNK2,      0x02  # unused shadow type\n"
".set SHADOW_SQUARE_PERMANENT, 0x0A  # square shadow that never disappears\n"
".set SHADOW_SQUARE_SCALABLE,  0x0B  # square shadow, shrinks with distance\n"
".set SHADOW_SQUARE_TOGGLABLE, 0x0C  # square shadow, disappears with distance\n"
".set SHADOW_CIRCLE_PLAYER,    0x63  # player (Mario) shadow\n"
".set SHADOW_RECTANGLE_HARDCODED_OFFSET, 0x32 # offset of hard-coded shadows\n"
".macro geo_shadow type, solidity, scale\n"
"    .byte 0x16, 0x00\n"
"    .hword \\type, \\solidity, \\scale\n"
".endm\n"
"\n"
"# 0x17: TODO Create ? scene graph node\n"
"#   0x01-0x03: unused\n"
".macro geo_todo_17\n"
"    .byte 0x17, 0x00, 0x00, 0x00\n"
".endm\n"
"\n"
"# 0x18: Create ? scene graph node\n"
"#   0x01: unused\n"
"#   0x02: s16 parameter\n"
"#   0x04: GraphNodeFunc function\n"
".macro geo_asm param, function\n"
"    .byte 0x18, 0x00\n"
"    .hword \\param\n"
"    .word \\function\n"
".endm\n"
"\n"
"# 0x19: Create background scene graph node\n"
"#   0x02: s16 background: background ID, or RGBA5551 color if backgroundFunc is null\n"
"#   0x04: GraphNodeFunc backgroundFunc\n"
".macro geo_background param, function=0\n"
"    .byte 0x19, 0x00\n"
"    .hword \\param\n"
"    .word \\function\n"
".endm\n"
"\n"
"# 0x1A: No operation\n"
".macro geo_nop_1A\n"
"    .byte 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00\n"
".endm\n"
"\n"
"# 0x1B: TODO Create ? scene graph node\n"
"#   0x02: s16 index of array\n"
".macro geo_todo_1B param\n"
"    .byte 0x1B, 0x00\n"
"    .hword \\param\n"
".endm\n"
"\n"
"# 0x1C: TODO Create ? scene graph node\n"
"#   0x01: u8 unk01\n"
"#   0x02: s16 unkX\n"
"#   0x04: s16 unkY\n"
"#   0x06: s16 unkZ\n"
"#   0x08: GraphNodeFunc nodeFunc\n"
".macro geo_todo_1C param, ux, uy, uz, nodeFunc\n"
"    .byte 0x1C, \\param\n"
"    .hword \\ux, \\uy, \\uz\n"
"    .word \\nodeFunc\n"
".endm\n"
"\n"
"# 0x1D: Create scale scene graph node with optional display list\n"
"#   0x01: u8 params\n"
"#     0b1000_0000: if set, enable displayList field and drawingLayer\n"
"#     0b0000_1111: drawingLayer\n"
"#   0x02-0x03: unused\n"
"#   0x04: u32 scale (0x10000 = 1.0)\n"
"#   0x08: [u32 displayList: if MSbit of params is set, display list segment address]\n"
".macro geo_scale layer, scale, displayList=0\n"
"    .byte 0x1D\n"
"    .if (\\displayList != 0)\n"
"        .byte 0x80 | \\layer\n"
"    .else\n"
"        .byte 0x00\n"
"    .endif\n"
"    .byte 0x00, 0x00\n"
"    .word \\scale\n"
"    .if (\\displayList != 0)\n"
"        .word \\displayList\n"
"    .endif\n"
".endm\n"
"\n"
"# 0x1E: No operation\n"
".macro geo_nop_1E\n"
"    .byte 0x1E, 0x00, 0x00, 0x00\n"
"    .byte 0x00, 0x00, 0x00, 0x00\n"
".endm\n"
"\n"
"# 0x1F: No operation\n"
".macro geo_nop_1F\n"
"    .byte 0x1F, 0x00, 0x00, 0x00\n"
"    .byte 0x00, 0x00, 0x00, 0x00\n"
"    .byte 0x00, 0x00, 0x00, 0x00\n"
"    .byte 0x00, 0x00, 0x00, 0x00\n"
".endm\n"
"\n"
"# 0x20: Create render distance scene graph node (unconfirmed?)\n"
"#   0x01: unused\n"
"#   0x02: s16 renderDistance?\n"
".macro geo_start_distance renderDistance\n"
"    .byte 0x20, 0x00\n"
"    .hword \\renderDistance\n"
".endm\n"
"\n"
   );
}