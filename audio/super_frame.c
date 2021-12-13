#include "super_frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/log.h"
#include "utils/bit_reader.h"
#include "utils/crc.h"
#include "aac.h"

super_frame *new_super_frame(int subchannel_size,
    void (*send_audio)(void *, void *, int), void *send_audio_data)
{
  super_frame *ret = calloc(1, sizeof(super_frame));
  if (ret == NULL) abort();

  ret->subchannel_index = subchannel_size / 8;
  ret->audio_super_frame_size = ret->subchannel_index * 110;

  ret->cif_start = -1;

  ret->data = malloc(ret->audio_super_frame_size + ret->subchannel_index * 10);
  if (ret->data == NULL) abort();

  ret->frame_size = (ret->audio_super_frame_size + ret->subchannel_index * 10) / 5;

  ret->aac_decoder = aac_new_decoder();

  ret->send_audio = send_audio;
  ret->send_audio_data = send_audio_data;

  return ret;
}

void free_super_frame(super_frame *s)
{
  free_aac_decoder(s->aac_decoder);
  free(s->data);
  free(s);
}

/* code based on ETSI TS 102 563 V2.1.1 */
static void audio_decode_super_frame(super_frame *s)
{
  struct {
    int num_aus;
    int au_start;
    int sample_rate;
  } conf[4] = {
    { 4,  8, 32 },
    { 2,  5, 16 },
    { 6, 11, 48 },
    { 3,  6, 24 },
  };

{
  int i;
  debug("superframe[%d]:", s->audio_super_frame_size);
  for (i = 0; i < s->audio_super_frame_size; i++) debug(" %2.2x", s->data[i]);
  debug("\n");
}

  bit_reader b;
  int dac_rate;
  int sbr_flag;
  int aac_channel_mode;
  int ps_flag;
  int mpeg_surround_config;
  int num_aus;
  int n;
  int au_start[6+1];
  int au_size[6];
  int sample_rate;

  init_bit_reader(&b, s->data, s->audio_super_frame_size);

  /* TODO: check/use header_firecode */
  get_bits(&b, 16);                           /* header_firecode */
  get_bits(&b, 1);                            /* rfa */
  dac_rate             = get_bits(&b, 1);
  sbr_flag             = get_bits(&b, 1);
  aac_channel_mode     = get_bits(&b, 1);
  ps_flag              = get_bits(&b, 1);
  mpeg_surround_config = get_bits(&b, 3);

  /* TODO: remove this hack done for no gcc warning */
  (void)ps_flag;
  (void)mpeg_surround_config;

  n = (dac_rate << 1) | sbr_flag;

  num_aus     = conf[n].num_aus;
  au_start[0] = conf[n].au_start;
  sample_rate = conf[n].sample_rate;

  for (n = 1; n < num_aus; n++) {
    au_start[n] = get_bits(&b, 12);
    debug("au start[%d] = %d\n", n, au_start[n]);
    if (au_start[n] <= au_start[n-1] + 2
       || au_start[n] >= s->audio_super_frame_size - 2) {
      debug("bad superframe, skipping\n");
      return;
    }
  }

  au_start[num_aus] = s->audio_super_frame_size;

  for (n = 0; n < num_aus; n++)
    au_size[n] = au_start[n+1] - au_start[n] - 2;

  /* check CRCs */
  for (n = 0; n < num_aus; n++) {
    int received_crc = (s->data[au_start[n] + au_size[n]] << 8) |
                        s->data[au_start[n] + au_size[n] + 1];
    int computed_crc = crc((char *)&s->data[au_start[n]], au_size[n],
                           0x11021, 16, 0xffff, CRC_COMPLEMENT);
    if (received_crc != computed_crc) {
      debug("bad crc (received %2.2x computed %2.2x), skip au %d (size %d)\n",
             received_crc, computed_crc, n, au_size[n]);
      au_size[n] = 0;
    }
  }

  for (n = 0; n < num_aus; n++) {
    debug("au start %d size %d\n", au_start[n], au_size[n]);
  }

  aac_set_config(s->aac_decoder, sample_rate, aac_channel_mode);

  for (n = 0; n < num_aus; n++) {
    int outlen;
    short *out;
    if (au_size[n] == 0)
      continue;
    out = decode_au(s->aac_decoder, s->data + au_start[n], au_size[n], &outlen);
    s->send_audio(s->send_audio_data, out, outlen*2);
#if 0
debug("%p\n", out);
static FILE *f = NULL;
if (f == NULL) f = fopen("/tmp/out.raw", "w");
if (f == NULL) abort();
fwrite(out, outlen, 2, f); fflush(f);
#endif
  }
}

void audio_receive_frame(super_frame *s, int cif_id, 
                         unsigned char *data, int len)
{
  if (len != s->frame_size) {
    error("%s:%d:%s: fatal, bad size\n", __FILE__, __LINE__, __FUNCTION__);
    return;
    exit(1);
  }

  /* check continuity of cif_id */
  if (cif_id != s->next_expected_cif) {
    /* we need to resync */
    s->cif_start = -1;
  }

  s->next_expected_cif = (cif_id + 1) % 5000;

  /* resync in progress? */
  if (s->cif_start == -1) {
    /* TODO: do real sync
     * the data I collected seems to start a superframe at cif % 5 == 0
     */
    if (cif_id % 5)
      return;

    /* sync done */
    s->cif_start = cif_id;
    s->pos = 0;
  }

  memcpy(s->data + s->pos * s->frame_size, data, len);
  s->pos++;

  /* check full super frame received */
  if (s->pos == 5) {
    audio_decode_super_frame(s);
    s->pos = 0;
  }
}
