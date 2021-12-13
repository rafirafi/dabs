#ifndef _IO_H_
#define _IO_H_

#include <complex.h>

typedef struct io {
  int (*get_samples)(struct io *f, float complex *out, int count);
  int (*get_raw_samples)(struct io *f, char *out, int count);
  void (*put_back_samples)(struct io *f, float complex *in, int count);
  void (*set_gain)(struct io *f, float gain);
  void (*set_freq)(struct io *f, float freq);
  void (*start)(struct io *f);
  void (*stop)(struct io *f);
  void (*free)(struct io *f);
  void (*reset)(struct io *f);

  float complex *back_buffer;
  int back_buffer_size;
  int back_buffer_pos;
} io;

void io_put_back_samples(io *f, float complex *in, int count);
int io_get_samples(io *f, float complex *out, int count);
void io_free(io *f);
void io_reset(io *f);

#endif /* _IO_H_ */
