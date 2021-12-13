#include "crc.h"

/* to be optimized */
int crc(char *_data, int len, int poly, int crcsize, int init_value,
        int do_complement)
{
  unsigned char *data = (unsigned char *)_data;
  int i;
  int j;
  int shift = init_value;
  int ret;

  poly >>= 1;

  for (i = 0; i < len; i++) {
    unsigned char d = data[i];
    for (j = 0; j < 8; j++, d<<=1) {
      int b = ((d >> 7) ^ (shift >> (crcsize-1))) & 1;
      shift = ((shift ^ (-b & poly)) << 1) | b;
    }
  }

  ret = shift & ((1 << crcsize) - 1);
  if (do_complement)
    ret = ~ret & ((1 << crcsize) - 1);

  return ret;
}
