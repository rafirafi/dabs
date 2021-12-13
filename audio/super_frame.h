#ifndef _SUPER_FRAME_H_
#define _SUPER_FRAME_H_

typedef struct {
  int subchannel_index;
  int audio_super_frame_size;
  unsigned char *data;
  int frame_size;          /* in bytes, for checking purpose */
  int pos;                 /* 0..4 */
  int cif_start;
  int next_expected_cif;
  void *aac_decoder;
  void (*send_audio)(void *send_audio_data, void *buffer, int size);
  void *send_audio_data;
} super_frame;

/* subchannel size is in kbps */
super_frame *new_super_frame(int subchannel_size,
    void (*send_audio)(void *, void *, int), void *send_audio_data);

void free_super_frame(super_frame *);

void audio_receive_frame(super_frame *s, int cif_id,
                         unsigned char *data, int len);

#endif /* _SUPER_FRAME_H_ */
