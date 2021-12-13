#include "energy_dispersal.h"

#include <stdlib.h>

int compute_poly(int *shift)
{
  int ret = (((*shift) >> 4) ^ (*shift)) & 1;

  *shift = ((*shift) >> 1) | (ret << 8);

  return ret;
}

energy_dispersal *new_energy_dispersal(int size)
{
  energy_dispersal *ret;
  int i;

  ret = calloc(1, sizeof(energy_dispersal));
  if (ret == NULL) abort();

  ret->v = calloc(1, size);
  if (ret->v == NULL) abort();

  ret->size = size;

  int out_bit = 7;
  int out_byte = 0;
  int shift = 0x1ff;

  for (i = 0; i < size * 8; i++) {
    ret->v[out_byte] |= compute_poly(&shift) << out_bit;
    out_bit--;
    if (out_bit == -1) {
      out_bit = 7;
      out_byte++;
    }
  }

  return ret;
}

void do_energy_dispersal(energy_dispersal *e, char *in_out)
{
  int i;

  for (i = 0; i < e->size; i++)
    in_out[i] ^= e->v[i];
}

void do_energy_dispersal_n(energy_dispersal *e, char *in_out, int n)
{
  int i;

  for (i = 0; i < n; i++)
    in_out[i] ^= e->v[i];
}
