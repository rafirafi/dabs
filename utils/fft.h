#ifndef _FFT_H_
#define _FFT_H_

#include <complex.h>

typedef void fft_data;

fft_data *init_fft(int fft_size);
void fft(fft_data *f, float complex *in, float complex *out);
void ifft(fft_data *f, float complex *in, float complex *out);
void free_fft(fft_data *f);

#endif /* _FFT_H_ */
