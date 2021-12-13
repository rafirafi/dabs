#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "utils/log.h"

typedef struct {
  io common;
  FILE *f;
  int do_loop;
  unsigned long size;
} file_io;

static int get_samples(io *_f, float complex *out, int count)
{
  file_io *f = (file_io *)_f;
  int a, b, c, d;
  int i;
  int16_t re, im;

  i = io_get_samples(_f, out, count);

  for (; i < count; i++) {
    if (f->do_loop && ftell(f->f) == f->size) fseek(f->f, 0, SEEK_SET);
    a = fgetc(f->f);
    b = fgetc(f->f);
    c = fgetc(f->f);
    d = fgetc(f->f);
    if (a == EOF || b == EOF || c == EOF || d == EOF)
      return i;
    re = a | (b << 8);
    im = c | (d << 8);
    if (out != NULL) out[i] = re / 32768. + im / 32768. * I;
  }

  return i;
}

/* nothing to do for those function */
static void set_gain(io *f, float gain) {}
static void set_freq(io *f, float freq) {}
static void start(io *f)                {}
static void stop(io *f)                 {}

static void file_free(io *_f)
{
  file_io *f = (file_io *)f;
  fclose(f->f);
  io_free(_f);
  free(f);
}

static void file_reset(io *f)
{

}

io *new_input_file(char *filename, int do_loop)
{
  file_io *ret = NULL;

  ret = calloc(1, sizeof(file_io));
  if (ret == NULL) { error("out of memory\n"); goto err; }

  ret->f = fopen(filename, "r");
  if (ret->f == NULL) {
    perror(filename);
    goto err;
  }

  fseek(ret->f, 0, SEEK_END);
  ret->size = ftell(ret->f);
  fseek(ret->f, 0, SEEK_SET);

  ret->do_loop = do_loop;

  ret->common.get_samples      = get_samples;
  ret->common.put_back_samples = io_put_back_samples;
  ret->common.set_gain         = set_gain;
  ret->common.set_freq         = set_freq;
  ret->common.start            = start;
  ret->common.stop             = stop;
  ret->common.free             = file_free;
  ret->common.reset            = file_reset;

  return (io *)ret;

err:
  if (ret != NULL && ret->f != NULL) fclose(ret->f);
  free(ret);
  return NULL;
}
