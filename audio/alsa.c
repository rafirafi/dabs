#include "alsa.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#include "utils/log.h"

typedef struct {
  void *data;
  int size;
} audio_buf_t;

typedef struct {
  alsa_out common;
  audio_buf_t b[8];
  volatile int b_start;
  volatile int b_size;
  pthread_t t;
  pthread_mutex_t m;
  pthread_cond_t c;
  int no_drop;
} alsa_out_internal;

#define A do { if (err < 0) { error("%s:%d: alsa error '%s'\n", __FILE__, __LINE__, snd_strerror(err)); abort(); } } while (0)

static snd_pcm_t *open_alsa(int samplerate, int channels)
{
  snd_pcm_t *h;
  snd_pcm_hw_params_t *hw_params;
  snd_pcm_sw_params_t *sw_params;
  snd_pcm_uframes_t   period_frames;
  snd_pcm_uframes_t   buffer_frames;
  unsigned int        rate;
  snd_pcm_format_t    format;
  int err;

  format = SND_PCM_FORMAT_S16_LE; //FLOAT_LE;
  err = snd_pcm_open(&h, "default", SND_PCM_STREAM_PLAYBACK, 0); A;
  snd_pcm_hw_params_alloca(&hw_params);
  snd_pcm_sw_params_alloca(&sw_params);
  err = snd_pcm_hw_params_any(h, hw_params); A;
  err = snd_pcm_hw_params_set_access(h, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED); A;
  err = snd_pcm_hw_params_set_format(h, hw_params, format); A;
  err = snd_pcm_hw_params_set_channels(h, hw_params, channels); A;
  rate = samplerate;
  err = snd_pcm_hw_params_set_rate_near(h, hw_params, &rate, 0); A;
  if (rate != samplerate) { error("bad rate\n"); abort(); }
  period_frames = 1024;
  buffer_frames = 1024*20;
  err = snd_pcm_hw_params_set_period_size_near(h,hw_params,&period_frames,0);A;
  err = snd_pcm_hw_params_set_buffer_size_near(h, hw_params, &buffer_frames);A;
  err = snd_pcm_hw_params(h, hw_params); A;

  return h;
}

void xrun(snd_pcm_t *h)
{
  snd_pcm_prepare(h);
}

void suspend(snd_pcm_t *h)
{
  int res;
  int loop = 0;
  while ((res = snd_pcm_resume(h)) == -EAGAIN && loop < 100) {
    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = 10 * 1000;
    select(0, 0, 0, 0, &t);
    loop++;
  }
  if (res < 0) snd_pcm_prepare(h);
}

static void put_alsa(snd_pcm_t *h, unsigned char *bb, int frames)
{
#define X do { warn("%s:%s:%d: alsa error\n", __FILE__, __FUNCTION__, __LINE__); xrun(h); } while (0)
#define Y do { warn("%s:%s:%d: alsa error\n", __FILE__, __FUNCTION__, __LINE__); suspend(h); } while (0)
  ssize_t r;
  size_t count = frames;
  int    channels = 2;

  void filter(short ina, short inb, short *outa, short *outb);
  short *b = (short *)bb;
  for (r = 0; r < frames; r++) {
    short ina = b[2 * r];
    short inb = b[2 * r + 1];
    filter(ina, inb, &b[2*r], &b[2*r+1]);
  }

  while (count) {
    r = snd_pcm_writei(h, bb, count);
    if (r == -EAGAIN || (r >= 0 && (size_t)r < count)) {
      /* nothing */
    }
    else if (r == -EPIPE) { /*usleep(1000);*/ X; }
    else if (r == -ESTRPIPE) Y;
    else if (r < 0) abort();
    if (r > 0) {
      count -= r;
      bb += r * 4 * channels;
    }
  }
#undef X
#undef Y
}

static void *alsa_thread(void *_alsa_out)
{
  alsa_out_internal *alsa_out = _alsa_out;
  snd_pcm_t *h;
  h = open_alsa(48000, 2);

  void init_filter(void);
  init_filter();

  while (1) {
    void *b;
    int size;
    if (pthread_mutex_lock(&alsa_out->m)) abort();
    if (alsa_out->b_size == 0)
      while (alsa_out->b_size == 0)
        if (pthread_cond_wait(&alsa_out->c, &alsa_out->m)) abort();
    b = alsa_out->b[alsa_out->b_start].data;
    size = alsa_out->b[alsa_out->b_start].size;
    alsa_out->b[alsa_out->b_start].data = NULL;
    alsa_out->b_start = (alsa_out->b_start + 1) % 8;
    alsa_out->b_size--;
    if (pthread_mutex_unlock(&alsa_out->m)) abort();
    put_alsa(h, b, size/4);
    free(b);
  }

  return (void *)0;
}

static void receive_audio(void *_alsa_out, void *buffer, int size)
{
  alsa_out_internal *alsa_out = _alsa_out;
  int slot;
  debug("audio data %d\n", size);
again:
  if (pthread_mutex_lock(&alsa_out->m)) abort();
  if (alsa_out->b_size == 8) {
    if (alsa_out->no_drop) {
      warn("alsa buffer full, (%d samples), sleep 50ms\n", size/4);
      if (pthread_mutex_unlock(&alsa_out->m)) abort();
      usleep(50*1000);
      goto again;
    }
    error("alsa buffer full, skip audio (%d samples)\n", size/4);
    goto over;
  }
  slot = (alsa_out->b_start + alsa_out->b_size) % 8;
  alsa_out->b[slot].data = malloc(size);
  if (alsa_out->b[slot].data == NULL) { error("out of memory\n"); exit(1); }
  memcpy(alsa_out->b[slot].data, buffer, size);
  alsa_out->b[slot].size = size;
  alsa_out->b_size++;
over:
  if (pthread_cond_signal(&alsa_out->c)) abort();
  if (pthread_mutex_unlock(&alsa_out->m)) abort();
}

static void reset(void *_alsa_out)
{
  int i;
  alsa_out_internal *alsa_out = _alsa_out;
  if (pthread_mutex_lock(&alsa_out->m)) abort();
  for (i = 0; i < 8; i++) {
    free(alsa_out->b[i].data);
    alsa_out->b[i].data = NULL;
  }
  alsa_out->b_start = 0;
  alsa_out->b_size = 0;
  if (pthread_mutex_unlock(&alsa_out->m)) abort();
}

alsa_out *new_alsa_out(int no_drop)
{
  alsa_out_internal *ret = calloc(1, sizeof(alsa_out_internal));
  if (ret == NULL) exit(1);

  ret->no_drop = no_drop;

  ret->common.receive_audio = receive_audio;
  ret->common.reset         = reset;

  pthread_mutex_init(&ret->m, NULL);
  pthread_cond_init(&ret->c, NULL);

  if (pthread_create(&ret->t, NULL, alsa_thread, ret)) {
    error("pthread_create failed\n");
    exit(1);
  }

  return (alsa_out *)ret;
}
