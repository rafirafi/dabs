#include "io.h"

#include <stdlib.h>
#include <string.h>
#include <complex.h>

void io_put_back_samples(io *f, float complex *in, int count)
{
  if (f->back_buffer_pos != f->back_buffer_size) {
    /* can't put back more than what was put back before */
    if (count > f->back_buffer_pos) abort();
    f->back_buffer_pos -= count;
    memcpy(&f->back_buffer[f->back_buffer_pos], in, count * sizeof(float complex));
    return;
  }
  free(f->back_buffer);
  f->back_buffer = malloc(count * sizeof(float complex));
  if (f->back_buffer == NULL) abort();
  memcpy(f->back_buffer, in, count * sizeof(float complex));
  f->back_buffer_pos = 0;
  f->back_buffer_size = count;
}

/* to be called by f->get_samples() to deal with back buffer */
int io_get_samples(io *f, float complex *out, int count)
{
  int i;

  i = 0;

  if (f->back_buffer_pos != f->back_buffer_size) {
    while (i < count && f->back_buffer_pos != f->back_buffer_size) {
      if (out != NULL) out[i] = f->back_buffer[f->back_buffer_pos];
      f->back_buffer_pos++;
      i++;
    }

    if (f->back_buffer_pos != f->back_buffer_size)
      return i;

    free(f->back_buffer);
    f->back_buffer = NULL;
    f->back_buffer_pos = 0;
    f->back_buffer_size = 0;
  }

  return i;
}

void io_free(io *f)
{
  free(f->back_buffer);
}

void io_reset(io *f)
{
  free(f->back_buffer);
  f->back_buffer = NULL;
  f->back_buffer_pos = 0;
  f->back_buffer_size = 0;
}
