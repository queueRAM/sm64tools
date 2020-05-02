#include "n64split.h"

#define MUSIC_SUBDIR "music"

void parse_music_sequences(FILE *out, unsigned char *data, split_section *sec, arg_config *args, strbuf *makeheader)
{
   typedef struct {
      unsigned int start;
      unsigned int length;
   } sequence;
   typedef struct {
      unsigned int revision;
      unsigned int count;
      sequence *seq;
   } sequence_bank;

   char music_dir[FILENAME_MAX];
   char m64_file[FILENAME_MAX];
   char m64_file_rel[FILENAME_MAX];
   char seq_name[128];
   sequence_bank seq_bank = {0};
   unsigned int i;

   sprintf(music_dir, "%s/%s", args->output_dir, MUSIC_SUBDIR);
   make_dir(music_dir);

   seq_bank.revision = read_u16_be(&data[sec->start]);
   seq_bank.count = read_u16_be(&data[sec->start+2]);
   if (seq_bank.count > 0) {
      seq_bank.seq = malloc(seq_bank.count * sizeof(*seq_bank.seq));
      for (i = 0; i < seq_bank.count; i++) {
         seq_bank.seq[i].start = read_u32_be(&data[sec->start+i*8+4]);
         seq_bank.seq[i].length = read_u32_be(&data[sec->start+i*8+8]);
      }
   }

   fprintf(out, "\n# music sequence table\n");
   fprintf(out, "music_sequence_table_header:\n");
   fprintf(out, ".hword %d, (music_sequence_table_end - music_sequence_table) / 8\n", seq_bank.revision);
   fprintf(out, "music_sequence_table:\n");
   for (i = 0; i < seq_bank.count; i++) {
      sprintf(seq_name, "seq_%02X", i);
      fprintf(out, ".word (%s - music_sequence_table_header), (%s_end - %s) # 0x%05X, 0x%04X\n",
              seq_name, seq_name, seq_name, seq_bank.seq[i].start, seq_bank.seq[i].length);
   }
   fprintf(out, "music_sequence_table_end:\n");
   fprintf(out, "\n.align 4, 0x01\n");
   for (i = 0; i < seq_bank.count; i++) {
      sprintf(seq_name, "seq_%02X", i);
      fprintf(out, "\n%s:", seq_name);

      sprintf(m64_file, "%s/%s.m64", music_dir, seq_name);
      write_file(m64_file, &data[sec->start + seq_bank.seq[i].start], seq_bank.seq[i].length);

      sprintf(m64_file_rel, "%s/%s.m64", MUSIC_SUBDIR, seq_name);
      fprintf(out, "\n.incbin \"%s\"\n", m64_file_rel);

      // append to Makefile
      strbuf_sprintf(makeheader, " \\\n$(MUSIC_DIR)/%s.m64", seq_name);

      fprintf(out, "%s_end:\n", seq_name);
   }

   // free used memory
   if (seq_bank.count > 0) {
      free(seq_bank.seq);
   }
}

void parse_instrument_set(FILE *out, unsigned char *data, split_section *sec)
{
   unsigned int *instrument_set;
   unsigned int count;
   unsigned int i, cur;

   count = sec->child_count;
   // each sequence has its own instrument set defined by offsets table
   instrument_set = malloc(count * sizeof(*instrument_set));
   for (i = 0; i < count; i++) {
      instrument_set[i] = read_u16_be(&data[sec->start + 2*i]);
   }
   fprintf(out, "\ninstrument_sets:\n");
   for (i = 0; i < count; i++) {
      fprintf(out, ".hword instrument_set_%02X - instrument_sets # 0x%04X\n", i, instrument_set[i]);
   }

   // output each instrument set
   cur = 0;
   for (i = 2*count; sec->start + i < sec->end; i++) {
      unsigned char val = data[sec->start + i];
      if (instrument_set[cur] == i) {
         fprintf(out, "\ninstrument_set_%02X:\n.byte 0x%02X", cur, val);
         cur++;
      } else {
         fprintf(out, ", 0x%02X", val);
      }
   }
   fprintf(out, "\ninstrument_sets_end:\n");
}

void parse_sound_banks(FILE *out, unsigned char *data, split_section *secCtl, split_section *secTbl, arg_config *args, strbuf *makeheader)
{
   // TODO: unused parameters
   (void)out;
   (void)makeheader;

   char sound_dir[FILENAME_MAX];
   char sfx_file[FILENAME_MAX];
   unsigned int i, j, sound_count;

   sfx_initialize_key_table();
   
   sprintf(sound_dir, "%s/%s", args->output_dir, SOUNDS_SUBDIR);
   make_dir(sound_dir);

   sound_data_header sound_data = read_sound_data(data, secTbl->start);
   sound_bank_header sound_banks = read_sound_bank(data, secCtl->start);
   
   sound_count = 0;
   
   for (i = 0; i < sound_banks.bank_count; i++) {
      for (j = 0; j < sound_banks.banks[i].instrument_count; j++) {
        if(sound_banks.banks[i].sounds[j].wav_prev != NULL) {
          sprintf(sfx_file, "Bank%uSound%uPrev", i, j);
          extract_raw_sound(sound_dir, sfx_file, sound_banks.banks[i].sounds[j].wav_prev, sound_banks.banks[i].sounds[j].key_base_prev, sound_data.data[i], 16000);
          sound_count++;
       }
        if(sound_banks.banks[i].sounds[j].wav != NULL) {
          sprintf(sfx_file, "Bank%uSound%u", i, j);
          extract_raw_sound(sound_dir, sfx_file, sound_banks.banks[i].sounds[j].wav, sound_banks.banks[i].sounds[j].key_base, sound_data.data[i], 16000);
          sound_count++;
       }
        if(sound_banks.banks[i].sounds[j].wav_sec != NULL) {
          sprintf(sfx_file, "Bank%uSound%uSec", i, j);
          extract_raw_sound(sound_dir, sfx_file, sound_banks.banks[i].sounds[j].wav_sec, sound_banks.banks[i].sounds[j].key_base_sec, sound_data.data[i], 16000);
          sound_count++;
       }
     }
     
     // Todo: add percussion export here
   }

   // free used memory
   if (sound_banks.bank_count > 0) {
      free(sound_banks.banks);
   }
   
   INFO("Successfully exported sounds:\n");
   INFO("  # of banks: %u\n", sound_banks.bank_count);
   INFO("  # of sounds: %u\n", sound_count);
}
