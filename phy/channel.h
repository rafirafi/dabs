#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include <complex.h>

void channel_estimate(float complex *z_ref, float complex *z_ref_in, float complex *channel_out);

void channel_compensate(float complex *in, float complex *channel, float complex *out);

#endif /* _CHANNEL_H_ */
