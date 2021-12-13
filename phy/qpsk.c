#include "qpsk.h"

#include <math.h>

#include "signal.h"

/*
xy:  x = p(l,n)  y = p(l,n+K)

00
     . *
     . .
01
     . .
     . *
10
     * .
     . .
11   . .
     * .
*/

void symbol_unmap(unsigned char *p, float complex *qpsk)
{
  int i;
  /* hard bits for the moment */
  for (i = 0; i < K; i++) {
    float angle = cargf(qpsk[i]);
    p[i] = angle > M_PI/2 || angle < -M_PI/2;
    p[i+K] = angle < 0;
  }
}
