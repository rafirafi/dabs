#include "rtlsdr_low.h"

#include <stdlib.h>
#include <string.h>
#include <rtl-sdr.h>

#include "utils/log.h"

typedef struct {
  shared_buffer *b;
  volatile int stop;
  volatile int freq_has_changed;
  pthread_t t;
  struct rtlsdr_dev * dev;
} rtlsdr_low;

void *new_rtlsdr_low(shared_buffer *b)
{
  rtlsdr_low *ret;

  ret = (rtlsdr_low *)calloc(1, sizeof(rtlsdr_low));
  if (ret == NULL) { error("rtlsdr out of memory\n"); exit(1); }

  ret->b = b;

  /* assume one rtlsdr device */
  if (rtlsdr_open(&ret->dev, 0) < 0) {
    error("rtlsdr no device?\n"); exit(1);
  }

  if (rtlsdr_set_sample_rate(ret->dev, 2.048e6) < 0) {
    error("rtlsdr failed to set sample rate"); exit(-1);
  }

  /* set AGC mode to enabled */
  if (rtlsdr_set_agc_mode(ret->dev, 1) < 0) {
      error("rtlsdr enabling agc mode failed");  exit(-1);
  }

    return (void *)ret;
}

void rtlsdr_low_set_gain(void *_f, float gain)
{
  rtlsdr_low *f = (rtlsdr_low *)_f;

  /* TODO : allow manual gain */
  int r = rtlsdr_set_tuner_gain_mode(f->dev, 0);
  if (r < 0) {
      error("rtlsdr set automatic gain failed"); exit(1);
  }

  if (pthread_mutex_lock(&f->b->m)) abort();
  f->b->start = 0;
  f->b->size = 0;
  if (pthread_mutex_unlock(&f->b->m)) abort();
}

void rtlsdr_low_set_freq(void *_f, float freq)
{
  rtlsdr_low *f = (rtlsdr_low *)_f;

  int r = rtlsdr_set_center_freq(f->dev, freq);
  if (r < 0) {
      error("rtlsdr set freq failed"); exit(1);
  }

  f->freq_has_changed = 1;
  debug("rx frequency set %g\n", freq);
}

static unsigned char buf[1024*4];
static void *rx(void *_f)
{
  rtlsdr_low *f = (rtlsdr_low *)_f;
  int pos;

  /* reset buffer */
  if (rtlsdr_reset_buffer(f->dev) < 0) {
      error("rtlsdr_reset_buffer failed"); exit(-1);
  }

  while (!f->stop) {
    int n;
    int r = rtlsdr_read_sync(f->dev, &buf[2 * 1024], 2 * 1024, &n);
    if (n != 2 * 1024) error("got %d samples instead of 1024\n", n);
    switch (r) {
    case 0:
      break;
      /* TODO : error code of libusb_bulk_transfer */
    default:
      error("[recv] Unexpected error on RX, Error code: 0x%x\n", r);
      break;
    }

    // convert from 8 bits to 16 bits in place
    for (int i = 2 * 1024, pos = 0; i < 4 * 1024; i++, pos += 2) {
      int val = (buf[i] - 128) / 128. * 32768.;
      buf[pos] = val & 0xFF;
      buf[pos + 1] = (val >> 8) & 0xFF;
    }

    if (0) {
      static FILE *f = NULL;
      if (f==NULL) f = fopen("/tmp/rec.raw", "w");
      if (f==NULL) abort();
      fwrite(buf, 1024*4, 1, f);
    }

    if (pthread_mutex_lock(&f->b->m)) abort();

    if (f->b->size > 2 * 1024 * 1024 * 4 - 1024 * 4) {
      error("shared buffer full, dropping data\n");
      goto drop;
    }

    if (f->freq_has_changed) {
      f->freq_has_changed = 0;
      f->b->start = 0;
      f->b->size = 0;
      goto drop;
    }

    pos = f->b->start + f->b->size;
    pos %= sizeof(f->b->buf);
    memcpy(f->b->buf + pos, buf, 1024*4);
    f->b->size += 1024 * 4;

drop:
    if (pthread_cond_signal(&f->b->c)) abort();
    if (pthread_mutex_unlock(&f->b->m)) abort();
  }

  return NULL;
}

void rtlsdr_low_start(void *_f)
{
  rtlsdr_low *f = (rtlsdr_low *)_f;

  if (pthread_create(&f->t, NULL, rx, f)) { error("pthread_create failed\n"); exit(1); }
}

void rtlsdr_low_stop(void *_f)
{
  rtlsdr_low *f = (rtlsdr_low *)_f;
  void *ret;
  f->stop = 1;
  if (pthread_join(f->t, &ret)) { error("pthread_join failed\n"); exit(1); }
}

void rtlsdr_low_free(void *_f)
{
  rtlsdr_low *f = (rtlsdr_low *)_f;
  if (f->dev) {
      rtlsdr_close(f->dev);
  }
}
