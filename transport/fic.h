#ifndef _FIC_H_
#define _FIC_H_

#include "coding/energy_dispersal.h"
#include "fig.h"
#include "coding/viterbi_decode.h"

typedef struct {
  unsigned char bits[2304];
  unsigned char unpunctured[3072+24];
  int pos;
  energy_dispersal *energy;
  viterbi_t v;
} fic;

fic *new_fic(void);
int receive_fic_bits(frame_figs *figs, fic *fic, unsigned char *bits, int fib_id);

#endif /* _FIC_H_ */
