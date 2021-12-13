#include "bit_reader.h"

#include <stdlib.h>

void init_bit_reader(bit_reader *b, unsigned char *data, int nbytes)
{
  b->data = data;
  b->nbytes = nbytes;
  b->next_byte = 0;
  b->next_bit = 0;
}

int get_bits(bit_reader *b, int count)
{
  int ret = 0;
  int i;

  if (count + b->next_byte * 8 + b->next_bit > b->nbytes * 8) abort();

  for (i = 0; i < count; i++) {
    ret <<= 1;
    ret |= (b->data[b->next_byte] >> (7 - b->next_bit)) & 1;
    b->next_bit++;
    if (b->next_bit == 8) {
      b->next_bit = 0;
      b->next_byte++;
    }
  }

  return ret;
}
