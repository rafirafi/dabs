#ifndef _SIGNAL_H_
#define _SIGNAL_H_

#include <complex.h>

#define K      1536
#define T_F    196608
#define T_NULL 2656
#define T_S    2552
#define T_U    2048
#define Delta  504

void do_signal(float complex *out, float complex *z);
void compute_phase_ref(float complex *z);

typedef void fft_data;
void compute_phase_ref_full(fft_data *fft_data, float complex *z);

#endif /* _SIGNAL_H_ */
