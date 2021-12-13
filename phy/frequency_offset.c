#include "frequency_offset.h"

#include <math.h>

#include "signal.h"
#include "utils/log.h"
#include "utils/fft.h"

static float complex table[2048000];

void init_frequency_offset(void)
{
  int i;

  for (i = 0; i < 2048000; i++)
    table[i] = cexpf(I * 2 * M_PI * i / 2048000.);
}

float frequency_offset_fine(float complex *symbol_time)
{
  float complex freq_corr = 0;
  int i;
  for (i = T_U; i < T_S; i++)
    freq_corr += symbol_time[i] * conjf(symbol_time[i - T_U]);
  return cargf(freq_corr)/(2 * M_PI) * 1000;
}

float frequency_offset_coarse_naive(float complex *symbol_freq)
{
  int i,k;
  double e = 0;
  double min_e;
  int min_i;

  /* the data is stored as: positive frequencies then negative frequencies.
   * DAB uses 1536 subcarriers so at the middle of the array there should
   * be 2048-1536 subcarriers with no energy. Let's try some positions around
   * this middle and take the one minimizing energy. (This logic may be
   * changed in the future, it iis hackish. I'm not phy loayer specialist.)
   * Subcarrier 0 is not used. 1..768 are positive. So hole should start at
   * 769. Let's look for the hole starting 50 before to 50 after this.
   */
  for (i = 769 - 50; i < 769 - 50 + 512; i++)
    e += cabsf(symbol_freq[i]);
  min_e = e;
  min_i = i;
  for (i = 769 - 50, k = 0; k < 100; k++, i++) {
    e -= cabsf(symbol_freq[i]);
    e += cabsf(symbol_freq[i + 512]);
    if (e < min_e) {
      min_e = e;
      min_i = i + 1;
    }
  }

  return (min_i - 769) * 1000;
}

float frequency_offset_coarse(float complex *symbol_freq,
                              float complex *z_ref_fft_conj,
                              void *fft_data)
{
  float complex symbol_freq_fft[T_U];
  float complex symbol_freq_ifft[T_U];
  int i;
  float peak;
  int i_peak;

  /* for coarse offset estimation, we correlate the known symbol 1 frequency
   * content with received data.
   * Starting from IQ time domain data it means we do an FFT to go into
   * frequency domain and then we have to switch this signal to the left
   * or to the right to find the best position that matches with known
   * symbol 1 frequency content. This is a correlation. To do the correlation,
   * we do an FFT of 'symbol_freq', multiply with conjugate of z_ref_fft and
   * do an IFFT, then look for the peak that gives us the best position.
   * Works more or less... (maybe all this is total garbage, to be fixed)
   */
  fft(fft_data, symbol_freq, symbol_freq_fft);

  for (i = 0; i < T_U; i++) symbol_freq_fft[i] *= z_ref_fft_conj[i];

  ifft(fft_data, symbol_freq_fft, symbol_freq_ifft);

  peak = cabsf(symbol_freq_ifft[0]);
  i_peak = 0;

  /* dig positive frequencies up to 100KHz */
  for (i = 1; i < 100; i++) {
    float a = cabsf(symbol_freq_ifft[i]);
    if (a > peak) {
      peak = a;
      i_peak = i;
    }
  }
  /* dig negative frequencies down to -100KHz */
  for (i = T_U - 1; i > T_U - 1 - 100; i--) {
    float a = cabsf(symbol_freq_ifft[i]);
    if (a > peak) {
      peak = a;
      i_peak = i - T_U;
    }
  }

  debug("coase offset peak i %d value %f\n", i_peak, peak);

  return i_peak * 1000;

//  return frequency_offset_coarse_naive(symbol_freq);
}

int remove_frequency_offset(int cur_pos,
                            float complex *symbol_time, int freq_offset_hz)
{
//freq_offset_hz = -9820;
  int i;

  for (i = 0; i < T_S; i++) {
    symbol_time[i] *= table[cur_pos];
    cur_pos -= freq_offset_hz;
    if (cur_pos < 0) cur_pos += 2048000;
    if (cur_pos > 2048000) cur_pos -= 2048000;
  }

  return cur_pos;
}
