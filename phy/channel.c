#include "channel.h"
#include "signal.h"

void channel_estimate(float complex *z_ref,
                      float complex *z_ref_in, float complex *channel_out)
{
  int i;

  for (i = 0; i < T_U; i++) {
    /* in = h * z_ref
     * in * z_ref* = h * z_ref * z_ref*
     * in * z_ref* = h * (a^2 + b^2)
     * in * z_ref* / cabsf(z_ref)^2 = h
     */
    float abs2 = crealf(z_ref[i]) * crealf(z_ref[i]) + cimagf(z_ref[i]) * cimagf(z_ref[i]);
    if (abs2 < 1e-10)
      channel_out[i] = 0;
    else {
      channel_out[i] = z_ref_in[i] * conjf(z_ref[i]) / abs2;
      channel_out[i] = 1 / channel_out[i];
    }
  }
}

void channel_compensate(float complex *in, float complex *channel, float complex *out)
{
  int i;
  for (i = 0; i < T_U; i++) {
    out[i] = channel[i] * in[i];
  }
}
