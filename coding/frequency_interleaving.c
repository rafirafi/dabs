#include "frequency_interleaving.h"

#include <stdlib.h>

frequency_interleaver *new_frequency_interleaver(void)
{
  frequency_interleaver *ret;
  int i;
  int n;
  int A[2048];

  ret = malloc(sizeof(frequency_interleaver));
  if (ret == NULL) abort();

  A[0] = 0;
  for (i = 1; i < 2048; i++)
    A[i] = (13 * A[i-1] + 511) & 2047;

  n = 0;
  for (i = 0; i < 2048; i++)
    if (A[i] >= 256 && A[i] <= 1792 && A[i] != 1024) {
      ret->n2k[n] = A[i] - 1024;
      n++;
    }

  for (i = 0; i < 1536; i++)
    ret->k2n[FK(ret->n2k[i])] = i;

  return ret;
}
