#include "rtlsdr.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "rtlsdr_low.h"
#include "utils/log.h"

typedef struct {
  io common;
  void *low;
  shared_buffer b;
} rtlsdr_io;

static int get_samples(io *_f, float complex *out, int count)
{
  rtlsdr_io *f = (rtlsdr_io *)_f;
  int i;
  int scount;
  int16_t re, im;
  int pos;

  i = io_get_samples(_f, out, count);
  scount = count - i;

  if (pthread_mutex_lock(&f->b.m)) abort();
  while (f->b.size < scount * 4)
    if (pthread_cond_wait(&f->b.c, &f->b.m)) abort();

  pos = f->b.start;

//printf("read %d count (%d scount) from pos %d start/size %d/%d", count, scount, pos, f->b.start, f->b.size);
  if (out != NULL)
  for (; i < count; i++) {
    re = *(int16_t *)(&f->b.buf[pos]);
    im = *(int16_t *)(&f->b.buf[pos+2]);
    out[i] = re / 32768. + im / 32768. * I;
    pos+=4;
    pos %= sizeof(f->b.buf);
  }

  f->b.size -= scount * 4;
  f->b.start += scount * 4;
  f->b.start %= sizeof(f->b.buf);
//printf(" %d/%d\n", f->b.start, f->b.size);

  if (pthread_mutex_unlock(&f->b.m)) abort();

  return i;
}

static int get_raw_samples(io *_f, char *out, int count)
{
  rtlsdr_io *f = (rtlsdr_io *)_f;
  int pos;

  if (pthread_mutex_lock(&f->b.m)) abort();
  while (f->b.size < count * 4)
    if (pthread_cond_wait(&f->b.c, &f->b.m)) abort();

  pos = f->b.start;
  if (pos + count * 4 > sizeof(f->b.buf)) { error("TODO?\n"); exit(1); }

  if (out != NULL)
    memcpy(out, &f->b.buf[pos], count * 4);

  f->b.size -= count * 4;
  f->b.start += count * 4;
  f->b.start %= sizeof(f->b.buf);

  if (pthread_mutex_unlock(&f->b.m)) abort();

  return count;
}

static void set_gain(io *_f, float gain)
{
  rtlsdr_io *f = (rtlsdr_io *)_f;
  rtlsdr_low_set_gain(f->low, gain);
}

static void set_freq(io *_f, float freq)
{
  rtlsdr_io *f = (rtlsdr_io *)_f;
  rtlsdr_low_set_freq(f->low, freq);
  io_reset(_f);
}

static void start(io *_f)
{
  rtlsdr_io *f = (rtlsdr_io *)_f;
  rtlsdr_low_start(f->low);
}

static void stop(io *_f)
{
  rtlsdr_io *f = (rtlsdr_io *)_f;
  rtlsdr_low_stop(f->low);
}

static void rtlsdr_free(io *_f)
{
  rtlsdr_io *f = (rtlsdr_io *)f;
  rtlsdr_low_free(f->low);
  io_free(_f);
  free(f);
}

io *new_rtlsdr(void)
{
  rtlsdr_io *ret;

  ret = calloc(1, sizeof(rtlsdr_io));
  if (ret == NULL) { error("out of memory\n"); exit(1); }

  pthread_mutex_init(&ret->b.m, NULL);
  pthread_cond_init(&ret->b.c, NULL);

  ret->low = new_rtlsdr_low(&ret->b);

  ret->common.get_samples      = get_samples;
  ret->common.get_raw_samples  = get_raw_samples;
  ret->common.put_back_samples = io_put_back_samples;
  ret->common.set_gain         = set_gain;
  ret->common.set_freq         = set_freq;
  ret->common.start            = start;
  ret->common.stop             = stop;
  ret->common.free             = rtlsdr_free;
  ret->common.reset            = io_reset;

  return (io *)ret;
}
