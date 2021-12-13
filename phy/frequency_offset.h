#ifndef _FREQUENCY_OFFSET_
#define _FREQUENCY_OFFSET_

#include <complex.h>

void init_frequency_offset(void);

float frequency_offset_fine(float complex *symbol_time);
float frequency_offset_coarse(float complex *symbol_time,
                              float complex *z_ref_fft_conj,
                              void *fft_data);

/* returns cur_pos to use for following samples (start with 0) */
int remove_frequency_offset(int cur_pos,
                            float complex *symbol_time, int freq_offset_hz);

#endif /* _FREQUENCY_OFFSET_ */
