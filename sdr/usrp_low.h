#ifndef _USRP_LOW_H_
#define _USRP_LOW_H_

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned char buf[2 * 1024 * 1024 * 4];
  volatile int start;
  volatile int size;
  pthread_mutex_t m;
  pthread_cond_t c;
} shared_buffer;

void *new_usrp_low(shared_buffer *b);
void usrp_low_set_gain(void *, float gain);
void usrp_low_set_freq(void *, float freq);
void usrp_low_start(void *);
void usrp_low_stop(void *);
void usrp_low_free(void *);

#ifdef __cplusplus
}
#endif

#endif /* _USRP_LOW_H_ */
