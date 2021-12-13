#ifndef _TIME_INTERLEAVING_H_
#define _TIME_INTERLEAVING_H_

extern int ti_table[16];

#define TIME_DEINTERLEAVE(r, i) (((r) - ti_table[(i) % 16] + 16) %  16)

#endif /* _TIME_INTERLEAVING_H_ */
