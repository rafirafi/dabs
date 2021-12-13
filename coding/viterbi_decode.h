#ifndef _VITERBI_DECODE_H_
#define _VITERBI_DECODE_H_

typedef struct {
  int k;
  int poly[10];
  int n_poly;
  unsigned char *distance;
  unsigned char *computed;
} viterbi_t;

void viterbi_init(viterbi_t *v, int k, int *poly, int n_poly);
void viterbi_decode(viterbi_t *v, char *out, unsigned char *m, int len);

#endif /* _VITERBI_DECODE_H_ */
