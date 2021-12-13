#ifndef _MSC_H_
#define _MSC_H_

#include "coding/energy_dispersal.h"
#include "fig.h"
#include "audio/super_frame.h"
#include "coding/viterbi_decode.h"

typedef struct {
  int configured;
  int start;
  int l[4];
  int pi[4];
  int bitrate;      /* unit kbps */
  int id;
} sub_channel;

typedef enum {
  AUDIO_STREAM, DATA_STREAM, RESERVED, DATA_PACKET
} service_component_type;

typedef enum {
  DAB, DAB_PLUS, UNKNOWN_AUDIO
} audio_type;

typedef struct {
  int configured;
  service_component_type type;
  union {
    struct {
      audio_type type;
      int subchannel;
      int is_primary;
      int has_control_access;
    } audio_stream;
    struct {
      int type;        /* we don't deal with data stream, so no type */
      int subchannel;
      int is_primary;
      int has_control_access;
    } data_stream;
    struct {
      int scid;
      int is_primary;
      int has_control_access;
    } data_packet;
  } u;
} service_component;

typedef struct {
  int configured;
  int ecc;                   /* -1 if not set, in [0..255] otherwise */
  int country_id;
  int service_reference;
  service_component sc[16];
} service;

typedef struct {
  int configured;
  char name[17];
} service_name;

typedef struct {
  int cif_id;
  unsigned char in[16][55296];
  int in_start_cif_id;
  int in_start;
  int in_size;            /* if < 16 then in is not full yet */
  sub_channel  sc[64];
  service      sr[64];   /* 64 is arbitrary, I don't know the limit */
  service_name sn[64];   /* 64: same value as for 'sr' */
  int service_count;
  energy_dispersal *energy_dispersal;
  super_frame *audio;
  void (*send_audio)(void *, void *, int);
  void *send_audio_data;
  /* set to the current freq/subchannel */
  float frequency;
  int subchannel;
  viterbi_t v;
} msc_config;

msc_config *new_msc_config(void (*send_audio)(void *, void *, int),
                           void *send_audio_data);

void receive_msc_bits(msc_config *m, int cif_id, unsigned char *bits, int cif_symbol);

void msc_update_config(msc_config *c, frame_figs *f);

void msc_set_channel(msc_config *c, float frequency, int subchannel);

#endif /* _MSC_H_ */
