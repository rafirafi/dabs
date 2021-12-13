#include "viterbi_decode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/log.h"

static int compute_poly(int poly, int shift, int k)
{
  int v = poly & shift;
  int r = 0;
  while (k) {
    r ^= v;
    v >>= 1;
    k--;
  }
  return r & 1;
}

static int distance(int *expected, int *actual, int n)
{
  int ret = 0;
  int i;

  for (i = 0; i < n; i++)
    if (actual[i] != 2)
      ret += expected[i] != actual[i];

  return ret;
}

void viterbi_decode(viterbi_t *v, char *out, unsigned char *m, int len)
{
  int k = v->k;
  int n_poly = v->n_poly;

  int shift = 0;
  int i;
  int s;
  int n_bits = len / n_poly;
  int n_states = 1 << (k-1);
  int error[n_bits+1][n_states];
  unsigned char prev[n_bits+1][n_states];
  unsigned char decoded[n_bits];
  int expected;
  int actual;
  int p;
  int next;
  int next_distance;

  if (n_bits * n_poly != len) { error("bad input\n"); exit(0); }

  memset(error, -1, sizeof(error));

  error[0][0] = 0;

  for (i = 0; i < n_bits; i++) {
    actual = 0;
    for (p = 0; p < n_poly; p++) {
      int c = *m;
      m++;
      if (c == -128) c = 2;
      actual <<= 2;
      actual |= c;
    }

    for (s = 0; s < n_states; s++) {
      if (error[i][s] == -1) continue;

      /* input bit 0 */
      shift = s;
      expected = v->computed[shift];
      next = shift >> 1;
      next_distance = error[i][s] + v->distance[(expected << (n_poly * 2)) | actual];
      if (error[i+1][next] == -1
         || next_distance < error[i+1][next]) {
        error[i+1][next] = next_distance;
        prev[i+1][next] = s;
      }

      /* input bit 1 (not for last k-1 bits, which are 0) */
      if (i >= n_bits - (k - 1)) continue;

      shift = s | (1 << (k -1));
      expected = v->computed[shift];
      next = shift >> 1;
      next_distance = error[i][s] + v->distance[(expected << (n_poly * 2)) | actual];
      if (error[i+1][next] == -1
         || next_distance < error[i+1][next]) {
        error[i+1][next] = next_distance;
        prev[i+1][next] = s;
      }
    }
  }

  s = 0;
  for (i = n_bits; i > 0; i--) {
    decoded[i-1] = s >> (k - 2);
    s = prev[i][s];
  }

  debug("last error %d\n", error[n_bits][0]);

  int out_byte = 0;
  int out_bit = 7;
  for (i = 0; i < n_bits; i++) {
    out[out_byte] |= (decoded[i] << out_bit);
    out_bit--;
    if (out_bit < 0) { out_bit = 7; out_byte++; }
  }
}

void viterbi_init(viterbi_t *v, int k, int *poly, int n_poly)
{
  int i, j;
  int expected, shift, p;

  if (n_poly > 10) exit(1);
  v->k = k;
  memcpy(v->poly, poly, sizeof(int) * n_poly);
  v->n_poly = n_poly;

  v->distance = malloc(1 << (n_poly * 3)); if (v->distance == NULL) abort();
  v->computed = malloc(1 << k); if (v->computed == NULL) abort();

  /* precompute distance */
  for (i = 0; i < 1 << n_poly; i++)
    for (j = 0; j < 1 << (n_poly * 2); j++) {
      int expected[n_poly], actual[n_poly];
      for (k = 0; k < n_poly; k++)
         expected[k] = (i >> k) & 1;
      for (k = 0; k < n_poly; k++) {
        int v = (j >> (2 * k)) & 3;
        actual[k] = v;
      }
      v->distance[(i << (n_poly * 2)) | j] = distance(expected, actual, n_poly);
    }

  /* precomputed polys */
  for (shift = 0; shift < 1 << v->k; shift++) {
    expected = 0;
    for (p = 0; p < n_poly; p++) {
      expected <<= 1;
      expected |= compute_poly(poly[p], shift, v->k);
    }
    v->computed[shift] = expected;
  }
}
