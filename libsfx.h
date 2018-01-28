#ifndef LIBSFX_H_
#define LIBSFX_H_

// defines

// typedefs

   typedef struct {
      unsigned int order;
      unsigned int predictor_count;
      unsigned *data;
   } predictor_data;
   
   typedef struct {
      unsigned int start;
      unsigned int end;
      unsigned int count;
      unsigned int unknown;
      unsigned *state;
   } loop_data;
   
   typedef struct {
      unsigned int unknown_1;
      unsigned int sound_offset;
      loop_data *loop;
      predictor_data *predictor;
      unsigned int sound_length;
      unsigned int unknown_2;
      unsigned int unknown_3;
      unsigned int unknown_4;
   } wave_table;
   
   typedef struct {
      unsigned char unknown_1;
     unsigned char pan;
     unsigned unknown_2;
     wave_table *wav;
     float key_base;
     unsigned *adrs;
   } percussion;
   
   typedef struct {
      percussion *items;
   } percussion_table;
   
   typedef struct {
      unsigned int unknown;
      unsigned *adrs;
      wave_table *wav_prev;
      float key_base_prev;
      wave_table *wav;
      float key_base;
      wave_table *wav_sec;
      float key_base_sec;
   } sound;
   
   typedef struct {
      unsigned int instrument_count;
      unsigned int percussion_count;
      unsigned int unknown_1;
      unsigned int unknown_2;
      percussion_table percussions;
      sound *sounds;
   } sound_bank;
   
   typedef struct {
      unsigned unknown;
      unsigned bank_count;
      sound_bank *banks;
   } sound_bank_header;
   
   typedef struct {
      unsigned unknown;
      unsigned data_count;
      unsigned char **data;
   } sound_data_header;

// function prototypes

//NEEDS COMMENTS!!!

// initialize the key table for vadpcm decoding
void sfx_initialize_key_table();

// read the sound bank table
// data: buffer containing sound bank data
// data_offset: offset in data where the sound bank begins
// returns a sound_data_header which contains info about all the sounds stored in the rom
sound_bank_header read_sound_bank(unsigned char *data, unsigned int data_offset);

// read the sound data table
// data: buffer containing sound data
// data_offset: offset in data where the sound data begins
// returns a sound_data_header which contains the raw, encoded sound data
sound_data_header read_sound_data(unsigned char *data, unsigned int data_offset);

// create a .wav file from provided encoded sound data
// sound_dir: directory to store the .wav file in
// wav_name: name for the new .wav file
// wav: the sound information that's stored in the sound_bank_header
// key_base: not entirely sure what this is, but it's converted & stored in the new .wav file
// snd_data: buffer containing the raw, encoded sound data
// sampling_rate: sample rate for the sound data (higher sampling rate = wav file speeds up)
// returns 1 if the .wav file was created, 0 if not
int extract_raw_sound(char *sound_dir, char *wav_name, wave_table *wav, float key_base, unsigned char *snd_data, unsigned long sampling_rate);

#endif // LIBMIO0_H_
