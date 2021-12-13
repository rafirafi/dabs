#include "aac.h"

#include <stdio.h>
#include <stdlib.h>
#include <neaacdec.h>

#include "utils/log.h"

typedef struct {
  int sample_rate;
  int mono_stereo;
  NeAACDecHandle h;
} aac;

void *aac_new_decoder(void)
{
  aac *ret = malloc(sizeof(aac));
  if (ret == NULL) abort();

  ret->sample_rate = -1;
  ret->mono_stereo = -1;
  ret->h = NeAACDecOpen();

  return ret;
}

void free_aac_decoder(void *_a)
{
  aac *a = _a;
  NeAACDecClose(a->h);
  free(a);
}

void aac_set_config(void *_a, int sample_rate, int mono_stereo)
{
  aac *a = _a;
  unsigned char cf[2];
  int sample_rate_index;
  int channel_config;
  unsigned long sample_rate_ret;
  unsigned char channels_ret;
  int ret;

  if (a->sample_rate == sample_rate || a->mono_stereo == mono_stereo)
    return;

  a->sample_rate = sample_rate;
  a->mono_stereo = mono_stereo;

  NeAACDecClose(a->h);
  a->h = NeAACDecOpen();

  switch (sample_rate) {
  default: abort();
  case 16: sample_rate_index = 8; break;
  case 24: sample_rate_index = 6; break;
  case 32: sample_rate_index = 5; break;
  case 48: sample_rate_index = 3; break;
  }

  channel_config = mono_stereo + 1;

  /* 16 bits of cf organized as:
   *  00010 = AudioObjectType 2 (AAC LC)
   *  xxxx  = (core) sample rate index
   *  xxxx  = (core) channel config
   *  100   = GASpecificConfig with 960 transform
   */
  cf[0] = (0x02 << 3) | (sample_rate_index >> 1);
  cf[1] = ((sample_rate_index & 1) << 7) | (channel_config << 3) | 0x4;

  ret = NeAACDecInit2(a->h, cf, 2, &sample_rate_ret, &channels_ret);
  if (ret) {
    error("NeAACDecInit2 failed\n");
    exit(1);
  }

  if (sample_rate_ret != 48000 || channels_ret != 2) {
    error("unhandled sample rate/channels (%ld/%d)\n",
          sample_rate_ret, channels_ret);
    //exit(1);
  }
}

void *decode_au(void *_a, unsigned char *data, int len, int *retlen)
{
  aac *a = _a;
  void *ret;
  NeAACDecFrameInfo hinfo;
  ret = NeAACDecDecode(a->h, &hinfo, data, len);
  *retlen = hinfo.samples;
  return ret;
}
