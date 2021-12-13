#ifndef _FREQUENCY_INTERLEAVING_H_
#define _FREQUENCY_INTERLEAVING_H_

#define FK(k) ((k) < 0 ? (k) + 768 : (k) + 767)

typedef struct {
  int n2k[1536];
  int k2n[1536];    /* k in [-768...-1,1...768], use FK(k) to index */
} frequency_interleaver;

frequency_interleaver *new_frequency_interleaver(void);

#endif /* _FREQUENCY_INTERLEAVING_H_ */
