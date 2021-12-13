#ifndef _ALSA_H_
#define _ALSA_H_

typedef struct {
  void (*receive_audio)(void *, void *, int);
  void (*reset)(void *);
} alsa_out;

alsa_out *new_alsa_out(int no_drop);

#endif /* _ALSA_H_ */
