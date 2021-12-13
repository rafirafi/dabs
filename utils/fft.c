#include "fft.h"

#include <complex.h>
#include <fftw3.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  float complex *in;
  float complex *out;
  fftwf_plan p_fft;
  fftwf_plan p_ifft;
  int size;
} _fft;

fft_data *init_fft(int fft_size)
{
  _fft *f;

  f = malloc(sizeof(_fft));

  f->in = fftwf_malloc(sizeof(float complex) * fft_size);
  f->out = fftwf_malloc(sizeof(float complex) * fft_size);
  if (f->in == NULL || f->out == NULL) abort();

  f->p_fft = fftwf_plan_dft_1d(fft_size, f->in, f->out,
                              FFTW_FORWARD, FFTW_ESTIMATE);
  f->p_ifft = fftwf_plan_dft_1d(fft_size, f->in, f->out,
                               FFTW_BACKWARD, FFTW_ESTIMATE);

  f->size = fft_size;

  memset(f->in, 0, sizeof(float complex) * fft_size);
  fftwf_execute(f->p_fft);
  fftwf_execute(f->p_ifft);

  return (fft_data *)f;
}

void fft(fft_data *_f, float complex *in, float complex *out)
{
  _fft *f = _f;
  memcpy(f->in, in, f->size * sizeof(float complex));
  fftwf_execute(f->p_fft);
  memcpy(out, f->out, f->size * sizeof(float complex));
}

void ifft(fft_data *_f, float complex *in, float complex *out)
{
  _fft *f = _f;
  memcpy(f->in, in, f->size * sizeof(float complex));
  fftwf_execute(f->p_ifft);
  memcpy(out, f->out, f->size * sizeof(float complex));
}

void free_fft(fft_data *_f)
{
  _fft *f = _f;
  fftwf_destroy_plan(f->p_fft);
  fftwf_destroy_plan(f->p_ifft);
  fftwf_free(f->in);
  fftwf_free(f->out);
  free(f);
}
