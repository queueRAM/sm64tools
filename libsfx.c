//A lot of this code is heavily influenced by and even directly copied from the work done by SubDrag and
// Ice Mario on the N64 Sound Tool and VADPCM decoding/encoding. This wouldn't be possible without their
// enormous contribution. Thanks guys!

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libsfx.h"
#include "strutils.h"
#include "utils.h"

// defines

// functions

static float sfx_key_table[0x100];

static wave_table * read_wave_table(unsigned char *data, unsigned int wave_offset, unsigned int sound_bank_offset)
{
   wave_table *wav = malloc(sizeof(wave_table));
   wav->unknown_1 = read_u32_be(&data[wave_offset]);
   wav->sound_offset = read_u32_be(&data[wave_offset+4]);
   
   //loop
   unsigned int loop_offset = read_u32_be(&data[wave_offset+8]);
   if(loop_offset != 0) {
     loop_offset += sound_bank_offset + 16;
     wav->loop = malloc(sizeof(loop_data));
     wav->loop->start = read_u32_be(&data[loop_offset]);
     wav->loop->end = read_u32_be(&data[loop_offset+4]);
     wav->loop->count = read_u32_be(&data[loop_offset+8]);
     wav->loop->unknown = read_u32_be(&data[loop_offset+12]);
     if(wav->loop->start != 0 || wav->loop->count != 0) {
       wav->loop->state = malloc(8 * sizeof(unsigned));
       for (int k = 0; k < 8; k++) {
          wav->loop->state[k] = read_u16_be(&data[loop_offset+16+k*2]);
       }
     }
   }
   
   //predictor
   unsigned int predictor_offset = read_u32_be(&data[wave_offset+12]);
   if(predictor_offset != 0) {
     predictor_offset += sound_bank_offset + 16;
     wav->predictor = malloc(sizeof(predictor_data));
     wav->predictor->order = read_u32_be(&data[predictor_offset]);
     wav->predictor->predictor_count = read_u32_be(&data[predictor_offset+4]);
    unsigned int num_predictor = wav->predictor->order * wav->predictor->predictor_count * 8;
     wav->predictor->data = malloc(num_predictor * sizeof(unsigned));
     for (unsigned int k = 0; k < num_predictor; k++) {
          wav->predictor->data[k] = read_u16_be(&data[predictor_offset+8+k*2]);
     }
   }
   
   wav->sound_length = read_u32_be(&data[wave_offset+16]);
   wav->unknown_2 = read_u32_be(&data[wave_offset+20]);
   wav->unknown_3 = read_u32_be(&data[wave_offset+24]);
   wav->unknown_4 = read_u32_be(&data[wave_offset+28]);
   
   return wav;
}

void sfx_initialize_key_table()
{
   for (int x = 0; x < 0xFF; x++)
   {
      sfx_key_table[x] = (float)((60.0 - (float)x) / 12.0) * (float)((60.0 - (float)x) / 12.0); //Squared
   }
}

static unsigned char sfx_convert_ead_game_value_to_key_base(float eadKeyvalue)
{
   float keybaseReal = (((eadKeyvalue - 0.0) < 0.00001) ? 1.0f : eadKeyvalue);

   float smallestDistance = 9999999999999.0f;
   unsigned char realKey = 0;

   for (int x = 0; x < 0x100; x++)
   {
      float distance = (fabsf(keybaseReal - sfx_key_table[x]));

      if (distance < smallestDistance)
      {
         smallestDistance = distance;
         realKey = x;
      }
   }

   if (realKey > 0x7F)
      realKey = 0x7F;

   return realKey;
}

// *************** //
// VADPCM Decoding //
// By Ice Mario!   //
// *************** //

static const short sfx_itable[16] =
{
   0,1,2,3,4,5,6,7,
   -8,-7,-6,-5,-4,-3,-2,-1,
};

static signed short sfx_sign_extend(unsigned b, // number of bits representing the number in x
                  int x      // sign extend this b-bit number to r
)
{
   

   int m = 1 << (b - 1); // mask can be pre-computed if b is fixed

   x = x & ((1 << b) - 1);  // (Skip this if bits in x above position b are already zero.)
   return (x ^ m) - m;
}

static void decode_8( unsigned char *in, signed short *out , int index, signed short * pred1, signed short lastsmp[8])
{
   int i;
   signed short tmp[8];
   signed long total = 0;
   signed short sample =0;
   memset(out, 0, sizeof(signed short)*8);

   signed short *pred2 = (pred1 + 8);

   //printf("pred2[] = %x\n" , pred2[0]);
   for(i=0; i<8; i++)
   {
      tmp[i] = sfx_itable[(i&1) ? (*in++ & 0xf) : ((*in >> 4) & 0xf)] << index;
      tmp[i] = sfx_sign_extend(index+4, tmp[i]);
   }

   for(i=0; i<8; i++)
   {
      total = (pred1[i] * lastsmp[6]);
      total+= (pred2[i] * lastsmp[7]);

      if (i>0)
      {
         for(int x=i-1; x>-1; x--)
         {
            total += ( tmp[((i-1)-x)] * pred2[x] );
            //printf("sample: %x - pred: %x - _smp: %x\n" , ((i-1)-x) , pred2[x] , tmp[((i-1)-x)]);
         }
      }

      //printf("pred = %x | total = %x\n" , pred2[0] , total);
      float result = ((tmp[i] << 0xb) + total) >> 0xb;
      if (result > 32767)
         sample = 32767;
      else if (result < -32768)
         sample = -32768;
      else
         sample = (signed short)result;

      out[i] = sample;
   }
   // update the last sample set for subsequent iterations
   memcpy(lastsmp, out, sizeof(signed short)*8);
}

static void decode_8_half( unsigned char *in, signed short *out , int index, signed short * pred1, signed short lastsmp[8])
{
   int i;
   signed short tmp[8];
   signed long total = 0;
   signed short sample =0;
   memset(out, 0, sizeof(signed short)*8);

   signed short *pred2 = (pred1 + 8);

   //printf("pred2[] = %x\n" , pred2[0]);

   tmp[0] = (((((*in) & 0xC0) >> 6) & 0x3)) << (index);
   tmp[0] = sfx_sign_extend(index+2, tmp[0]);
   tmp[1] = (((((*in) & 0x30) >> 4) & 0x3)) << (index);
   tmp[1] = sfx_sign_extend(index+2, tmp[1]);
   tmp[2] = (((((*in) & 0x0C) >> 2) & 0x3)) << (index);
   tmp[2] = sfx_sign_extend(index+2, tmp[2]);
   tmp[3] = ((((*in++) & 0x03) & 0x3)) << (index);
   tmp[3] = sfx_sign_extend(index+2, tmp[3]);
   tmp[4] = (((((*in) & 0xC0) >> 6) & 0x3)) << (index);
   tmp[4] = sfx_sign_extend(index+2, tmp[4]);
   tmp[5] = (((((*in) & 0x30) >> 4) & 0x3)) << (index);
   tmp[5] = sfx_sign_extend(index+2, tmp[5]);
   tmp[6] = (((((*in) & 0x0C) >> 2) & 0x3)) << (index);
   tmp[6] = sfx_sign_extend(index+2, tmp[6]);
   tmp[7] = ((((*in++) & 0x03) & 0x3)) << (index);
   tmp[7] = sfx_sign_extend(index+2, tmp[7]);

   for(i=0; i<8; i++)
   {
      total = (pred1[i] * lastsmp[6]);
      total+= (pred2[i] * lastsmp[7]);

      if (i>0)
      {
         for(int x=i-1; x>-1; x--)
         {
            total += ( tmp[((i-1)-x)] * pred2[x] );
            //printf("sample: %x - pred: %x - _smp: %x\n" , ((i-1)-x) , pred2[x] , tmp[((i-1)-x)]);
         }
      }

      //printf("pred = %x | total = %x\n" , pred2[0] , total);
      float result = ((tmp[i] << 0xb) + total) >> 0xb;
      if (result > 32767)
         sample = 32767;
      else if (result < -32768)
         sample = -32768;
      else
         sample = (signed short)result;

      out[i] = sample;
   }
   // update the last sample set for subsequent iterations
   memcpy(lastsmp, out, sizeof(signed short)*8);
}

static unsigned long decode( unsigned char *in, signed short *out, unsigned long len, predictor_data *book, int decode8Only )
{
   signed short lastsmp[8];

   for (int x = 0; x < 8; x++)
      lastsmp[x] = 0;

   int index;
   int pred;

   int samples = 0;

   // flip the predictors
   signed short *preds = (signed short*)malloc( 32 * book->predictor_count );
   for (unsigned int p = 0; p < (8 * book->order * book->predictor_count); p++)
   {
      preds[p] = book->data[p];
   }

   if (decode8Only == 0)
   {
      int _len = (len / 9) * 9;   //make sure length was actually a multiple of 9

      while(_len > 0)
      {
         index = (*in >> 4) & 0xf;
         pred = (*in & 0xf);

         // to not make zelda crash but doesn't fix it
         pred = pred % (book->predictor_count);

         _len--;

         signed short * pred1 = &preds[ pred * 16] ;

         decode_8(++in, out, index, pred1, lastsmp);
         in+=4;   _len-=4;   out+=8;

         decode_8(in, out, index, pred1, lastsmp);
         in+=4;   _len-=4;   out+=8;

         samples += 16;
      }
   }
   else
   {
      int _len = (len / 5) * 5;   //make sure length was actually a multiple of 5

      while(_len > 0)
      {
         index = (*in >> 4) & 0xf;
         pred = (*in & 0xf);

         // to not make zelda crash but doesn't fix it
         pred = pred % (book->predictor_count);

         _len--;

         signed short * pred1 = &preds[ pred * 16] ;

         decode_8_half(++in, out, index, pred1, lastsmp);
         in+=2;   _len-=2;   out+=8;

         decode_8_half(in, out, index, pred1, lastsmp);
         in+=2;   _len-=2;   out+=8;

         samples += 16;
      }
   }

   free(preds);

   return samples;
}


int extract_raw_sound(char *sound_dir, char *wav_name, wave_table *wav, float key_base, unsigned char *snd_data, unsigned long sampling_rate)
{
   char wav_file[FILENAME_MAX];
   sprintf(wav_file, "%s/%s.wav", sound_dir, wav_name);
   
   float sampling_rate_float = (float)sampling_rate;

   /*if (!ignoreKeyBase)
   {
      if (
         (alBank->soundBankFormat == STANDARDFORMAT)
         || (alBank->soundBankFormat == STANDARDRNCCOMPRESSED)
         || (alBank->soundBankFormat == STANDARDRNXCOMPRESSED)
         || (alBank->soundBankFormat == BLASTCORPSZLBSTANDARD)
         || (alBank->soundBankFormat == NINDEC)
         )
      {
         samplingRateFloat = samplingRateFloat / CN64AIFCAudio::keyTable[alBank->inst[instrument]->sounds[sound]->key.keybase];
      }
      else if (
            (alBank->soundBankFormat == SUPERMARIO64FORMAT)
            || (alBank->soundBankFormat == MARIOKART64FORMAT) 
            || (alBank->soundBankFormat == ZELDAFORMAT)
            || (alBank->soundBankFormat == STARFOX64FORMAT)
         )
      {
         samplingRateFloat = samplingRateFloat / (((*reinterpret_cast<float*> (&alBank->inst[instrument]->sounds[sound]->unknown3) - 0.0) < 0.00001) ? 1.0f : *reinterpret_cast<float*> (&alBank->inst[instrument]->sounds[sound]->unknown3));
      }
   }*/

   //This algorithm is only for ADPCM WAVE format
   if ((wav == NULL) || (wav->predictor == NULL))
      return 0;
   
   unsigned char *sndData = malloc(wav->sound_length * sizeof(unsigned char));
   for(unsigned int i = 0; i < wav->sound_length; i++) {
      sndData[i] = snd_data[wav->sound_offset+i];
   }
   

   signed short* out_raw_data = malloc(wav->sound_length * 4 * sizeof(signed short));
   int n_samples = decode(sndData, out_raw_data, wav->sound_length, wav->predictor, 0);
   
   unsigned long chunk_size = 0x28 + (n_samples * 2) + 0x44 - 0x8;
   
   unsigned char *wav_data = malloc((chunk_size + 0x8 + 0x4) * sizeof(unsigned char));
   
   wav_data[0x0] = 0x52;
   wav_data[0x1] = 0x49;
   wav_data[0x2] = 0x46;
   wav_data[0x3] = 0x46;
   wav_data[0x4] = ((chunk_size >> 0) & 0xFF);
   wav_data[0x5] = ((chunk_size >> 8) & 0xFF);
   wav_data[0x6] = ((chunk_size >> 16) & 0xFF);
   wav_data[0x7] = ((chunk_size >> 24) & 0xFF);
   wav_data[0x8] = 0x57;
   wav_data[0x9] = 0x41;
   wav_data[0xA] = 0x56;
   wav_data[0xB] = 0x45;
   wav_data[0xC] = 0x66;
   wav_data[0xD] = 0x6D;
   wav_data[0xE] = 0x74;
   wav_data[0xF] = 0x20;
   wav_data[0x10] = 0x10;
   wav_data[0x11] = 0x00;
   wav_data[0x12] = 0x00;
   wav_data[0x13] = 0x00;
   wav_data[0x14] = 0x01;
   wav_data[0x15] = 0x00;
   wav_data[0x16] = 0x01;
   wav_data[0x17] = 0x00;
   wav_data[0x18] = (((unsigned long)sampling_rate_float >> 0) & 0xFF);
   wav_data[0x19] = (((unsigned long)sampling_rate_float >> 8) & 0xFF);
   wav_data[0x1A] = (((unsigned long)sampling_rate_float >> 16) & 0xFF);
   wav_data[0x1B] = (((unsigned long)sampling_rate_float >> 24) & 0xFF);
   wav_data[0x1C] = ((((unsigned long)sampling_rate_float * 2) >> 0) & 0xFF);
   wav_data[0x1D] = ((((unsigned long)sampling_rate_float * 2) >> 8) & 0xFF);
   wav_data[0x1E] = ((((unsigned long)sampling_rate_float * 2) >> 16) & 0xFF);
   wav_data[0x1F] = ((((unsigned long)sampling_rate_float * 2) >> 24) & 0xFF);
   wav_data[0x20] = 0x02;
   wav_data[0x21] = 0x00;
   wav_data[0x22] = 0x10;
   wav_data[0x23] = 0x00;
   wav_data[0x24] = 0x64;
   wav_data[0x25] = 0x61;
   wav_data[0x26] = 0x74;
   wav_data[0x27] = 0x61;

   unsigned long length = (n_samples * 2);
   
   wav_data[0x28] = ((length >> 0) & 0xFF);
   wav_data[0x29] = ((length >> 8) & 0xFF);
   wav_data[0x2A] = ((length >> 16) & 0xFF);
   wav_data[0x2B] = ((length >> 24) & 0xFF);
   
   for (int x = 0; x < n_samples; x++)
   {
      wav_data[0x2C + x * 2] = ((out_raw_data[x] >> 0) & 0xFF);
      wav_data[0x2C + x * 2 + 1] = ((out_raw_data[x] >> 8) & 0xFF);
   }

   unsigned long wavIndex = 0x2C + n_samples * 2;
   
   if (wav->loop->start != 0 || wav->loop->count != 0)
   {
      for (int x = 0; x < 0x44; x++)
         wav_data[wavIndex + x] = 0x00;

      wav_data[wavIndex + 0x0] = 0x73;
      wav_data[wavIndex + 0x1] = 0x6D;
      wav_data[wavIndex + 0x2] = 0x70;
      wav_data[wavIndex + 0x3] = 0x6C;
      wav_data[wavIndex + 0x4] = 0x3C;
      wav_data[wavIndex + 0x5] = 0x00;
      wav_data[wavIndex + 0x6] = 0x00;
      wav_data[wavIndex + 0x7] = 0x00;
      
      //This value only holds true for Mario/Zelda/StarFox formats
      {
         wav_data[wavIndex + 0x14] = sfx_convert_ead_game_value_to_key_base(key_base);
      }
      
      wav_data[wavIndex + 0x15] = 0x00;
      wav_data[wavIndex + 0x16] = 0x00;
      wav_data[wavIndex + 0x17] = 0x00;
      
      wav_data[wavIndex + 0x24] = 0x01;
      wav_data[wavIndex + 0x25] = 0x00;
      wav_data[wavIndex + 0x26] = 0x00;
      wav_data[wavIndex + 0x27] = 0x00;

      if (wav->loop->count > 0)
      {
         wav_data[wavIndex + 0x34] = ((wav->loop->start >> 0) & 0xFF);
         wav_data[wavIndex + 0x35] = ((wav->loop->start >> 8) & 0xFF);
         wav_data[wavIndex + 0x36] = ((wav->loop->start >> 16) & 0xFF);
         wav_data[wavIndex + 0x37] = ((wav->loop->start >> 24) & 0xFF);
         wav_data[wavIndex + 0x38] = (((wav->loop->end) >> 0) & 0xFF);
         wav_data[wavIndex + 0x39] = (((wav->loop->end) >> 8) & 0xFF);
         wav_data[wavIndex + 0x3A] = (((wav->loop->end) >> 16) & 0xFF);
         wav_data[wavIndex + 0x3B] = (((wav->loop->end) >> 24) & 0xFF);

         if (wav->loop->count == 0xFFFFFFFF)
         {
            wav_data[wavIndex + 0x40] = 0x00;
            wav_data[wavIndex + 0x41] = 0x00;
            wav_data[wavIndex + 0x42] = 0x00;
            wav_data[wavIndex + 0x43] = 0x00;
         }
         else
         {
            wav_data[wavIndex + 0x40] = (((wav->loop->count) >> 0) & 0xFF);
            wav_data[wavIndex + 0x41] = (((wav->loop->count) >> 8) & 0xFF);
            wav_data[wavIndex + 0x42] = (((wav->loop->count) >> 16) & 0xFF);
            wav_data[wavIndex + 0x43] = (((wav->loop->count) >> 24) & 0xFF);
         }
      }
   }
   else
   {
      for (int x = 0; x < 0x44; x++)
         wav_data[wavIndex + x] = 0x00;

      wav_data[wavIndex + 0x0] = 0x73;
      wav_data[wavIndex + 0x1] = 0x6D;
      wav_data[wavIndex + 0x2] = 0x70;
      wav_data[wavIndex + 0x3] = 0x6C;
      wav_data[wavIndex + 0x4] = 0x3C;
      wav_data[wavIndex + 0x5] = 0x00;
      wav_data[wavIndex + 0x6] = 0x00;
      wav_data[wavIndex + 0x7] = 0x00;
      
      //This value only holds true for Mario/Zelda/StarFox formats
      {
         wav_data[wavIndex + 0x14] = sfx_convert_ead_game_value_to_key_base(key_base);
      }
      wav_data[wavIndex + 0x15] = 0x00;
      wav_data[wavIndex + 0x16] = 0x00;
      wav_data[wavIndex + 0x17] = 0x00;
   }

   write_file(wav_file, wav_data, (chunk_size + 0x8 + 0x4));
   
   free(wav_data);
   free(out_raw_data);

   return 1;
}

sound_data_header read_sound_data(unsigned char *data, unsigned int data_offset) {
   
   unsigned i, j;
   sound_data_header sound_data;
   
   sound_data.unknown = read_u16_be(&data[data_offset]);
   sound_data.data_count = read_u16_be(&data[data_offset+2]);
   
   if (sound_data.data_count > 0) {
      sound_data.data = malloc(sound_data.data_count * sizeof(*sound_data.data));
      for (i = 0; i < sound_data.data_count; i++) {
         unsigned int sound_data_offset = read_u32_be(&data[data_offset+i*8+4]) + data_offset;
         unsigned int sound_data_length = read_u32_be(&data[data_offset+i*8+8]);

         sound_data.data[i] = malloc(sound_data_length * sizeof(unsigned char));
         for(j = 0; j < sound_data_length; j++) {
            sound_data.data[i][j] = data[sound_data_offset+j];
         }
      }
   }
   
   return sound_data;
}
   
sound_bank_header read_sound_bank(unsigned char *data, unsigned int data_offset) {
   
   unsigned i, j, k;
   sound_bank_header sound_banks;
   
   sound_banks.unknown = read_u16_be(&data[data_offset]);
   sound_banks.bank_count = read_u16_be(&data[data_offset+2]);
   if (sound_banks.bank_count > 0) {
      sound_banks.banks = malloc(sound_banks.bank_count * sizeof(*sound_banks.banks));
      for (i = 0; i < sound_banks.bank_count; i++) {
        unsigned int sound_bank_offset = read_u32_be(&data[data_offset+i*8+4]) + data_offset;
        //unsigned int length = read_u32_be(&data[secCtl->start+i*8+8]);
       
       sound_banks.banks[i].instrument_count = read_u32_be(&data[sound_bank_offset]);
       sound_banks.banks[i].percussion_count = read_u32_be(&data[sound_bank_offset+4]);
       sound_banks.banks[i].unknown_1 = read_u32_be(&data[sound_bank_offset+8]);
       sound_banks.banks[i].unknown_2 = read_u32_be(&data[sound_bank_offset+12]);
       
       //sounds
       if (sound_banks.banks[i].instrument_count > 0) {
          sound_banks.banks[i].sounds = malloc(sound_banks.banks[i].instrument_count * sizeof(*sound_banks.banks[i].sounds));
         for (j = 0; j < sound_banks.banks[i].instrument_count; j++) {
            unsigned int sound_offset = read_u32_be(&data[sound_bank_offset+20+j*4]);
            
            if(sound_offset != 0)
            {
               sound_offset += sound_bank_offset + 16;
              
               sound_banks.banks[i].sounds[j].unknown = read_u32_be(&data[sound_offset]);
               
               //adrs
               unsigned int adrs_offset = read_u32_be(&data[sound_offset+4]);
               if(adrs_offset != 0) {
                  sound_banks.banks[i].sounds[j].adrs = malloc(8 * sizeof(unsigned));
                  for (k = 0; k < 8; k++) {
                     sound_banks.banks[i].sounds[j].adrs[k] = read_u16_be(&data[adrs_offset+sound_bank_offset+16+k*2]);
                  }
               }
               
               //wav_prev
               unsigned int wav_prev_offset = read_u32_be(&data[sound_offset+8]);
               if(wav_prev_offset != 0) {
                  sound_banks.banks[i].sounds[j].wav_prev = read_wave_table(data, wav_prev_offset + sound_bank_offset + 16, sound_bank_offset);
               }
               else {
                  sound_banks.banks[i].sounds[j].wav_prev = NULL;
               }
               sound_banks.banks[i].sounds[j].key_base_prev = read_f32_be(&data[sound_offset+12]);
               
               //wav
               unsigned int wav_offset = read_u32_be(&data[sound_offset+16]);
               if(wav_offset != 0) {
                  sound_banks.banks[i].sounds[j].wav = read_wave_table(data, wav_offset + sound_bank_offset + 16, sound_bank_offset);
               }
               else {
                  sound_banks.banks[i].sounds[j].wav = NULL;
               }
               sound_banks.banks[i].sounds[j].key_base = read_f32_be(&data[sound_offset+20]);
               
               //wav_sec
               unsigned int wav_sec_offset = read_u32_be(&data[sound_offset+24]);
               if(wav_sec_offset != 0) {
                 sound_banks.banks[i].sounds[j].wav_sec = read_wave_table(data, wav_sec_offset + sound_bank_offset + 16, sound_bank_offset);
               }
               else {
                  sound_banks.banks[i].sounds[j].wav_sec = NULL;
               }
               sound_banks.banks[i].sounds[j].key_base_sec = read_f32_be(&data[sound_offset+28]);
            }
            else {
               sound_banks.banks[i].sounds[j].wav_prev = NULL;
               sound_banks.banks[i].sounds[j].wav = NULL;
               sound_banks.banks[i].sounds[j].wav_sec = NULL;
            }
         }
       }
       
       //percussion
       if (sound_banks.banks[i].percussion_count > 0) {
         unsigned int perc_table_offset = read_u32_be(&data[sound_bank_offset+16]) + sound_bank_offset + 16;
         
          sound_banks.banks[i].percussions.items = malloc(sound_banks.banks[i].percussion_count * sizeof(percussion));
         for (j = 0; j < sound_banks.banks[i].percussion_count; j++) {
            unsigned int perc_offset = read_u32_be(&data[perc_table_offset+j*4]);
            
            if(perc_offset != 0)
            {
              perc_offset += sound_bank_offset + 16;
               sound_banks.banks[i].percussions.items[j].unknown_1 = data[perc_offset];
               sound_banks.banks[i].percussions.items[j].pan = data[perc_offset+1];
               sound_banks.banks[i].percussions.items[j].unknown_2 = read_u16_be(&data[perc_offset+2]);
               
               //wav
               unsigned int wav_offset = read_u32_be(&data[perc_offset+4]);
               if(wav_offset != 0) {
                 sound_banks.banks[i].percussions.items[j].wav = read_wave_table(data, wav_offset + sound_bank_offset + 16, sound_bank_offset);
               }
               sound_banks.banks[i].percussions.items[j].key_base = read_f32_be(&data[perc_offset+8]);
               
               //adrs
               unsigned int adrs_offset = read_u32_be(&data[perc_offset+12]);
               if(adrs_offset != 0) {
                  sound_banks.banks[i].percussions.items[j].adrs = malloc(8 * sizeof(unsigned));
                 for (k = 0; k < 8; k++) {
                    sound_banks.banks[i].percussions.items[j].adrs[k] = read_u16_be(&data[adrs_offset+sound_bank_offset+16+k*2]);
                 }
              }
            }
         }
       }
      }
   }
   
   return sound_banks;
}
   
   
