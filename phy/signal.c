#include "signal.h"

#include <stdio.h>
#include <math.h>
#include <complex.h>

#include "utils/fft.h"

void do_signal(float complex *out, float complex *z)
{
  int k;
  int t;

  for (t = 0; t < T_S; t++) {
    for (k = -K/2; k <= K/2; k++) {
      out[t] += z[k + K/2] * cexpf(2 * I * M_PI * k * ((float)t - Delta) / T_U);
//if (isnanf(crealf(z[k + K/2])) || isnanf(cimagf(z[k + K/2]))) abort();
//if (isnanf(crealf(out[t])) || isnanf(cimagf(out[t]))) { printf("t %d k %d\n", t, k); }
    }
  }
}

static struct {
  int min;
  int max;
  int k_prime;
  int i;
  int n;
} table23[] = {
  {-768, -737, -768, 0, 1},
  {-736, -705, -736, 1, 2},
  {-704, -673, -704, 2, 0},
  {-672, -641, -672, 3, 1},
  {-640, -609, -640, 0, 3},
  {-608, -577, -608, 1, 2},
  {-576, -545, -576, 2, 2},
  {-544, -513, -544, 3, 3},
  {-512, -481, -512, 0, 2},
  {-480, -449, -480, 1, 1},
  {-448, -417, -448, 2, 2},
  {-416, -385, -416, 3, 3},
  {-384, -353, -384, 0, 1},
  {-352, -321, -352, 1, 2},
  {-320, -289, -320, 2, 3},
  {-288, -257, -288, 3, 3},
  {-256, -225, -256, 0, 2},
  {-224, -193, -224, 1, 2},
  {-192, -161, -192, 2, 2},
  {-160, -129, -160, 3, 1},
  {-128,  -97, -128, 0, 1},
  { -96,  -65,  -96, 1, 3},
  { -64,  -33,  -64, 2, 1},
  { -32,   -1,  -32, 3, 2},
  { 1,   32,   1, 0, 3},
  { 33,  64,  33, 3, 1},
  { 65,  96,  65, 2, 1},
  {97,   128, 97, 1, 1},
  {129, 160, 129, 0, 2},
  {161, 192, 161, 3, 2},
  {193, 224, 193, 2, 1},
  {225, 256, 225, 1, 0},
  {257, 288, 257, 0, 2},
  {289, 320, 289, 3, 2},
  {321, 352, 321, 2, 3},
  {353, 384, 353, 1, 3},
  {385, 416, 385, 0, 0},
  {417, 448, 417, 3, 2},
  {449, 480, 449, 2, 1},
  {481, 512, 481, 1, 3},
  {513, 544, 513, 0, 3},
  {545, 576, 545, 3, 3},
  {577, 608, 577, 2, 3},
  {609, 640, 609, 1, 0},
  {641, 672, 641, 0, 3},
  {673, 704, 673, 3, 0},
  {705, 736, 705, 2, 1},
  {737, 768, 737, 1, 1},
};

static int table24[4][32] = {
{0,2,0,0,0,0,1,1,2,0,0,0,2,2,1,1,0,2,0,0,0,0,1,1,2,0,0,0,2,2,1,1},
{0,3,2,3,0,1,3,0,2,1,2,3,2,3,3,0,0,3,2,3,0,1,3,0,2,1,2,3,2,3,3,0},
{0,0,0,2,0,2,1,3,2,2,0,2,2,0,1,3,0,0,0,2,0,2,1,3,2,2,0,2,2,0,1,3},
{0,1,2,1,0,3,3,2,2,3,2,1,2,1,3,2,0,1,2,1,0,3,3,2,2,3,2,1,2,1,3,2},
};

void compute_phase_ref(float complex *z)
{
  int k;
  int cur = 0;
  for (k = -K/2; k <= K/2; k++) {
    int k_prime;
    int h;
    int n;
    int i;
    if (k == 0) continue;
    while (!(k >= table23[cur].min && k <= table23[cur].max)) cur++;
    k_prime = table23[cur].k_prime;
    i       = table23[cur].i;
    n       = table23[cur].n;
    h       = table24[i][k-k_prime];
    float phi = M_PI/2 * (h + n);
    z[k + K/2] = cexpf(I * phi);
//printf("z[%d] = %g %g %g (%g)\n", k+K/2, crealf(z[k + K/2]), cimagf(z[k + K/2]), phi, cabsf(z[k+K/2]));
  }
}

void compute_phase_ref_full(fft_data *fft_data, float complex *z)
{
  float complex z_ref[T_U];
  float complex z_ref_time[T_S];
  int i;

  for (i = 0; i < T_S; i++) z_ref_time[i] = 0;

  compute_phase_ref(z_ref);
  do_signal(z_ref_time, z_ref);
  for (i = 0; i < T_S; i++) z_ref_time[i] /= 2048;
  fft(fft_data, &z_ref_time[Delta], z);
//for (i = 0; i < T_U; i++) printf("%g %g\n", crealf(z[i]), cimagf(z[i]));
//exit(1);
}
