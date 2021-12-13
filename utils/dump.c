#include "dump.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void short_dump(char *filename, float complex *buf, int samples, float scale)
{
  FILE *f;
  int16_t re, im;
  int i;

  f = fopen(filename, "w"); if (f == NULL) { perror(filename); exit(1); }
  for (i = 0; i < samples; i++) {
    re = crealf(buf[i]) * scale;
    im = cimagf(buf[i]) * scale;
    if (fwrite(&re, 1, 2, f) != 2) abort();
    if (fwrite(&im, 1, 2, f) != 2) abort();
  }
  if (fclose(f)) { perror(filename); exit(1); }
}

void text_dump(char *filename, float complex *buf, int samples)
{
  FILE *f;
  int i;

  f = fopen(filename, "w"); if (f == NULL) { perror(filename); exit(1); }
  for (i = 0; i < samples; i++) {
    fprintf(f, "%g %g\n", crealf(buf[i]), cimagf(buf[i]));
  }
  if (fclose(f)) { perror(filename); exit(1); }
}
