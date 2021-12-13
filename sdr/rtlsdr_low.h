#ifndef _RTLSDR_LOW_H_
#define _RTLSDR_LOW_H_

#include <pthread.h>

typedef struct {
  unsigned char buf[2 * 1024 * 1024 * 4];
  volatile int start;
  volatile int size;
  pthread_mutex_t m;
  pthread_cond_t c;
} shared_buffer;

void *new_rtlsdr_low(shared_buffer *b);
void rtlsdr_low_set_gain(void *, float gain);
void rtlsdr_low_set_freq(void *, float freq);
void rtlsdr_low_start(void *);
void rtlsdr_low_stop(void *);
void rtlsdr_low_free(void *);

#endif /* _RTLSDR_LOW_H_ */
